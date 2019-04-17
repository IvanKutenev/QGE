#include "Definitions.fx"
#include "GIDefinitions.fx"
#include "GPUCollisionDefinitions.fx"
#include "SVODefinitions.fx"

////////////////////////////Shaders////////////////////////////////////////////////////////////////

ShaderOut VS(VertexIn vin, uint InstanceID : SV_InstanceID)
{
	ShaderOut vout;

	vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld[InstanceID]).xyz;
	vout.NormalW = mul(float4(vin.NormalL, 1.0f), gWorldInvTranspose[InstanceID]).xyz;
	vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj[InstanceID]);
	vout.PosSS = mul(vout.PosH, toTexSpace);
	vout.Tex = mul(float4(vin.Tex, 0.0f, 1.0f), gTexTransform[0]).xy;
	vout.TangentW = ComputeTangent(vout.NormalW);
	vout.InstanceID = InstanceID;
	return vout;
}

PatchTess ConstantHS(InputPatch<ShaderOut, 3>patch, uint patchID : SV_PrimitiveID)
{
	PatchTess hsOut = (PatchTess)0;

	float3 triangleVP[3];
	triangleVP[0] = patch[0].PosW;
	triangleVP[1] = patch[1].PosW;
	triangleVP[2] = patch[2].PosW;

	float3 triangleN[3];
	triangleN[0] = patch[0].NormalW;
	triangleN[1] = patch[1].NormalW;
	triangleN[2] = patch[2].NormalW;

	hsOut.controlP = calculateControlPoints(triangleVP, triangleN);

	float tess = 1.0f;

	if (gEnableDynamicLOD == true){
		float d = distance(gEyePosW, hsOut.controlP.B111);

		const float d0 = gMaxLODdist;
		const float d1 = gMinLODdist;
		tess = 64.0f*saturate((d1 - d) / (d1 - d0));

		if (tess < 1.0f)
			tess = 1.0f;

		if (tess > gMaxTessFactor)
			tess = gMaxTessFactor;
	}
	else
		tess = gConstantLODTessFactor;
	hsOut.EdgeTess[0] = tess;
	hsOut.EdgeTess[1] = tess;
	hsOut.EdgeTess[2] = tess;

	hsOut.InsideTess = tess;

	return hsOut;
}

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
ShaderOut HS(InputPatch<ShaderOut, 3> p,
	uint i : SV_OutputControlPointID,
	uint patchId : SV_PrimitiveID)
{
	ShaderOut hout;

	hout.PosW    = p[i].PosW;
	hout.NormalW = p[i].NormalW;
	hout.PosH    = float4(1.0f, 1.0f, 1.0f, 1.0f);
	hout.PosSS = float4(1.0f, 1.0f, 1.0f, 1.0f);
	hout.Tex     = p[i].Tex;
	hout.TangentW = p[i].TangentW;
	hout.InstanceID = p[i].InstanceID;
	return hout;
}

[domain("tri")]
ShaderOut DS(PatchTess patchTess,
	float3 bary : SV_DomainLocation,
	const OutputPatch<ShaderOut, 3>bezPatch)
{
	ShaderOut dsOut = (ShaderOut)0;

	float4 triangleVP[3];
	triangleVP[0] = float4(bezPatch[0].PosW, 1.0f);
	triangleVP[1] = float4(bezPatch[1].PosW, 1.0f);
	triangleVP[2] = float4(bezPatch[2].PosW, 1.0f);
	float3 triangleN[3];
	triangleN[0] = bezPatch[0].NormalW;
	triangleN[1] = bezPatch[1].NormalW;
	triangleN[2] = bezPatch[2].NormalW;
	float3 position = calculatePositionfromUVW(triangleVP, bary,
		patchTess.controlP);
	
	float3 normal = calculateNormalfromUVW(triangleN, bary,
		patchTess.controlP);
	
	dsOut.Tex = interpolateTexC(bezPatch[0].Tex, bezPatch[1].Tex, bezPatch[2].Tex,
		bary);
	
	dsOut.NormalW = normal;
	
	dsOut.PosW = position;
	
	dsOut.TangentW = ComputeTangent(dsOut.NormalW);

	if (gUseDisplacementMapping)
	{
		const float MipInterval = 20.0f;
		float mipLevel = clamp((distance(dsOut.PosW, gEyePosW) - MipInterval) / MipInterval, 0.0f, 6.0f);

		float h = gNormalMap.SampleLevel(samLinear, dsOut.Tex, 0).a;

		dsOut.NormalW = normalize(dsOut.NormalW);

		dsOut.PosW += (gHeightScale * (h - 1.0))*dsOut.NormalW;
	}

	dsOut.PosH = mul(float4(dsOut.PosW, 1.0f), gViewProj);
	dsOut.PosSS = mul(dsOut.PosH, toTexSpace);
	dsOut.InstanceID = bezPatch[0].InstanceID;
	return dsOut;
}

[maxvertexcount(3)]
void ComputeNormalGS(triangle ShaderOut gin[3], inout TriangleStream<ShaderOut>triStream)
{
	float3 n;
	ShaderOut gout[3];

	gout[0] = gin[0];
	gout[1] = gin[1];
	gout[2] = gin[2];

	n = ComputeNormal(gout[0].PosW, gout[1].PosW, gout[2].PosW);
	n = normalize(n);
	
	gout[0].NormalW = n;
	gout[1].NormalW = n;
	gout[2].NormalW = n;

	triStream.Append(gout[0]);
	triStream.Append(gout[1]);
	triStream.Append(gout[2]);
}

[maxvertexcount(3 * GI_CASCADES_COUNT)]
void GenerateGridGS(triangle ShaderOut gin[3], inout TriangleStream<GenGridGSOut>outputStream)
{
	float3 polygonNormal = normalize(gin[0].NormalW + gin[1].NormalW + gin[2].NormalW);
	
	[unroll(GI_CASCADES_COUNT)]
	for(int j = 0; j < GI_CASCADES_COUNT; ++j)
	{
		if(gBoundingSphereRadius < gMeshCullingFactor / gInvertedGridCellSizes[j].x)
			continue;

		int viewIdx = GetViewIndex(polygonNormal, j);

		GenGridGSOut gout[3];

		[unroll(3)]
		for(int i = 0; i < 3; ++i)
		{
			gout[i].PosH = mul(gGridViewProj[viewIdx], float4(gin[i].PosW, 1.0f));
			gout[i].PosW = gin[i].PosW.xyz;
			gout[i].Tex = gin[i].Tex;
			gout[i].NormalW = gin[i].NormalW;
			gout[i].PosSS = gin[i].PosSS;
			gout[i].TangentW = gin[i].TangentW;
			gout[i].CascadeIndex = j;
		}

		float2 side0N = normalize(gout[1].PosH.xy - gout[0].PosH.xy);		
		float2 side1N = normalize(gout[2].PosH.xy - gout[1].PosH.xy);
		float2 side2N = normalize(gout[0].PosH.xy - gout[2].PosH.xy);
		gout[0].PosH.xy += normalize(-side0N + side2N) * texelSize;
		gout[1].PosH.xy += normalize(side0N - side1N) * texelSize;
		gout[2].PosH.xy += normalize(side1N - side2N) * texelSize;

		[unroll(3)]
		for(int i = 0; i < 3; ++i)
			outputStream.Append(gout[i]);
		outputStream.RestartStrip();
	}
}

float4 DrawScreenQuadPS(ShaderOut pin) : SV_Target
{
	return gDiffuseMapArray.Sample(samAnisotropic, float3(pin.Tex, 0.0f));
}

cbuffer cbGrid
{
	int gBodiesCount;
	float4 gBodiesGridCenterPosW[PARTICLES_GRID_CASCADES_COUNT];
	float4 gBodiesGridCellSizes[PARTICLES_GRID_CASCADES_COUNT];
};
StructuredBuffer<MicroParams>gPreIterationParticlesGridRO;

