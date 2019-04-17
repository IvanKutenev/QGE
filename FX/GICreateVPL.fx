#include "GIDefinitions.fx"
#include "Definitions.fx"

StructuredBuffer<Voxel>gGridBuffer;
RWStructuredBuffer<VPL>gSHBuffer;

void CalculateSHcoeffs(float3 albedo, uint4 normalMasks, float3 PosW, float3 toEyeW, float PixelDepthV, int gridIndex)
{
	gSHBuffer[gridIndex].redSH = 0.0f;
	gSHBuffer[gridIndex].greenSH = 0.0f;
	gSHBuffer[gridIndex].blueSH = 0.0f;

	Material mat;
	mat.Ambient = 1.0f;
	mat.Diffuse = float4(albedo, 1.0f);
	mat.Specular = 1.0f;
	mat.Reflect = 1.0f;

	float4 dirLightShadowPosH[8][CASCADE_COUNT * MAX_DIR_LIGHTS_NUM];
	[unroll(MAX_DIR_LIGHTS_NUM)]
	for (int j = 0; j < gDirLightsNum; ++j) {
		[unroll(CASCADE_COUNT)]
		for (int i = 0; i < CASCADE_COUNT; ++i) {
			[unroll(8)]
			for(int k = 0; k < 8; ++k)
			{
				dirLightShadowPosH[k][i + j * CASCADE_COUNT] = mul(float4(PosW + gGridCellSizes[gProcessingCascadeIndex] * cornersOffsets[k], 1.0f), gDirLightShadowTransformArray[i + j * CASCADE_COUNT]);
			}
		}
	}

	float4 spotLightShadowPosH[8][MAX_SPOT_LIGHTS_NUM];
	[unroll(MAX_SPOT_LIGHTS_NUM)]
	for (int i = 0; i < gSpotLightsNum; ++i)
	{
		[unroll(8)]
		for(int k = 0; k < 8; ++k)
		{
			spotLightShadowPosH[k][i] = mul(float4(PosW + gGridCellSizes[gProcessingCascadeIndex] * cornersOffsets[k], 1.0f), gSpotLightShadowTransformArray[i]);
		}
	}

	float4 diffuse = 0.0f;
	float4 A, D, S;

	[unroll(MAX_DIR_LIGHTS_NUM)]
	for (int i = 0; i < gDirLightsNum; ++i) {
		float3 NormalW = GetClosestNormal(normalMasks, -gDirLight[i].Direction);
		ComputeDirectionalLight(mat, NormalW,
			toEyeW, i, A, D, S);
		int curCascade = 0;
		[unroll(CASCADE_COUNT)]
		for (int cIndex = 0; cIndex < CASCADE_COUNT; cIndex++)
		{
			if (PixelDepthV < gSplitDistances[cIndex].x)
			{
				curCascade = cIndex;
				break;
			}
		}

		float shadow = 0.0f;
		[unroll(8)]
		for(int k = 0; k < 8; ++k)
		{
			shadow += CalcDirLightShadowFactorNoPCSS(samShadow, gDirLightShadowMapArray, curCascade + i * CASCADE_COUNT, dirLightShadowPosH[k][curCascade + i * CASCADE_COUNT], gDirLight[i].Direction, PosW + gGridCellSizes[gProcessingCascadeIndex] * cornersOffsets[k], NormalW).x;
		}

		if(shadow > 0.0f)
			shadow = 1.0f;

		float4 coeffs = ClampedCosineCoeffs(NormalW);
		gSHBuffer[gridIndex].redSH += coeffs * D.r * shadow;
		gSHBuffer[gridIndex].greenSH += coeffs * D.g * shadow;
		gSHBuffer[gridIndex].blueSH += coeffs * D.b * shadow;
	}

	[unroll(MAX_POINT_LIGHTS_NUM)]
	for (int g = 0; g < gPointLightsNum; ++g) {
		float3 NormalW = GetClosestNormal(normalMasks, gPointLight[g].Position - PosW);
		ComputePointLight(mat, PosW, NormalW,
			toEyeW, g, A, D, S);
		
		float shadow = 0.0f;
		[unroll(8)]
		for(int k = 0; k < 8; ++k)
		{
			shadow += CalcPointLightShadowFactorNoPCSS(samLinear, gPointLightCubicShadowMapArray, g, gPointLight[g].Position, PosW + gGridCellSizes[gProcessingCascadeIndex] * cornersOffsets[k], NormalW).x;
		}
			
		if(shadow > 0.0f)
			shadow = 1.0f;

		float4 coeffs = ClampedCosineCoeffs(NormalW);
		gSHBuffer[gridIndex].redSH += coeffs * D.r * shadow;
		gSHBuffer[gridIndex].greenSH += coeffs * D.g * shadow;
		gSHBuffer[gridIndex].blueSH += coeffs * D.b * shadow;
	}

	[unroll(MAX_SPOT_LIGHTS_NUM)]
	for (int m = 0; m < gSpotLightsNum; ++m) {
		float3 NormalW = GetClosestNormal(normalMasks, gSpotLight[m].Position - PosW);
		ComputeSpotLight(mat, PosW, NormalW,
			toEyeW, m, A, D, S);
		
		float shadow = 0.0f;
		[unroll(8)]
		for(int k = 0; k < 8; ++k)
		{
			shadow += CalcSpotLightShadowFactorNoPCSS(samLinear, gSpotLightShadowMapArray, m, spotLightShadowPosH[k][m], gSpotLight[m].Position, PosW + gGridCellSizes[gProcessingCascadeIndex] * cornersOffsets[k], NormalW).x;
		}

		if(shadow > 0.0f)
			shadow = 1.0f;

		float4 coeffs = ClampedCosineCoeffs(NormalW);
		gSHBuffer[gridIndex].redSH += coeffs * D.r * shadow;
		gSHBuffer[gridIndex].greenSH += coeffs * D.g * shadow;
		gSHBuffer[gridIndex].blueSH += coeffs * D.b * shadow;
	}
}

[numthreads(GI_VOXEL_GRID_RESOLUTION / GI_CS_THREAD_GROUPS_NUMBER,
			GI_VOXEL_GRID_RESOLUTION / GI_CS_THREAD_GROUPS_NUMBER,
			GI_VOXEL_GRID_RESOLUTION / GI_CS_THREAD_GROUPS_NUMBER)]
void CreateVPLCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	int gridIndex = GetGridIndex(dispatchThreadID, gProcessingCascadeIndex);

	Voxel curVoxel = gGridBuffer[gridIndex];
	if(curVoxel.colorMask & 31u == 0)
		return;
	int3 offset = dispatchThreadID - int3(GI_VOXEL_GRID_RESOLUTION / 2, GI_VOXEL_GRID_RESOLUTION / 2, GI_VOXEL_GRID_RESOLUTION / 2);
	float3 PosW = gGridCenterPosW[gProcessingCascadeIndex] + offset * gGridCellSizes[gProcessingCascadeIndex];

	float PixelDepthV = CalculateViewSpaceDepth(PosW);
	float4 toEyeW = CalculateToEyeW(PosW);
	float3 color = DecodeColor(curVoxel.colorMask);
	CalculateSHcoeffs(color, curVoxel.normalMasks, PosW, toEyeW, PixelDepthV, gridIndex);
}

technique11 CreateVPLTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, CreateVPLCS()));
	}
};