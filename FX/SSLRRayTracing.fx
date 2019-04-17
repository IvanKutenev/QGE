 struct VertexIn
 {
	float3 PosL : POSITION;
	float2 Tex : TEXCOORD;
 };

 struct VertexOut
 {
	float4 PosH : SV_POSITION;
	float2 Tex : TEXCOORD;
 };

 SamplerState samPointClamp
 {
	Filter = MIN_MAG_MIP_POINT;

	AddressU = CLAMP;
	AddressV = CLAMP;
 };

 Texture2D gHiZBuffer;

 ////For testing////////////
 Texture2D gNormalDepthMap;
 Texture2D gWSPositionMap;
 ///////////////////////////

 cbuffer cbPerFrame
 {
	float4x4 gView;
	float4x4 gProj;

	float gClientWidth;
	float3 Pad1;
	float gClientHeight;
	float3 Pad2;

	int MIPS_COUNT;
	bool gEnableTraceIntervalRestrictions;
 }

static const float HIZ_START_MIP_NUM = 0.0f; // always start <= HIZ_STOP_LEVEL
static const float HIZ_STOP_MIP_NUM = 0.0f; // set higher to use less iterations - quality vs performance tradeoff
static const float HIZ_MAX_MIP_NUM = 2.0f/*max(float(MIPS_COUNT) - 1, 2.0f)*/;
static const int EPSILON_OFFSET_ADJUST = 2.0f;
static const float2 HIZ_TEX_COORD_OFFSET_EPSILON = float2(1 / gClientWidth, 1 / gClientHeight) / EPSILON_OFFSET_ADJUST; // just needs to make sure in the next pixel, not skipping them entirely
static const uint MAX_ITERATIONS = EPSILON_OFFSET_ADJUST * max(gClientWidth, gClientHeight) / MIPS_COUNT; // a good balance of quality vs performance - most common cases will not use anywhere near this full value

static const float2 hiZSize = float2(gClientWidth, gClientHeight); // not sure if correct - this is mip level 0 size

float3 IntersectDepthPlane(float3 o, float3 d, float t)
{
	return o + d * t;
}

float2 GetCellIndex(float2 ray, float2 mipSz)
{
	return floor(ray * mipSz);
}

float3 IntersectCellBoundary(float3 o, float3 d, float2 cellIndex, float2 mipSz, float2 crossStep, float2 crossOffset)
{
	float2 Index = cellIndex + crossStep;
	Index /= mipSz;
	Index += crossOffset;
	float2 delta = Index - o.xy;
	delta /= d.xy;
	float t = min(delta.x, delta.y);

	return IntersectDepthPlane(o, d, t);
}

float GetMaximumHiZValue(float2 ray, float level, float rootLevel)
{
	// not sure why we need rootLevel for this
	return gHiZBuffer.SampleLevel(samPointClamp, ray.xy, min(level, rootLevel)).y;
}

float GetMinimumHiZValue(float2 ray, float level, float rootLevel)
{
	// not sure why we need rootLevel for this
	return gHiZBuffer.SampleLevel(samPointClamp, ray.xy, min(level, rootLevel)).x;
}

float2 GetMipLevelSz(float level, float rootLevel)
{
	// not sure why we need rootLevel for this
	float2 div = (level == 0.0f ? 1.0f : exp2(min(level, rootLevel)));
	return float2(gClientWidth, gClientHeight) / div;
}

bool CrossedCellBoundary(float2 cellIdxOne, float2 cellIdxTwo)
{
	return cellIdxOne.x != cellIdxTwo.x || cellIdxOne.y != cellIdxTwo.y;
}

