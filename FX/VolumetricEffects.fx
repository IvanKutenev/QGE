#include "VolumetricDefinitions.fx"

RWStructuredBuffer<LightingData>gLightingBufferRW;

[numthreads(GI_VOXEL_GRID_RESOLUTION / GI_CS_THREAD_GROUPS_NUMBER,
	GI_VOXEL_GRID_RESOLUTION / GI_CS_THREAD_GROUPS_NUMBER,
	GI_VOXEL_GRID_RESOLUTION / GI_CS_THREAD_GROUPS_NUMBER)]
void ComputeLightingCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	int gridIndex = GetGridIndex(dispatchThreadID, gProcessingCascadeIndex);

	int3 offset = dispatchThreadID - int3(GI_VOXEL_GRID_RESOLUTION / 2, GI_VOXEL_GRID_RESOLUTION / 2, GI_VOXEL_GRID_RESOLUTION / 2);
	float3 PosW = gGridCenterPosW[gProcessingCascadeIndex] + offset * gGridCellSizes[gProcessingCascadeIndex];

	Material mat;
	mat.Ambient = 1.0f;
	mat.Diffuse = SampleAlbedo(PosW);
	mat.Specular = 1.0f;
	mat.Reflect = 1.0f;

	float4 A, D, S;
	float4 toEyeW = CalculateToEyeW(PosW);
	float4 lighting[3][16];

	//TODO: turn off accumulating density for global fog rendering or optimization and turn on for small volumes
	for (int i = 0; i < gDirLightsNum; ++i) {
		float3 NormalW = -gDirLight[i].Direction;
		ComputeDirectionalLight(mat, NormalW,
			toEyeW, i, A, D, S);
		lighting[DIR][i] = saturate(D * exp(-AccumulateDensityToDirLight(PosW, i)));
	}
	for (int i = gDirLightsNum; i < 16; ++i) {
		lighting[DIR][i] = float4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	
	for (int g = 0; g < gPointLightsNum; ++g) {
		float3 NormalW = gPointLight[g].Position - PosW;
		ComputePointLight(mat, PosW, NormalW,
			toEyeW, g, A, D, S);
		lighting[POINT][g] = saturate(D * exp(-AccumulateDensityToPointLight(PosW, g)));
	}
	for (int g = gPointLightsNum; g < 16; ++g) {
		lighting[POINT][g] = float4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	for (int m = 0; m < gSpotLightsNum; ++m) {
		float3 NormalW = gSpotLight[m].Position - PosW;
		ComputeSpotLight(mat, PosW, NormalW,
			toEyeW, m, A, D, S);
		lighting[SPOT][m] = saturate(D * exp(-AccumulateDensityToSpotLight(PosW, m)));
	}
	for (int m = gSpotLightsNum; m < 16; ++m) {
		lighting[SPOT][m] = float4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	for (int k = 0; k < 12; ++k)
	{
		gLightingBufferRW[gridIndex].lighting[k] = uint4(
			PackFloats(lighting[k / 4][0 + (k % 4) * 4].r, lighting[k / 4][0 + (k % 4) * 4].g, lighting[k / 4][0 + (k % 4) * 4].b, lighting[k / 4][0 + (k % 4) * 4].a),
			PackFloats(lighting[k / 4][1 + (k % 4) * 4].r, lighting[k / 4][1 + (k % 4) * 4].g, lighting[k / 4][1 + (k % 4) * 4].b, lighting[k / 4][1 + (k % 4) * 4].a),
			PackFloats(lighting[k / 4][2 + (k % 4) * 4].r, lighting[k / 4][2 + (k % 4) * 4].g, lighting[k / 4][2 + (k % 4) * 4].b, lighting[k / 4][2 + (k % 4) * 4].a),
			PackFloats(lighting[k / 4][3 + (k % 4) * 4].r, lighting[k / 4][3 + (k % 4) * 4].g, lighting[k / 4][3 + (k % 4) * 4].b, lighting[k / 4][3 + (k % 4) * 4].a));
	}
}

technique11 ComputeLightingTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, ComputeLightingCS()));
	}
};