RWStructuredBuffer<Voxel>gGridBuffer;
StructuredBuffer<VPL>gSHBuffer;
StructuredBuffer<Voxel>gGridBufferRO;
float4 PS(ShaderOut pin) : SV_Target
{
	//
	//Texturing
	//
	float4 texColor = CalculateTexColor(pin.Tex);
	//
	//Some fast computation
	//
	pin.PosSS /= pin.PosSS.w;

	if (pin.PosW.y <= gWaterHeight - WORLD_POSITION_CLIP_TRESHOLD && gDrawWaterReflection)
		clip(-1);

	if (pin.PosW.y >= gWaterHeight + WORLD_POSITION_CLIP_TRESHOLD && gDrawWaterRefraction)
		clip(-1);

	pin.NormalW = normalize(pin.NormalW);
	//
	//Normal/Displacement mapping
	//
	if (gUseNormalMapping || gUseDisplacementMapping)
	{
		float3 normalMapSample = gNormalMap.Sample(samLinear, pin.Tex).rgb;
		pin.NormalW = NormalSampleToWorldSpace(normalMapSample, pin.NormalW, pin.TangentW);
	}
	//
	//Material mapping
	//
	Material mMaterial = gMaterial;
	if(gMaterialType == MTL_TYPE_REFLECT_MAP)
	{
		mMaterial.Reflect.x = 1.0f;
		mMaterial.Reflect.y = gReflectMap.Sample(samAnisotropic, pin.Tex);
		mMaterial.Reflect.z = -1.0f;
	}
	//
	//Lighting
	//
	float PixelDepthV = CalculateViewSpaceDepth(pin.PosW);
	float4 toEyeW = CalculateToEyeW(pin.PosW);
	float4x4 ADS = CalculateLitColor(mMaterial, pin.NormalW, pin.PosW, toEyeW, PixelDepthV);
	//Indirect lighting
	for (int i = 0; i < GI_CASCADES_COUNT && gEnableGI; ++i)
	{
		float3 samplePos;
		int3 voxelPos;
		CalculateGridPositions(pin.PosW, i, samplePos, voxelPos);

		if ((voxelPos.x > 0) && (voxelPos.x < GI_VOXEL_GRID_RESOLUTION - 1) &&
			(voxelPos.y > 0) && (voxelPos.y < GI_VOXEL_GRID_RESOLUTION - 1) &&
			(voxelPos.z > 0) && (voxelPos.z < GI_VOXEL_GRID_RESOLUTION - 1))
		{
			int gridIdx = GetGridIndex(voxelPos, i);
			float4 surfaceNormalLobe = ClampedCosineCoeffs(pin.NormalW);
			float3 diffuseGlobalIllumination;
			VPL sampledVPL = SampleSHBuffer(pin.PosW, gSHBuffer, gGridBufferRO, i);
			diffuseGlobalIllumination.r = dot(sampledVPL.redSH, surfaceNormalLobe);
			diffuseGlobalIllumination.g = dot(sampledVPL.greenSH, surfaceNormalLobe);
			diffuseGlobalIllumination.b = dot(sampledVPL.blueSH, surfaceNormalLobe);
			diffuseGlobalIllumination = max(diffuseGlobalIllumination, float3(0.0f, 0.0f, 0.0f));
			diffuseGlobalIllumination /= gGlobalIllumAttFactor * PI;
			ADS[0].rgb += texColor * diffuseGlobalIllumination;
			break;
		}
	}
	//ssao
	if(gEnableSsao)
		ADS[0] *= gSsaoMap.SampleLevel(samPointClamp, pin.PosSS.xy, 0.0f);
	float4 litColor = texColor*(ADS[0] + ADS[1]) + ADS[2];
	//
	//Refraction/reflection
	//
	litColor = CalculateReflectionsAndRefractions(litColor, mMaterial, toEyeW, pin.NormalW, pin.PosW, pin.PosSS, true, pin.InstanceID);
	litColor.a = gMaterial.Diffuse.a;	

	return litColor;
}

float4 BuildSpecularMapPS(ShaderOut pin) : SV_Target
{
	//
	//Texturing
	//
	float4 texColor = CalculateTexColor(pin.Tex);
	//
	//Material mapping
	//
	Material mMaterial = gMaterial;
	if (gMaterialType == MTL_TYPE_REFLECT_MAP)
	{
		mMaterial.Reflect.x = 1.0f;
		mMaterial.Reflect.y = gReflectMap.Sample(samAnisotropic, pin.Tex);
		mMaterial.Reflect.z = -1.0f;
	}
	return float4(mMaterial.Reflect.xyz, 1.0f);
}

float4 BuildNormalDepthMapPS(ShaderOut pin) : SV_Target
{
	//
	//Texturing
	//
	float4 texColor = CalculateTexColor(pin.Tex);
	pin.NormalW = normalize(pin.NormalW);
	//
	//Normal/Displacement mapping
	//
	if (gUseNormalMapping || gUseDisplacementMapping)
	{
		float3 normalMapSample = gNormalMap.Sample(samLinear, pin.Tex).rgb;
		pin.NormalW = NormalSampleToWorldSpace(normalMapSample, pin.NormalW, pin.TangentW);
	}
	//
	//Calculate view space depth and normal
	//
	float PixelDepthV = CalculateViewSpaceDepth(pin.PosW);
	float3 PixelNormalV = mul(pin.NormalW, (float3x3)gView).xyz;

	return float4(PixelNormalV, PixelDepthV);
}

float4 BuildDepthMapPS(ShaderOut pin) : SV_Target
{
	//
	//Texturing
	//
	float4 texColor = CalculateTexColor(pin.Tex);
	float PixelDepth = CalculatePostProjectedDepth(pin.PosW);
	if(PixelDepth >= 1.0f)
		return float4(1.0f, 0.0f, 0.0f, 1.0f);

	return PixelDepth;
}

float BuildShadowMapPS(ShaderOut pin) : SV_Target
{
	//
	//Texturing
	//
	float4 texColor = CalculateTexColor(pin.Tex);
	//
	//Shadow position computation
	//
	if (gShadowType == DRAW_SHADOW_TYPE_DIRECTIONAL_LIGHT)
		return dot(gDirLight[gDirLightIndex].Direction, pin.PosW) / length(gDirLight[gDirLightIndex].Direction);
	else if (gShadowType == DRAW_SHADOW_TYPE_SPOT_LIGHT)
		return length(pin.PosW - gSpotLight[gSpotLightIndex].Position);
	else if (gShadowType == DRAW_SHADOW_TYPE_POINT_LIGHT)
		return length(pin.PosW - gPointLight[gPointLightIndex].Position);
	else
		return 0.0f;
}

float4 BuildAmbientMapPS(ShaderOut pin) : SV_Target
{
	//
	//Texturing
	//
	float4 texColor = CalculateTexColor(pin.Tex);
	//
	//Normal/Displacement mapping
	//
	pin.NormalW = normalize(pin.NormalW);
	if (gUseNormalMapping||gUseDisplacementMapping)
	{
		float3 normalMapSample = gNormalMap.Sample(samLinear, pin.Tex).rgb;
		pin.NormalW = NormalSampleToWorldSpace(normalMapSample, pin.NormalW, pin.TangentW);
	}
	//
	//Lighting
	//
	float4 toEyeW = CalculateToEyeW(pin.PosW);
	float4 litColor = CalculateLitColor(gMaterial, pin.NormalW, pin.PosW, toEyeW, 0)[0];
		
	litColor.a = gMaterial.Diffuse.a;
	return litColor;
}

float4 BuildDiffuseMapPS(ShaderOut pin) : SV_Target
{
	//
	//Texturing
	//
	float4 texColor = CalculateTexColor(pin.Tex);
	//
	//Normal/Displacement mapping
	//
	pin.NormalW = normalize(pin.NormalW);
	if (gUseNormalMapping||gUseDisplacementMapping)
	{
		float3 normalMapSample = gNormalMap.Sample(samLinear, pin.Tex).rgb;
		pin.NormalW = NormalSampleToWorldSpace(normalMapSample, pin.NormalW, pin.TangentW);
	}
	//
	//Material mapping
	//
	Material mMaterial = gMaterial;
	if(gMaterialType == MTL_TYPE_REFLECT_MAP)
	{
		mMaterial.Reflect.x = 1.0f;
		mMaterial.Reflect.y = gReflectMap.Sample(samAnisotropic, pin.Tex);
		mMaterial.Reflect.z = -1.0f;

	}
	//
	//Lighting
	//
	float PixelDepthV = CalculateViewSpaceDepth(pin.PosW);
	float4 toEyeW = CalculateToEyeW(pin.PosW);
	float4 litColor = texColor*CalculateLitColor(mMaterial, pin.NormalW, pin.PosW, toEyeW, PixelDepthV)[1];

	litColor.a = gMaterial.Diffuse.a;
	return litColor;
}

float4 BuildWSPositionMapPS(ShaderOut pin) : SV_Target
{
	CalculateTexColor(pin.Tex);
	return float4(pin.PosW, 1.0f);
}

//PPLL/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ListNode
{
	float4 PackedData;
	float2 DepthAndCoverage;//coverage for MSAA
	uint NextIndex;
};

cbuffer cbPPLL
{
	uint clientH;
	uint clientW;
};

globallycoherent RWTexture2D<uint>HeadIndexBuffer;
globallycoherent RWStructuredBuffer<ListNode>FragmentsListBuffer;