float3 hiZTrace(float3 PosSS, float3 RefelectVectorSS, float3 RefelectVectorVS)
{
	const float SmallestMipNum = float(MIPS_COUNT) - 1.0f; // convert to 0-based indexing
	
	float CurMipNum = HIZ_START_MIP_NUM;

	uint iterations = 0u;

	// get the cell cross direction and a small offset to enter the next cell when doing cell crossing
	float2 Offset = float2(RefelectVectorSS.x >= 0.0f ? 1.0f : -1.0f, RefelectVectorSS.y >= 0.0f ? 1.0f : -1.0f);
	float2 EpsilonOffset = float2(Offset.xy * HIZ_TEX_COORD_OFFSET_EPSILON.xy);
	Offset.xy = saturate(Offset.xy);

	// set current ray to original screen coordinate and depth
	float3 ray = PosSS.xyz;

	// scale vector such that z is 1.0f (maximum depth)
	float3 d = RefelectVectorSS.xyz / RefelectVectorSS.z;

	// set starting point to the point where z equals 0.0f (minimum depth)
	float3 o = IntersectDepthPlane(PosSS, d, -PosSS.z);

	// cross to next cell to avoid immediate self-intersection
	float2 rayCell = GetCellIndex(ray.xy, hiZSize.xy);
	ray = IntersectCellBoundary(o, d, rayCell.xy, hiZSize.xy, 2.0f * Offset.xy, EpsilonOffset.xy);

	while(CurMipNum >= HIZ_STOP_MIP_NUM && iterations < MAX_ITERATIONS)
	{
		// get the minimum depth plane in which the current ray resides
		float minZ = GetMinimumHiZValue(ray.xy, CurMipNum, SmallestMipNum);
		float maxZ = GetMaximumHiZValue(ray.xy, CurMipNum, SmallestMipNum);

		// get the cell number of the current ray
		const float2 mipSz = GetMipLevelSz(CurMipNum, SmallestMipNum);
		const float2 oldCellIdx = GetCellIndex(ray.xy, mipSz);

		// intersect only if ray depth is below the minimum depth plane
		float3 tmpRay;
		if (gEnableTraceIntervalRestrictions)
			tmpRay = IntersectDepthPlane(o, d, min(max(ray.z, minZ), maxZ + 0.002f * pow((1 - ray.z), 0.575f)));
		else
			tmpRay = IntersectDepthPlane(o, d, max(ray.z, minZ));

		// get the new cell number as well
		const float2 newCellIdx = GetCellIndex(tmpRay.xy, mipSz);

		// if the new cell number is different from the old cell number, a cell was crossed
		if(CrossedCellBoundary(oldCellIdx, newCellIdx))
		{
			// intersect the boundary of that cell instead, and go up a level for taking a larger step next iteration
			tmpRay = IntersectCellBoundary(o, d, oldCellIdx, mipSz, Offset.xy, EpsilonOffset.xy); //// NOTE added .xy to o and d arguments
			CurMipNum = min(HIZ_MAX_MIP_NUM, CurMipNum + 2.0f);
		}

		ray.xyz = tmpRay.xyz;

		// go down a level in the hi-z buffer
		--CurMipNum;

		++iterations;
	}
	if(CurMipNum < HIZ_STOP_MIP_NUM && iterations > 0)
		return ray;
	else
		return float3(-10,-10,-10);
}

float4 GetReflectionPosition(VertexOut pin)
{
	// PSS
	float3 PosSS = float3(pin.Tex, gHiZBuffer.SampleLevel(samPointClamp, pin.Tex, 0.0f).x);
	if(PosSS.z == 1.0f)
		return float4(0.0f,0.0f,0.0f,1.0f);
	// PVS
	float3 PosVS = mul(float4(gWSPositionMap.SampleLevel(samPointClamp, pin.Tex, 0.0f).xyz, 1.0f), gView).xyz;

	// V>VS - since calculations are in view-space, we can just normalize the position to point at it
	float3 PosVS_U = normalize(PosVS);
	// N>VS
	float3 NormalVS = gNormalDepthMap.SampleLevel(samPointClamp, pin.Tex, 0.0f).xyz;
	if(dot(NormalVS, float3(1.0f, 1.0f, 1.0f)) == 0.0f)
	{
		return float4(-10.0f, -10.0f, -10.0f, 1.0f);
	}
	
	float3 ReflectionVectorVS = reflect(PosVS_U, NormalVS);
	float3 ReflectPosH = mul(float4(PosVS + ReflectionVectorVS, 1.0f), gProj).xyz/mul(float4(PosVS + ReflectionVectorVS, 1.0f), gProj).w;
	ReflectPosH.x = ReflectPosH.x * 0.5f + 0.5f;
	ReflectPosH.y = ReflectPosH.y * -0.5f + 0.5f;

	// V>SS - screen space reflection vector
	float3 ReflectionVectorSS = ReflectPosH - PosSS;

	// calculate the ray
	float3 IntersectPosSS = hiZTrace(PosSS, ReflectionVectorSS, ReflectionVectorVS);
	if (IntersectPosSS.x > 1.0f || IntersectPosSS.y > 1.0f ||
		IntersectPosSS.x < 0.0f || IntersectPosSS.y < 0.0f ||
		IntersectPosSS.z >= 1.0f)
		return float4(-10.0f, -10.0f, -10.0f, 1.0f);
	return float4(IntersectPosSS, 1.0f);
}

 VertexOut VS(VertexIn vin)
 {
	VertexOut vout;
	vout.PosH = float4(vin.PosL, 1.0f);
	vout.Tex = vin.Tex;
	return vout;
 }

 float4 PS(VertexOut pin) : SV_TARGET
 {
	return GetReflectionPosition(pin);	
 }

technique11 SSLRRayTraceTech
 {
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS() ) );
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader( CompileShader( ps_5_0, PS() ) );
	}
 }