Texture2D<float4>gPosWMap;
StructuredBuffer<LightingData>gLightingBufferRO;
RWTexture2D<float4>gOutputFogColorRW;

////////////////////////////////////////////////////////////////////////////////////
////TODO: add ambient and henyey-greenstein function////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

[numthreads(NUM_THREADS_X, NUM_THREADS_Y, 1)]
void AccumulateScatteringCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	float2 PosSS = float2(dispatchThreadID.x / gClientW, dispatchThreadID.y / gClientH);
	float3 PosW = gPosWMap.SampleLevel(samPointClamp, PosSS.xy, 0.0f);

	if (PosW.x >= gInfinity &&
		PosW.y >= gInfinity &&
		PosW.z >= gInfinity)
	{
		float4 PosH;
		PosH.x = 2.0f * PosSS.x - 1.0f;
		PosH.y = -2.0f * PosSS.y + 1.0;
		PosH.z = 1.0f;
		PosH.w = 1.0f;
		float3 PosV = mul(PosH, gInvProj).xyz / mul(PosH, gInvProj).w;
		PosW = mul(float4(PosV, 1.0f), gInvView);
	}

	float DepthV = CalculateViewSpaceDepth(PosW);

	float3 viewDir = normalize(PosW - gEyePosW);
	float3 CurPosW = gEyePosW;
	float CurDepthV = CalculateViewSpaceDepth(CurPosW);

	int SamplingCascade = 0;

	float AccumulatedDensity = SampleDensity(CurPosW, SamplingCascade) * gStepPrecisionCoeff;
	float4 AccumulatedDirectLight = CalculateDirectIllumination(CurPosW, gLightingBufferRO, 0, SampleAlbedo(CurPosW));
	while (CurDepthV < DepthV)
	{
		int testCascade = ChooseCascade(CurPosW + gGridCellSizes[SamplingCascade].x * gStepPrecisionCoeff * viewDir);
		if (testCascade == GI_CASCADES_COUNT)
			break;
		if (SamplingCascade != testCascade)
			SamplingCascade = testCascade;

		CurPosW = CurPosW + gGridCellSizes[SamplingCascade].x * gStepPrecisionCoeff * viewDir;

		CurDepthV = CalculateViewSpaceDepth(CurPosW);
		if (CurDepthV >= DepthV)
			break;

		float NextVoxelDensity = SampleDensity(CurPosW, SamplingCascade) * gStepPrecisionCoeff * gGridCellSizes[SamplingCascade].x * gInvertedGridCellSizes[0].x;

		AccumulatedDirectLight += max(saturate(exp(-AccumulatedDensity)) *  CalculateDirectIllumination(CurPosW, gLightingBufferRO, SamplingCascade, SampleAlbedo(CurPosW)), 0.0f);
		AccumulatedDensity += NextVoxelDensity;

		if (!IsInYBoundary(CurPosW, viewDir))
			break;
	}

	float3 inScatteredLight = AccumulatedDirectLight.rgb;
	float Transmittance = saturate(exp(-AccumulatedDensity));
	gOutputFogColorRW[uint2(dispatchThreadID.x, dispatchThreadID.y)] = float4(inScatteredLight * gAttenuation, Transmittance);
}

technique11 AccumulateScatteringTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, AccumulateScatteringCS()));
	}
};

Texture2D<float4>gOutputFogColorRO;

FogVertexOut VS(FogVertexIn vin)
{
	FogVertexOut vout;
	vout.PosH = float4(vin.PosL, 1.0f);
	vout.PosSS = mul(vout.PosH, toTexSpace);
	vout.Tex = vin.Tex;
	return vout;
}

float4 PS(FogVertexOut pin) : SV_TARGET
{
	pin.PosSS /= pin.PosSS.w;
	return gOutputFogColorRO.SampleLevel(samPointClamp, pin.PosSS.xy, 0.0f);
}

technique11 DrawFogTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, PS()));
	}
}