[earlydepthstencil]//writing to ppll without this modifier will lead to incorrect results
float4 BuildPPLLPS(ShaderOut pin, uint coverage : SV_Coverage, bool isFrontFace : SV_IsFrontFace) : SV_Target
{
	float4 texColor = CalculateTexColor(pin.Tex), output = 0;
	float PixelDepthV = CalculateViewSpaceDepth(pin.PosW);
	float4 toEyeW = CalculateToEyeW(pin.PosW);
	
	Material mMaterial = gMaterial;
	if (gMaterialType == MTL_TYPE_REFLECT_MAP) {
		mMaterial.Reflect.x = 1.0f;
		mMaterial.Reflect.y = gReflectMap.Sample(samAnisotropic, pin.Tex);
		mMaterial.Reflect.z = -1.0f;
	}

	pin.NormalW = normalize(pin.NormalW);
	if (gUseNormalMapping || gUseDisplacementMapping) {
		float3 normalMapSample = gNormalMap.Sample(samLinear, pin.Tex).rgb;
		pin.NormalW = NormalSampleToWorldSpace(normalMapSample, pin.NormalW, pin.TangentW);
	}

	if (!isFrontFace)
		pin.NormalW *= -1;

	float4x4 ADS = CalculateLitColor(mMaterial, pin.NormalW, pin.PosW, toEyeW, PixelDepthV);

	switch (gDrawPassType)
	{
	case DRAW_PASS_TYPE_COLOR:
		pin.PosSS /= pin.PosSS.w;
	
		if (pin.PosW.y <= gWaterHeight - WORLD_POSITION_CLIP_TRESHOLD && gDrawWaterReflection)
			clip(-1);

		if (pin.PosW.y >= gWaterHeight + WORLD_POSITION_CLIP_TRESHOLD && gDrawWaterRefraction)
			clip(-1);

		for (int i = 0; i < GI_CASCADES_COUNT && gEnableGI; ++i)
		{
			float3 samplePos;
			int3 voxelPos;
			CalculateGridPositions(pin.PosW, i, samplePos, voxelPos);

			if ((voxelPos.x > 0) && (voxelPos.x < GI_VOXEL_GRID_RESOLUTION - 1) &&
				(voxelPos.y > 0) && (voxelPos.y < GI_VOXEL_GRID_RESOLUTION - 1) &&
				(voxelPos.z > 0) && (voxelPos.z < GI_VOXEL_GRID_RESOLUTION - 1))
			{
				int gridIdx = GetGridIndex(voxelPos, i);
				float4 surfaceNormalLobe = ClampedCosineCoeffs(pin.NormalW);
				float3 diffuseGlobalIllumination;
				VPL sampledVPL = SampleSHBuffer(pin.PosW, gSHBuffer, gGridBufferRO, i);
				diffuseGlobalIllumination.r = dot(sampledVPL.redSH, surfaceNormalLobe);
				diffuseGlobalIllumination.g = dot(sampledVPL.greenSH, surfaceNormalLobe);
				diffuseGlobalIllumination.b = dot(sampledVPL.blueSH, surfaceNormalLobe);
				diffuseGlobalIllumination = max(diffuseGlobalIllumination, float3(0.0f, 0.0f, 0.0f));
				diffuseGlobalIllumination /= gGlobalIllumAttFactor * PI;
				ADS[0].rgb += texColor * diffuseGlobalIllumination;
				break;
			}
		}

		if (gEnableSsao && isFrontFace)
			ADS[0] *= gSsaoMap.SampleLevel(samPointClamp, pin.PosSS.xy, 0.0f);
		
		output = texColor*(ADS[0] + ADS[1]) + ADS[2];
		output = CalculateReflectionsAndRefractions(output, mMaterial, toEyeW, pin.NormalW, pin.PosW, pin.PosSS, isFrontFace, pin.InstanceID);
		break;
	case DRAW_PASS_TYPE_AMBIENT:
		output = ADS[0];
		break;
	case DRAW_PASS_TYPE_DIFFUSE:
		output = texColor*ADS[1];
		break;
	default:
		clip(-1);
		break;
	}

	output.a = gMaterial.Diffuse.a * texColor.a;

	uint newHeadIndexBufferValue = FragmentsListBuffer.IncrementCounter();
	if (newHeadIndexBufferValue == 0xffffffff)
		return float4(0.0f, 0.0f, 0.0f, 0.0f);

	uint2 ScreenPosXY = pin.PosSS.xy * uint2(clientW, clientH);
	uint prevHeadIndexBufferValue;
	InterlockedExchange(HeadIndexBuffer[ScreenPosXY], newHeadIndexBufferValue, prevHeadIndexBufferValue);

	float PixelDepth = CalculatePostProjectedDepth(pin.PosW);

	ListNode node;
	node.PackedData = output;
	node.DepthAndCoverage = float2(PixelDepth, coverage);
	node.NextIndex = prevHeadIndexBufferValue;

	FragmentsListBuffer[newHeadIndexBufferValue] = node;

	discard;

	return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

//GPU particle-based physics///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

cbuffer cbBodiesPPLL
{
	int gBodyIndex[MAX_INSTANCES_COUNT];
};

globallycoherent RWTexture2D<uint>gHeadIndexBufferRW;
globallycoherent RWStructuredBuffer<BodiesListNode>gBodiesListBufferRW;

//before this pass need to clean head buffer with BodiesCount value!!!!

//render to offscreen PARTICLES_GRID_RESOLUTIONx x PARTICLES_GRID_RESOLUTION render target
[earlydepthstencil]//writing to ppll without this modifier will lead to incorrect results
float4 BuildBodiesPPLLPS(ShaderOut pin, bool isFrontFace : SV_IsFrontFace) : SV_Target
{
	CalculateTexColor(pin.Tex);
	int BodyIndex = gBodyIndex[pin.InstanceID];
	if (!isFrontFace)
		BodyIndex = -(BodyIndex + 1);

	int newHeadIndexBufferValue = gBodiesListBufferRW.IncrementCounter();
	if (newHeadIndexBufferValue == 0xffffffff)
	{
		return float4(0.0f, 0.0f, 0.0f, 0.0f);
	}
	uint2 ScreenPosXY = pin.PosSS.xy * uint2(PARTICLES_GRID_RESOLUTION, PARTICLES_GRID_RESOLUTION);
	int prevHeadIndexBufferValue;
	InterlockedExchange(gHeadIndexBufferRW[ScreenPosXY], newHeadIndexBufferValue, prevHeadIndexBufferValue);

	BodiesListNode node;
	node.PosWBodyIndex = float4(pin.PosW, BodyIndex);
	node.NextIndex = prevHeadIndexBufferValue;
	gBodiesListBufferRW[newHeadIndexBufferValue] = node;
	
	return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
//SVO Builder//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
[maxvertexcount(3)]
void BuildSVOGS(triangle ShaderOut gin[3], inout TriangleStream<SvoBuildGsOut>outputStream)
{
	float3 polygonNormal = normalize(gin[0].NormalW + gin[1].NormalW + gin[2].NormalW);

	int viewIdx = GetViewMatrixIndex(polygonNormal);

	SvoBuildGsOut gout[3];

	[unroll(3)]
	for (int i = 0; i < 3; ++i)
	{
		gout[i].PosH = mul(gSVOViewProj[viewIdx], float4(gin[i].PosW, 1.0f));
		gout[i].PosW = gin[i].PosW.xyz;
		gout[i].Tex = gin[i].Tex;
		gout[i].NormalW = gin[i].NormalW;
	}

	float texSz = 1.0f / SVO_BUILD_RT_SZ;

	float2 side0N = normalize(gout[1].PosH.xy - gout[0].PosH.xy);
	float2 side1N = normalize(gout[2].PosH.xy - gout[1].PosH.xy);
	float2 side2N = normalize(gout[0].PosH.xy - gout[2].PosH.xy);
	gout[0].PosH.xy += normalize(-side0N + side2N) * texSz;
	gout[1].PosH.xy += normalize(side0N - side1N) * texSz;
	gout[2].PosH.xy += normalize(side1N - side2N) * texSz;

	[unroll(3)]
	for (int i = 0; i < 3; ++i)
		outputStream.Append(gout[i]);
	outputStream.RestartStrip();
}

void BuildSVOPS(SvoBuildGsOut pin)
{
	// Continue only if pixel in root node bounds
	if (!inBounds(gSvoRootNodeCenterPosW, SVO_ROOT_NODE_SZ, pin.PosW))
		return;
	/*
	// Normalize normal
	pin.NormalW = normalize(pin.NormalW);
	// Material mapping
	Material mMaterial = gMaterial;
	if(gMaterialType == MTL_TYPE_REFLECT_MAP)
	{
		mMaterial.Reflect.x = 1.0f;
		mMaterial.Reflect.y = gReflectMap.Sample(samAnisotropic, pin.Tex);
		mMaterial.Reflect.z = -1.0f;
	}
	// Calcualte view-space depth and "to eye" vector
	float PixelDepthV = CalculateViewSpaceDepth(pin.PosW);
	float4 toEyeW = CalculateToEyeW(pin.PosW);
	// Compute shadows and ambient, diffuse and specular lighting terms
	float4x4 ADS = CalculateLitColor(mMaterial, pin.NormalW, pin.PosW, toEyeW, PixelDepthV);*/
	// Sample texture color
	float4 texColor = CalculateTexColor(pin.Tex);
	// Lit pixel
	float4 litColor = texColor * gMaterial.Diffuse;
	// Compute color mask
	uint colorMask = PackUnormFloat4(litColor);
	// Fill octree in top-to-bottom manner
	[unroll(SVO_LEVELS_COUNT)]
	for (int SvoLevelIdx = 0; SvoLevelIdx < SVO_LEVELS_COUNT; ++SvoLevelIdx)
	{
		// add current level node as child of some node only if we alredy initialized root node
		if (SvoLevelIdx > 0)
		{
			int parentIdx = getSvoNodeIndexRW(pin.PosW, SvoLevelIdx);
			addSvoNodeChild(parentIdx, pin.PosW);
		}
		// write voxels with levels only above gSvoMinAveragingLevelIdx for performance purposes
		if (SvoLevelIdx >= gSvoMinAveragingLevelIdx)
		{
			SvoVoxel newVoxel;
			newVoxel.colorMask = colorMask;
			setVoxelData(pin.PosW, SvoLevelIdx + 1, newVoxel, 1);
		}
	}
}
//SVO structure visualizer///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SvoDebugDrawVsIn SvoDebugDrawVS(SvoDebugDrawVsIn vin)
{
	return vin;
}

[maxvertexcount(24)]
void SvoDebugDrawGS(point SvoDebugDrawVsIn gin[1],
	inout LineStream<SvoDebugDrawGsOut> lineStream)
{
	float offsetLen = getSvoNodeSize(gin[0].svoLevelIdx) / 2.0;
	
	float4 v[8];
	v[0] = float4(gin[0].PosW + offsetLen * float3(1, 1, 1), 1.0f);
	v[1] = float4(gin[0].PosW + offsetLen * float3(1, 1, -1), 1.0f);
	v[2] = float4(gin[0].PosW + offsetLen * float3(-1, 1, 1), 1.0f);
	v[3] = float4(gin[0].PosW + offsetLen * float3(-1, 1, -1), 1.0f);
	v[4] = float4(gin[0].PosW + offsetLen * float3(1, -1, 1), 1.0f);
	v[5] = float4(gin[0].PosW + offsetLen * float3(1, -1, -1), 1.0f);
	v[6] = float4(gin[0].PosW + offsetLen * float3(-1, -1, 1), 1.0f);
	v[7] = float4(gin[0].PosW + offsetLen * float3(-1, -1, -1), 1.0f);

	SvoDebugDrawGsOut gout;
	//cube top
	gout.PosH = mul(v[0], gViewProj);
	lineStream.Append(gout);
	gout.PosH = mul(v[1], gViewProj);
	lineStream.Append(gout);
	lineStream.RestartStrip();

	lineStream.Append(gout);
	gout.PosH = mul(v[3], gViewProj);
	lineStream.Append(gout);
	lineStream.RestartStrip();

	lineStream.Append(gout);
	gout.PosH = mul(v[2], gViewProj);
	lineStream.Append(gout);
	lineStream.RestartStrip();

	lineStream.Append(gout);
	gout.PosH = mul(v[0], gViewProj);
	lineStream.Append(gout);
	lineStream.RestartStrip();
	//cube bottom
	gout.PosH = mul(v[4], gViewProj);
	lineStream.Append(gout);
	gout.PosH = mul(v[5], gViewProj);
	lineStream.Append(gout);
	lineStream.RestartStrip();

	lineStream.Append(gout);
	gout.PosH = mul(v[7], gViewProj);
	lineStream.Append(gout);
	lineStream.RestartStrip();

	lineStream.Append(gout);
	gout.PosH = mul(v[6], gViewProj);
	lineStream.Append(gout);
	lineStream.RestartStrip();

	lineStream.Append(gout);
	gout.PosH = mul(v[4], gViewProj);
	lineStream.Append(gout);
	lineStream.RestartStrip();
	//cube vertical edges
	gout.PosH = mul(v[0], gViewProj);
	lineStream.Append(gout);
	gout.PosH = mul(v[4], gViewProj);
	lineStream.Append(gout);
	lineStream.RestartStrip();

	gout.PosH = mul(v[1], gViewProj);
	lineStream.Append(gout);
	gout.PosH = mul(v[5], gViewProj);
	lineStream.Append(gout);
	lineStream.RestartStrip();

	gout.PosH = mul(v[2], gViewProj);
	lineStream.Append(gout);
	gout.PosH = mul(v[6], gViewProj);
	lineStream.Append(gout);
	lineStream.RestartStrip();

	gout.PosH = mul(v[3], gViewProj);
	lineStream.Append(gout);
	gout.PosH = mul(v[7], gViewProj);
	lineStream.Append(gout);
	lineStream.RestartStrip();
}

float4 SvoDebugDrawPS(SvoDebugDrawGsOut pin) : SV_TARGET
{
	return float4(1,0,0,1);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GenerateGridPS(GenGridGSOut pin)
{
	//
	//Material mapping
	//
	Material mMaterial = gMaterial;
	if(gMaterialType == MTL_TYPE_REFLECT_MAP)
	{
		mMaterial.Reflect.x = 1.0f;
		mMaterial.Reflect.y = gReflectMap.Sample(samAnisotropic, pin.Tex);
		mMaterial.Reflect.z = -1.0f;

	}

	float4 texColor = CalculateTexColor(pin.Tex) * mMaterial.Diffuse;
	uint colorMask = EncodeColor(texColor.rgb);

	float3 normal = normalize(pin.NormalW);
	uint normalMask = EncodeNormal(normal.xyz);

	float3 samplePos;
	int3 voxelPos;
	CalculateGridPositions(pin.PosW, pin.CascadeIndex, samplePos, voxelPos);

	if((voxelPos.x > -1) && (voxelPos.x < GI_VOXEL_GRID_RESOLUTION) &&
		(voxelPos.y > -1) && (voxelPos.y < GI_VOXEL_GRID_RESOLUTION) &&
		(voxelPos.z > -1) && (voxelPos.z < GI_VOXEL_GRID_RESOLUTION))
	{
		int gridIdx = GetGridIndex(voxelPos, pin.CascadeIndex);
		int normalIdx = GetNormalIndex(pin.NormalW);

		InterlockedMax(gGridBuffer[gridIdx].colorMask, colorMask);
		InterlockedMax(gGridBuffer[gridIdx].normalMasks[normalIdx], normalMask);
	}
}

//////////////////////Terrain////////////////////////////////////////////////////////////////////////
TerrainVertexOut TerrainVS(TerrainVertexIn vin)
{
	TerrainVertexOut vout;

	vout.PosW = vin.PosL;
	vout.PosW.y = gHeightMap.SampleLevel(samHeightmap, vin.Tex, 0).r;

	vout.Tex = vin.Tex;

	vout.BoundsY = vin.BoundsY;

	return vout;
}

TerrainPatchTess TerrainConstantHS(InputPatch<TerrainVertexOut, 4>patch, uint patchID : SV_PrimitiveID)
{
	TerrainPatchTess pt;

	float minY = patch[0].BoundsY.x;
	float maxY = patch[0].BoundsY.y;

	float3 vMin = float3(patch[2].PosW.x, minY, patch[2].PosW.z);
		float3 vMax = float3(patch[1].PosW.x, maxY, patch[1].PosW.z);

		float3 boxCenter = 0.5f*(vMin + vMax);
		float3 boxExtents = 0.5f*(vMax - vMin);

	if (AABBOutsideFrustrumTest(boxCenter, boxExtents, gWorldFrustrumPlanes))
	{
		pt.EdgeTess[0] = 0.0f;
		pt.EdgeTess[1] = 0.0f;
		pt.EdgeTess[2] = 0.0f;
		pt.EdgeTess[3] = 0.0f;

		pt.InsideTess[0] = 0.0f;
		pt.InsideTess[1] = 0.0f;

		return pt;
	}
	else{
		float3 e0 = 0.5f*(patch[0].PosW + patch[2].PosW);
		float3 e1 = 0.5f*(patch[0].PosW + patch[1].PosW);
		float3 e2 = 0.5f*(patch[1].PosW + patch[3].PosW);
		float3 e3 = 0.5f*(patch[2].PosW + patch[3].PosW);
		
		float3 c = 0.25f*(patch[0].PosW + patch[1].PosW + patch[2].PosW + patch[3].PosW);

		pt.EdgeTess[0] = CalcTessFactor(e0);
		pt.EdgeTess[1] = CalcTessFactor(e1);
		pt.EdgeTess[2] = CalcTessFactor(e2);
		pt.EdgeTess[3] = CalcTessFactor(e3);

		pt.InsideTess[0] = CalcTessFactor(c);
		pt.InsideTess[1] = pt.InsideTess[0];

		return pt;
	}
}

[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("TerrainConstantHS")]
[maxtessfactor(64.0f)]
HullOut TerrainHS(InputPatch<TerrainVertexOut, 4>p,
	uint i : SV_OutputControlPointID,
	uint patchId : SV_PrimitiveID)
{
	HullOut hout;
	hout.PosW = p[i].PosW;
	hout.Tex = p[i].Tex;

	return hout;
}

[domain("quad")]
DomainOut TerrainDS(TerrainPatchTess patchTess,
	float2 uv : SV_DomainLocation,
	const OutputPatch<HullOut, 4>quad)
{
	DomainOut dout;

	dout.PosW = lerp(
		lerp(quad[0].PosW, quad[1].PosW, uv.x),
		lerp(quad[2].PosW, quad[3].PosW, uv.x),
		uv.y);

	dout.Tex = lerp(
		lerp(quad[0].Tex, quad[1].Tex, uv.x),
		lerp(quad[2].Tex, quad[3].Tex, uv.x),
		uv.y);

	dout.TiledTex = dout.Tex*gTexScale;

	dout.PosW.y = gHeightMap.SampleLevel(samHeightmap, dout.Tex, 0).r;

	float4 normalW = CalculateTerrainNormalW(dout.Tex, dout.TiledTex);

	dout.PosW += (gHeightScale*((normalW.w - 1.0f) == 0 ? 1 : (normalW.w - 1.0f)))*normalW;
	dout.PosH = mul(float4(dout.PosW, 1.0f), gViewProj);
	dout.PosSS = mul(dout.PosH, toTexSpace);

	return dout;
}

[maxvertexcount(3 * GI_CASCADES_COUNT)]
void GenerateGridTerrainGS(triangle DomainOut gin[3], inout TriangleStream<GenTerrainGridGSOut>outputStream)
{
	float3 polygonNormal = normalize(CalculateTerrainNormalW(gin[0].Tex, gin[0].TiledTex) +
		CalculateTerrainNormalW(gin[1].Tex, gin[1].TiledTex) +
		CalculateTerrainNormalW(gin[2].Tex, gin[2].TiledTex));
	
	[unroll(GI_CASCADES_COUNT)]
	for(int j = 0; j < GI_CASCADES_COUNT; ++j)
	{
		int viewIdx = GetViewIndex(polygonNormal, j);

		GenTerrainGridGSOut gout[3];

		[unroll(3)]
		for(int i = 0; i < 3; ++i)
		{
			gout[i].PosH = mul(gGridViewProj[viewIdx], float4(gin[i].PosW, 1.0f));
			gout[i].PosW = gin[i].PosW.xyz;
			gout[i].Tex = gin[i].Tex;
			gout[i].TiledTex = gin[i].TiledTex;
			gout[i].PosSS = gin[i].PosSS;
			gout[i].CascadeIndex = j;
		}

		float2 side0N = normalize(gout[1].PosH.xy - gout[0].PosH.xy);		
		float2 side1N = normalize(gout[2].PosH.xy - gout[1].PosH.xy);
		float2 side2N = normalize(gout[0].PosH.xy - gout[2].PosH.xy);
		gout[0].PosH.xy += normalize(-side0N + side2N) * texelSize;
		gout[1].PosH.xy += normalize(side0N - side1N) * texelSize;
		gout[2].PosH.xy += normalize(side1N - side2N) * texelSize;

		[unroll(3)]
		for(int i = 0; i < 3; ++i)
			outputStream.Append(gout[i]);
		outputStream.RestartStrip();
	}
}


float4 TerrainPS(DomainOut pin) : SV_Target
{
	pin.PosSS /= pin.PosSS.w;

	if (pin.PosW.y <= gWaterHeight - WORLD_POSITION_CLIP_TRESHOLD && gDrawWaterReflection)
		clip(-1);

	if (pin.PosW.y >= gWaterHeight + WORLD_POSITION_CLIP_TRESHOLD && gDrawWaterRefraction)
		clip(-1);

	float3 normalW = CalculateTerrainNormalW(pin.Tex, pin.TiledTex);
	float4 texColor = CalculateTerrainTexColor(pin.Tex, pin.TiledTex);

	float PixelDepthV = CalculateViewSpaceDepth(pin.PosW);
	float4 toEyeW = CalculateToEyeW(pin.PosW);
	float4x4 ADS = CalculateLitColor(gMaterial, normalW, pin.PosW, toEyeW, PixelDepthV);
	
	for (int i = 0; i < GI_CASCADES_COUNT && gEnableGI; ++i)
	{
		float3 samplePos;
		int3 voxelPos;
		CalculateGridPositions(pin.PosW, i, samplePos, voxelPos);

		if ((voxelPos.x > 0) && (voxelPos.x < GI_VOXEL_GRID_RESOLUTION - 1) &&
			(voxelPos.y > 0) && (voxelPos.y < GI_VOXEL_GRID_RESOLUTION - 1) &&
			(voxelPos.z > 0) && (voxelPos.z < GI_VOXEL_GRID_RESOLUTION - 1))
		{
			int gridIdx = GetGridIndex(voxelPos, i);
			float4 surfaceNormalLobe = ClampedCosineCoeffs(normalW);
			float3 diffuseGlobalIllumination;
			VPL sampledVPL = SampleSHBuffer(pin.PosW, gSHBuffer, gGridBufferRO, i);
			diffuseGlobalIllumination.r = dot(sampledVPL.redSH, surfaceNormalLobe);
			diffuseGlobalIllumination.g = dot(sampledVPL.greenSH, surfaceNormalLobe);
			diffuseGlobalIllumination.b = dot(sampledVPL.blueSH, surfaceNormalLobe);
			diffuseGlobalIllumination = max(diffuseGlobalIllumination, float3(0.0f, 0.0f, 0.0f));
			diffuseGlobalIllumination /= gGlobalIllumAttFactor * PI;
			ADS[0].rgb += texColor * diffuseGlobalIllumination;
			break;
		}
	}

	if (gEnableSsao)
		ADS[0] *= gSsaoMap.SampleLevel(samPointClamp, pin.PosSS.xy, 0.0f);
	float4 litColor = texColor*(ADS[0] + ADS[1]) + ADS[2];

	litColor.a = gMaterial.Diffuse.a;
	return litColor;
}

void GenerateGridTerrainPS(GenTerrainGridGSOut pin)
{
	float4 texColor = CalculateTerrainTexColor(pin.Tex, pin.TiledTex);
	uint colorMask = EncodeColor(texColor.rgb);

	float3 normal = CalculateTerrainNormalW(pin.Tex, pin.TiledTex);
	uint normalMask = EncodeNormal(normal.xyz);

	float3 samplePos;
	int3 voxelPos;
	CalculateGridPositions(pin.PosW, pin.CascadeIndex, samplePos, voxelPos);

	if((voxelPos.x > -1) && (voxelPos.x < GI_VOXEL_GRID_RESOLUTION) &&
		(voxelPos.y > -1) && (voxelPos.y < GI_VOXEL_GRID_RESOLUTION) &&
		(voxelPos.z > -1) && (voxelPos.z < GI_VOXEL_GRID_RESOLUTION))
	{
		int gridIdx = GetGridIndex(voxelPos, pin.CascadeIndex);
		int normalIdx = GetNormalIndex(normal);

		InterlockedMax(gGridBuffer[gridIdx].colorMask, colorMask);
		InterlockedMax(gGridBuffer[gridIdx].normalMasks[normalIdx], normalMask);
	}
}

float BuildShadowMapTerrainPS(DomainOut pin) : SV_Target
{
	float4 texColor = CalculateTerrainTexColor(pin.Tex, pin.TiledTex);

	if (gShadowType == DRAW_SHADOW_TYPE_DIRECTIONAL_LIGHT)
		return dot(gDirLight[gDirLightIndex].Direction, pin.PosW) / length(gDirLight[gDirLightIndex].Direction);
	else if (gShadowType == DRAW_SHADOW_TYPE_SPOT_LIGHT)
		return length(pin.PosW - gSpotLight[gSpotLightIndex].Position);
	else if (gShadowType == DRAW_SHADOW_TYPE_POINT_LIGHT)
		return length(pin.PosW - gPointLight[gPointLightIndex].Position);
	else
		return 0.0f;
}

float4 BuildNormalDepthMapTerrainPS(DomainOut pin) : SV_Target
{
	float3 normalW = CalculateTerrainNormalW(pin.Tex, pin.TiledTex);

	float3 PixelNormalV = mul(normalW, (float3x3)gView).xyz;
	float PixelDepthV = mul(float4(pin.PosW, 1.0f), gView).z;

	return float4(PixelNormalV, PixelDepthV);
}

float4 BuildDepthMapTerrainPS(DomainOut pin) : SV_Target
{
	return CalculatePostProjectedDepth(pin.PosW);
}

float4 BuildAmbientMapTerrainPS(DomainOut pin) : SV_Target
{
	float3 normalW = CalculateTerrainNormalW(pin.Tex, pin.TiledTex);
	float4 texColor = CalculateTerrainTexColor(pin.Tex, pin.TiledTex);

	float PixelDepthV = CalculateViewSpaceDepth(pin.PosW);
	float4 toEyeW = CalculateToEyeW(pin.PosW);
	float4 litColor = CalculateLitColor(gMaterial, normalW, pin.PosW, toEyeW, PixelDepthV)[0];

	litColor.a = gMaterial.Diffuse.a;
	return litColor;
}

float4 BuildWSPositionMapTerrainPS(DomainOut pin) : SV_Target
{
	return float4(pin.PosW, 1.0f);
}

float4 BuildDiffuseMapTerrainPS(DomainOut pin) : SV_Target
{
	float3 normalW = CalculateTerrainNormalW(pin.Tex, pin.TiledTex);
	float4 texColor = CalculateTerrainTexColor(pin.Tex, pin.TiledTex);

	float PixelDepthV = CalculateViewSpaceDepth(pin.PosW);
	float4 toEyeW = CalculateToEyeW(pin.PosW);
	float4 litColor = texColor*CalculateLitColor(gMaterial, normalW, pin.PosW, toEyeW, PixelDepthV)[1];

	litColor.a = gMaterial.Diffuse.a;
	return litColor;
}

////////////////Water////////////////////////////////////////////////////////////////////////////////
struct VertexInputType
{
	float3 PosL : POSITION;
	float3 NormalL : NORMAL;
	float2 Tex : TEXCOORD;
	float3 TangentL : TANGENT;
	uint InstanceID : SV_INSTANCEID;
};

struct PixelInputType
{
	float2 Tex : TEXCOORD0;
	float4 PosH : SV_POSITION;
	float4 reflectionPosition : TEXCOORD1;
	float4 refractionPosition : TEXCOORD2;
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
	uint InstanceID : INSTANCEID;
};

PixelInputType WaterVertexShader(VertexInputType input)
{
	PixelInputType output;

	output.Tex = mul(float4(input.Tex, 0.0f, 1.0f), gTexTransform[0]).xy + cos(gTime);
	output.PosH = mul(float4(input.PosL, 1.0f), gWorldViewProj[input.InstanceID]);
	output.reflectionPosition = output.PosH;
	output.refractionPosition = output.PosH;
	output.PosW = mul(float4(input.PosL, 1.0f), gWorld[input.InstanceID]).xyz;
	output.NormalW = mul(float4(input.NormalL, 1.0f), gWorldInvTranspose[input.InstanceID]).xyz;
	output.InstanceID = input.InstanceID;
	return output;
}

float4 WaterPixelShader(PixelInputType input) : SV_TARGET
{
	//
	// Normal mapping and reflection/refraction texture distortion
	//
	float4 inTexProj = mul(input.reflectionPosition, Mr);
	float2 reflectTexCoord = inTexProj.xy / inTexProj.w;
	float3 normal = gNormalMap.Sample(samLinear, input.Tex).xyz;
	normal = (normal * 2.0f) - 1.0f;
	reflectTexCoord = reflectTexCoord + (normal.xy * WATER_PLANAR_REFLECTION_DISTORTION);
	float4 reflectionColor = gWaterReflectionTex.Sample(samLinear, reflectTexCoord);

	float2 refractTexCoord;
	refractTexCoord.x = input.refractionPosition.x / input.refractionPosition.w / 2.0f + 0.5f;
	refractTexCoord.y = -input.refractionPosition.y / input.refractionPosition.w / 2.0f + 0.5f;
	refractTexCoord = refractTexCoord + (normal.xy * WATER_PLANAR_REFRACTION_DISTORTION);
	float4 refractionColor = gWaterRefractionTex.Sample(samLinear, refractTexCoord);

	float3 tangent = ComputeTangent(input.NormalW);
	input.NormalW = NormalSampleToWorldSpace(gNormalMap.Sample(samLinear, input.Tex).xyz, input.NormalW, tangent);
	//
	// Texturing
	//
	float4 texColor = CalculateCustomMeshFbxTexColor(input.Tex);
	//
	// Lighting
	//
	float PixelDepthV = CalculateViewSpaceDepth(input.PosW);
	float4 toEyeW = CalculateToEyeW(input.PosW);
	float4x4 ADS = CalculateLitColor(gMaterial, input.NormalW, input.PosW, toEyeW, PixelDepthV);
	//
	// Refraction & reflection
	//
	float4 litColor = texColor*(ADS[0] + ADS[1]) + ADS[2];

	float n = gMaterial.Reflect.x;
	float sr = gMaterial.Reflect.y;
	float d = gMaterial.Reflect.z;

	float3 incident = -toEyeW;
	float FresnelMultiplier = FresnelSchlickApprox(FresnelCoeff(n, d), -incident, input.NormalW);

	litColor = lerp(litColor, reflectionColor, FresnelMultiplier);
	litColor = lerp(refractionColor, litColor, FresnelMultiplier);
	//
	// Fog
	//
	litColor.a = gMaterial.Diffuse.a;
	return litColor;
}

float BuildShadowMapWaterPixelShader(PixelInputType input) : SV_Target
{
	//
	//Shadow position calculation
	//
	if (gShadowType == DRAW_SHADOW_TYPE_DIRECTIONAL_LIGHT)
		return dot(gDirLight[gDirLightIndex].Direction, input.PosW) / length(gDirLight[gDirLightIndex].Direction);
	else if (gShadowType == DRAW_SHADOW_TYPE_SPOT_LIGHT)
		return length(input.PosW - gSpotLight[gSpotLightIndex].Position);
	else if (gShadowType == DRAW_SHADOW_TYPE_POINT_LIGHT)
		return length(input.PosW - gPointLight[gPointLightIndex].Position);
	else
		return 0.0f;
}

float4 BuildNormalDepthMapWaterPixelShader(PixelInputType input) : SV_TARGET
{
	float3 tangent = ComputeTangent(input.NormalW);
	input.NormalW = NormalSampleToWorldSpace(gNormalMap.Sample(samLinear, input.Tex).xyz, input.NormalW, tangent);

	float PixelDepthV = CalculateViewSpaceDepth(input.PosW);
	float3 PixelNormalV = mul(input.NormalW, (float3x3)gView).xyz;
	//
	//Texturing
	//
	float4 texColor = CalculateCustomMeshFbxTexColor(input.Tex);

	return float4(PixelNormalV, PixelDepthV);
}

float4 BuildDepthMapWaterPixelShader(PixelInputType input) : SV_TARGET
{
	//
	//Texturing
	//
	float4 texColor = CalculateCustomMeshFbxTexColor(input.Tex);

	float PixelDepth = CalculatePostProjectedDepth(input.PosW);

	return PixelDepth;
}

float4 BuildWSPositionMapWaterPixelShader(PixelInputType input) : SV_TARGET
{
	return float4(input.PosW, 1.0f);
}

float4 BuildAmbientMapWaterPixelShader(PixelInputType input) : SV_TARGET
{
	float3 tangent = ComputeTangent(input.NormalW);
	input.NormalW = NormalSampleToWorldSpace(gNormalMap.Sample(samLinear, input.Tex).xyz, input.NormalW, tangent);
	//
	// Lighting
	//
	float PixelDepthV = CalculateViewSpaceDepth(input.PosW);
	float4 toEyeW = CalculateToEyeW(input.PosW);
	float4 litColor = CalculateLitColor(gMaterial, input.NormalW, input.PosW, toEyeW, PixelDepthV)[0];

	litColor.a = gMaterial.Diffuse.a;

	return litColor;
}

float4 BuildDiffuseMapWaterPixelShader(PixelInputType input) : SV_TARGET
{
	float3 tangent = ComputeTangent(input.NormalW);
	input.NormalW = NormalSampleToWorldSpace(gNormalMap.Sample(samLinear, input.Tex).xyz, input.NormalW, tangent);
	//
	//Texturing
	//
	float4 texColor = CalculateCustomMeshFbxTexColor(input.Tex);
	//
	// Lighting
	//
	float PixelDepthV = CalculateViewSpaceDepth(input.PosW);
	float4 toEyeW = CalculateToEyeW(input.PosW);
	float4 litColor = texColor * CalculateLitColor(gMaterial, input.NormalW, input.PosW, toEyeW, PixelDepthV)[1];
	
	litColor.a = gMaterial.Diffuse.a;

	return litColor;
}

//////SPH////////////////////////////////////////////////////////////////////////////////////////////
Texture2D gWaterParticleNormalMap;
Texture2D gPosWMap;
Texture2D gNormalWMap;
Texture2D gFluidRefractionMap;

cbuffer cbWaterParticle
{
	float gScale;
	float gRadius;

	float2 gQuadTex[4] =
	{
		float2(0.0f, 1.0f),
		float2(1.0f, 1.0f),
		float2(0.0f, 0.0f),
		float2(1.0f, 0.0f)
	};
};

struct BuildPosWMapVsIn
{
	float3 Pos : POSITION;
	float3 Vel : VELOCITY;
	float3 Acc : ACCELERATION;
};

struct BuildPosWMapVsOut
{
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
};

struct BuildPosWMapGsOut
{
	float2 TexC : TEXCOORD0;
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float3 LookV : NORMAL;
};

BuildPosWMapVsOut BuildPosWMapVS(BuildPosWMapVsIn vin)
{
	BuildPosWMapVsOut vout;

	vout.PosW = mul(float4(vin.Pos, 1.0f), gWorld[0]).xyz;
	vout.PosH = mul(float4(vin.Pos, 1.0f), gWorldViewProj[0]);
	
	return vout;
}

[maxvertexcount(8)]
void BuildPosWMapGS(point BuildPosWMapVsOut gin[1],
	inout TriangleStream<BuildPosWMapGsOut> triStream)
{
	float3 look;
	if (gShadowType == DRAW_SHADOW_TYPE_NO_SHADOW)
		look = normalize(gEyePosW.xyz - gin[0].PosW);
	else if (gShadowType == DRAW_SHADOW_TYPE_DIRECTIONAL_LIGHT)
		look = -gDirLight[gDirLightIndex].Direction;
	else if (gShadowType == DRAW_SHADOW_TYPE_SPOT_LIGHT)
		look = gSpotLight[gSpotLightIndex].Position - gin[0].PosW;
	else
		look = gPointLight[gPointLightIndex].Position - gin[0].PosW;
	float3 right = normalize(cross(float3(0, 1, 0), look));
	float3 up = cross(look, right);

	//
	// Compute triangle strip vertices (quad) in world space.
	//

	float4 v[4];
	v[0] = float4(gin[0].PosW + gRadius*gScale*right - gRadius*gScale*up, 1.0f);
	v[1] = float4(gin[0].PosW + gRadius*gScale*right + gRadius*gScale*up, 1.0f);
	v[2] = float4(gin[0].PosW - gRadius*gScale*right - gRadius*gScale*up, 1.0f);
	v[3] = float4(gin[0].PosW - gRadius*gScale*right + gRadius*gScale*up, 1.0f);

	//
	// Transform quad vertices to world space and output 
	// them as a triangle strip.
	//
	BuildPosWMapGsOut gout;
	[unroll]
	for (int i = 0; i < 4; ++i)
	{
		gout.PosH = mul(v[i], gViewProj);
		gout.PosW = v[i];
		gout.TexC = gQuadTex[i];
		gout.LookV = look;
		triStream.Append(gout);
	}
}

//BuildDepthMap
float4 BuildPosWMapPS(BuildPosWMapGsOut pin) : SV_TARGET
{
	float3 N = gWaterParticleNormalMap.SampleLevel(samPointClamp, pin.TexC, 0.0f).xyz;
	clip(N.r + N.g + N.b - 0.01);
	
	pin.PosW = float4(pin.PosW.xyz + pin.LookV * abs(N.z) * gRadius*gScale, 1.0f);
	
	float4 PosV = mul(float4(pin.PosW, 1.0f), gView);
	pin.PosH = mul(PosV, gProj);

	return float4(pin.PosW, 1.0f);
}

/////////////////////////////
//Blur depth map somewhere
//...
/////////////////////////////

struct ScrenQuadVsIn
{
	float3 PosL : POSITION;
	float2 Tex  : TEXCOORD;
};

struct ScrenQuadVsOut
{
	float4 PosH : SV_POSITION;
	float2 Tex  : TEXCOORD;
};

//Unified screen quad processing vertex shader
ScrenQuadVsOut ScreenQuadVS(ScrenQuadVsIn vin)
{
	ScrenQuadVsOut vout;
	
	vout.PosH = float4(vin.PosL, 1.0f);
	vout.Tex = vin.Tex;

	return vout;
}

//Build normal map
float4 BuildNormalWMapPS(ScrenQuadVsOut pin) : SV_Target
{
	float3 p = gPosWMap.SampleLevel(samPointClamp, pin.Tex, 0.0f).xyz;
	return float4(cross(ddx(p), ddy(p)), 1.0f);
}

//Draw fluid
float4 DrawSPHFluidPS(ScrenQuadVsOut pin) : SV_Target
{
	float3 PosW = gPosWMap.SampleLevel(samPointClamp, pin.Tex, 0.0f).xyz;
	if (length(PosW) < EPSILON)
		clip(-1);
	
	float3 NormalW = gNormalWMap.SampleLevel(samAnisotropic, pin.Tex, 0.0f).xyz;
	if (gDrawPassType != DRAW_PASS_TYPE_SHADOW_MAP_BUILD)
	{
		if (length(NormalW) < EPSILON)
			clip(-1);
	}
	NormalW = normalize(NormalW);

	float4x4 ADS = 0;
	float PixelDepthV = 0;
	float4 toEyeW = 0;
	float4 output = 0;

	switch (gDrawPassType)
	{
		case DRAW_PASS_TYPE_COLOR:
			PixelDepthV = CalculateViewSpaceDepth(PosW);
			toEyeW = CalculateToEyeW(PosW);
			ADS = CalculateLitColor(gMaterial, NormalW, PosW, toEyeW, PixelDepthV);
			output = ADS[0] + ADS[1] + ADS[2];
			output = CalculateReflectionsAndRefractions(output, gMaterial, toEyeW, NormalW, PosW, float4(pin.Tex, 0.0f, 1.0f), true, 0);
			break;
		case DRAW_PASS_TYPE_NORMAL_DEPTH:
			PixelDepthV = CalculateViewSpaceDepth(PosW);
			float3 NormalV = mul(NormalW, (float3x3)gView).xyz;
			output = float4(NormalV, PixelDepthV);
			break;	
		case DRAW_PASS_TYPE_AMBIENT:
			toEyeW = CalculateToEyeW(PosW);
			ADS = CalculateLitColor(gMaterial, NormalW, PosW, toEyeW, 0);
			output = ADS[0];
			break;
		case DRAW_PASS_TYPE_DIFFUSE:
			PixelDepthV = CalculateViewSpaceDepth(PosW);
			toEyeW = CalculateToEyeW(PosW);
			ADS = CalculateLitColor(gMaterial, NormalW, PosW, toEyeW, PixelDepthV);
			output = ADS[1];
			break;
		case DRAW_PASS_TYPE_DEPTH:
			output = CalculatePostProjectedDepth(PosW);
			break;
		case DRAW_PASS_TYPE_POSITION_WS:
			output = float4(PosW, 1.0f);
			break;
		case DRAW_PASS_TYPE_COLOR_NO_REFLECTION:
			PixelDepthV = CalculateViewSpaceDepth(PosW);
			toEyeW = CalculateToEyeW(PosW);
			ADS = CalculateLitColor(gMaterial, NormalW, PosW, toEyeW, PixelDepthV);
			output = ADS[0] + ADS[1] + ADS[2];
			break;
		case DRAW_PASS_TYPE_MATERIAL_SPECULAR:
			output = float4(gMaterial.Reflect.xyz, 1.0f);
			break;
		case DRAW_PASS_TYPE_SHADOW_MAP_BUILD:
			if (gShadowType == DRAW_SHADOW_TYPE_DIRECTIONAL_LIGHT)
				output =  dot(gDirLight[gDirLightIndex].Direction, PosW) / length(gDirLight[gDirLightIndex].Direction);
			else if (gShadowType == DRAW_SHADOW_TYPE_SPOT_LIGHT)
				output = length(PosW - gSpotLight[gSpotLightIndex].Position);
			else if (gShadowType == DRAW_SHADOW_TYPE_POINT_LIGHT)
				output = length(PosW - gPointLight[gPointLightIndex].Position);
			break;
		default:
			clip(-1);
			break;
	}
	return output;
}

technique11 BuildPosWMapTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, BuildPosWMapVS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(CompileShader(gs_5_0, BuildPosWMapGS()));
		SetPixelShader(CompileShader(ps_5_0, BuildPosWMapPS()));
	}
};

technique11 BuildNormalWMapTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, ScreenQuadVS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildNormalWMapPS()));
	}
};

technique11 DrawSPHFluidTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, ScreenQuadVS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, DrawSPHFluidPS()));
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
technique11 TerrainTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, TerrainVS()));
		SetHullShader(CompileShader(hs_5_0, TerrainHS()));
		SetDomainShader(CompileShader(ds_5_0, TerrainDS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, TerrainPS()));
	}
};

technique11 BuildShadowMapTerrainTech {
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, TerrainVS()));
		SetHullShader(CompileShader(hs_5_0, TerrainHS()));
		SetDomainShader(CompileShader(ds_5_0, TerrainDS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildShadowMapTerrainPS()));
	}
};

technique11 BuildWSPositionMapTerrainTech {
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, TerrainVS()));
		SetHullShader(CompileShader(hs_5_0, TerrainHS()));
		SetDomainShader(CompileShader(ds_5_0, TerrainDS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildWSPositionMapTerrainPS()));
	}
};

technique11 BuildNormalDepthMapTerrainTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, TerrainVS()));
		SetHullShader(CompileShader(hs_5_0, TerrainHS()));
		SetDomainShader(CompileShader(ds_5_0, TerrainDS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildNormalDepthMapTerrainPS()));
	}
};

technique11 BuildDepthMapTerrainTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, TerrainVS()));
		SetHullShader(CompileShader(hs_5_0, TerrainHS()));
		SetDomainShader(CompileShader(ds_5_0, TerrainDS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildDepthMapTerrainPS()));
	}
};

technique11 BuildDiffuseMapTerrainTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, TerrainVS()));
		SetHullShader(CompileShader(hs_5_0, TerrainHS()));
		SetDomainShader(CompileShader(ds_5_0, TerrainDS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildDiffuseMapTerrainPS()));
	}
}

technique11 BuildAmbientMapTerrainTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, TerrainVS()));
		SetHullShader(CompileShader(hs_5_0, TerrainHS()));
		SetDomainShader(CompileShader(ds_5_0, TerrainDS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildAmbientMapTerrainPS()));
	}
}

technique11 GenerateGridTerrainTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, TerrainVS()));
		SetHullShader(CompileShader(hs_5_0, TerrainHS()));
		SetDomainShader(CompileShader(ds_5_0, TerrainDS()));
		SetGeometryShader(CompileShader(gs_5_0, GenerateGridTerrainGS()));
		SetPixelShader(CompileShader(ps_5_0, GenerateGridTerrainPS()));
	}
};

technique11 BasicTech
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_5_0, VS() ) );
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
        SetPixelShader( CompileShader( ps_5_0, PS() ) );
	}
};

technique11 BuildShadowMapBasicTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildShadowMapPS()));
	}
};

technique11 BuildWSPositionMapBasicTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildWSPositionMapPS()));
	}
};

technique11 BuildNormalDepthMapBasicTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildNormalDepthMapPS()));
	}
};

technique11 BuildDepthMapBasicTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildDepthMapPS()));
	}
};

technique11 BuildAmbientMapBasicTech
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_5_0, VS() ) );
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
        SetPixelShader( CompileShader( ps_5_0, BuildAmbientMapPS() ) );
	}
};

technique11 BuildDiffuseMapBasicTech
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_5_0, VS() ) );
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
        SetPixelShader( CompileShader( ps_5_0, BuildDiffuseMapPS() ) );
	}
};

technique11 BuildSpecularMapBasicTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildSpecularMapPS()));
	}
};

technique11 BuildPPLLBasicTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildPPLLPS()));
	}
};

technique11 BuildBodiesPPLLBasicTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildBodiesPPLLPS()));
	}
};

technique11 GenerateGridBasicTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(CompileShader(gs_5_0, GenerateGridGS()));
		SetPixelShader(CompileShader(ps_5_0, GenerateGridPS()));
	}
};

technique11 WavesTech
{
    pass P0
    {
		SetVertexShader(CompileShader(vs_5_0, WaterVertexShader()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, WaterPixelShader()));
	}
};

technique11 BuildShadowMapWavesTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, WaterVertexShader()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildShadowMapWaterPixelShader()));
	}
};

technique11 BuildWSPositionMapWavesTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, WaterVertexShader()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildWSPositionMapWaterPixelShader()));
	}
};

technique11 BuildNormalDepthMapWavesTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, WaterVertexShader()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildNormalDepthMapWaterPixelShader()));
	}
};

technique11 BuildDepthMapWavesTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, WaterVertexShader()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildDepthMapWaterPixelShader()));
	}
};

technique11 BuildAmbientMapWavesTech
{
    pass P0
    {
		SetVertexShader(CompileShader(vs_5_0, WaterVertexShader()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildAmbientMapWaterPixelShader()));
	}
};

technique11 BuildDiffuseMapWavesTech
{
    pass P0
    {
		SetVertexShader(CompileShader(vs_5_0, WaterVertexShader()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildDiffuseMapWaterPixelShader()));
	}
};

technique11 DrawScreenQuadTech
{
	pass P0
	{
		SetVertexShader( CompileShader( vs_5_0, VS() ) );
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader( CompileShader( ps_5_0, DrawScreenQuadPS() ) );
	}
};
technique11 TessTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(CompileShader(hs_5_0, HS()));
		SetDomainShader(CompileShader(ds_5_0, DS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, PS()));
	}
};

technique11 BuildShadowMapTessTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(CompileShader(hs_5_0, HS()));
		SetDomainShader(CompileShader(ds_5_0, DS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildShadowMapPS()));
	}
};

technique11 BuildWSPositionMapTessTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(CompileShader(hs_5_0, HS()));
		SetDomainShader(CompileShader(ds_5_0, DS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildWSPositionMapPS()));
	}
};

technique11 BuildNormalDepthMapTessTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(CompileShader(hs_5_0, HS()));
		SetDomainShader(CompileShader(ds_5_0, DS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildNormalDepthMapPS()));
	}
};

technique11 BuildDepthMapTessTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(CompileShader(hs_5_0, HS()));
		SetDomainShader(CompileShader(ds_5_0, DS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildDepthMapPS()));
	}
};

technique11 BuildAmbientMapTessTech
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(CompileShader(hs_5_0, HS()));
		SetDomainShader(CompileShader(ds_5_0, DS()));
		SetGeometryShader(NULL);
        SetPixelShader( CompileShader( ps_5_0, BuildAmbientMapPS() ) );
	}
};

technique11 BuildDiffuseMapTessTech
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(CompileShader(hs_5_0, HS()));
		SetDomainShader(CompileShader(ds_5_0, DS()));
		SetGeometryShader(NULL);
        SetPixelShader( CompileShader( ps_5_0, BuildDiffuseMapPS() ) );
	}
};

technique11 BuildSpecularMapTessTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(CompileShader(hs_5_0, HS()));
		SetDomainShader(CompileShader(ds_5_0, DS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildSpecularMapPS()));
	}
};

technique11 BuildPPLLTessTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(CompileShader(hs_5_0, HS()));
		SetDomainShader(CompileShader(ds_5_0, DS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildPPLLPS()));
	}
};

technique11 BuildBodiesPPLLTessTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(CompileShader(hs_5_0, HS()));
		SetDomainShader(CompileShader(ds_5_0, DS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, BuildBodiesPPLLPS()));
	}
};

technique11 GenerateGridTessTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(CompileShader(hs_5_0, HS()));
		SetDomainShader(CompileShader(ds_5_0, DS()));
		SetGeometryShader(CompileShader(gs_5_0, GenerateGridGS()));
		SetPixelShader(CompileShader(ps_5_0, GenerateGridPS()));
	}
};

technique11 BuildSVOBasicTech {
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(CompileShader(gs_5_0, BuildSVOGS()));
		SetPixelShader(CompileShader(ps_5_0, BuildSVOPS()));
	}
};

technique11 BuildSVOTessTech {
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(CompileShader(hs_5_0, HS()));
		SetDomainShader(CompileShader(ds_5_0, DS()));
		SetGeometryShader(CompileShader(gs_5_0, BuildSVOGS()));
		SetPixelShader(CompileShader(ps_5_0, BuildSVOPS()));
	}
};

technique11 SvoDebugDrawTech {
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, SvoDebugDrawVS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(CompileShader(gs_5_0, SvoDebugDrawGS()));
		SetPixelShader(CompileShader(ps_5_0, SvoDebugDrawPS()));
	}
};