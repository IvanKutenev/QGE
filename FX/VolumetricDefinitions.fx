#include "GIDefinitions.fx"
#include "Definitions.fx"

#define DIR 0
#define SPOT 1
#define POINT 2

#define NUM_THREADS_X 8
#define NUM_THREADS_Y 8

static const float gInfinity = 999999.0f;
static const float gStepCorrectionCoeff = 1.1f;
static const float gAttenuation = 0.004f;
static const float gStepPrecisionCoeff = 0.25f;

struct LightingData
{
	uint4 lighting[12]; //RGBA packed into uint, 4 lights into uint4, set of 16 lights into uint4[3]
};

cbuffer cbVolEffect
{
	float gClientW;
	float gClientH;
	float4x4 gInvProj;
	float4x4 gInvView;
};

struct FogVertexIn
{
	float3 PosL : POSITION;
	float2 Tex : TEXCOORD;
};

struct FogVertexOut
{
	float4 PosH : SV_POSITION;
	float2 Tex : TEXCOORD0;
	float4 PosSS : TEXCOORD1;
};

int ChooseCascade(float3 PosW) //choose smallest cascade with PosW in it
{
	for (int curCascade = 0; curCascade < GI_CASCADES_COUNT; ++curCascade)
		if (length(PosW - gGridCenterPosW[curCascade]) < (GI_VOXEL_GRID_RESOLUTION / 2) * gGridCellSizes[curCascade].x)
			return curCascade;
	return GI_CASCADES_COUNT;
}

uint PackFloats(float v1, float v2, float v3, float v4)
{
	uint i_v1 = floor(v1 * 255.0f);
	uint i_v2 = floor(v2 * 255.0f);
	uint i_v3 = floor(v3 * 255.0f);
	uint i_v4 = floor(v4 * 255.0f);
	return (i_v1 << 24) | (i_v2 << 16) | (i_v3 << 8) | i_v4;
}

float4 UnpackFloats(uint pack)
{
	float4 retval;
	retval.x = (pack >> 24) & 0x000000ff;
	retval.y = (pack >> 16) & 0x000000ff;
	retval.z = (pack >> 8) & 0x000000ff;
	retval.w = pack & 0x000000ff;

	retval /= 255.0f;

	return retval;
}


//Example of shader usage: clouds rendering////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//TODO: manage all constants

SamplerState samDensity
{
	Filter = ANISOTROPIC;
	MaxAnisotropy = 16;

	AddressU = MIRROR;
	AddressV = MIRROR;
};

Texture2D<float4>gDistributionDensityMap;
Texture2D<float4>gWorleyNoiseMap;
Texture2D<float4>gPerlinNoiseMap;

static const float gCloudsDistribDensityMapScale = 1000.0f;
static const float2 gCloudsVelocity = float2(1000.0f, 1000.0f);
static const float gMarchVolBoundMin = 0.0f;
static const float gMarchVolBoundMax = 1000.0f;
static const float gCloudsLowerBound = 500.0f;
static const float gCloudsHigherBound = 1000.0f;

bool IsInYBoundary(float3 ray_origin, float3 ray_dir)
{
	if (ray_origin.y > gMarchVolBoundMax && ray_dir.y > 0 || (ray_origin.y < gMarchVolBoundMin && ray_dir.y < 0))
		return false;
	return true;
}

//TODO: sample albedo from 3D-texture or other buffer
float4 SampleAlbedo(float3 PosW)
{
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}

//TODO: add your density texture sampler function
float SampleCloudsDensityFromTexture(float3 PosW, int cascadeIndex)
{
	float cloudsDensityMultiplier = 30.0f;
	float cloudsThickness = gCloudsHigherBound - gCloudsLowerBound;
	//sample horizontal density distribution
	float2 cloudsDistribSamplePos = (PosW.xz + gTime * gCloudsVelocity) / gCloudsDistribDensityMapScale;
	float cloudsHorzSampledDensity = gDistributionDensityMap.SampleLevel(samDensity, cloudsDistribSamplePos, cascadeIndex).x;
	//sample vertical density distribution
	float2 cloudsHeightSamplePosXY = float2(PosW.x + gTime * gCloudsVelocity.x, gCloudsHigherBound - clamp(PosW.y, gCloudsLowerBound, gCloudsHigherBound)) / cloudsThickness;
	float2 cloudsHeightSamplePosZY = float2(PosW.z + gTime * gCloudsVelocity.y, gCloudsHigherBound - clamp(PosW.y, gCloudsLowerBound, gCloudsHigherBound)) / cloudsThickness;
	float2 cloudsHeightSamplePosXZ = float2(PosW.x + gTime * gCloudsVelocity.x, PosW.z + gTime * gCloudsVelocity.y) / cloudsThickness;
	float cloudsHeightSampledDensityXY = (gPerlinNoiseMap.SampleLevel(samDensity, cloudsHeightSamplePosXY, cascadeIndex).x * 0.7f + 0.3f) * (gWorleyNoiseMap.SampleLevel(samDensity, cloudsHeightSamplePosXY, cascadeIndex).x * 0.7f + 0.3f);
	float cloudsHeightSampledDensityZY = (gPerlinNoiseMap.SampleLevel(samDensity, cloudsHeightSamplePosZY, cascadeIndex).x * 0.7f + 0.3f) * (gWorleyNoiseMap.SampleLevel(samDensity, cloudsHeightSamplePosZY, cascadeIndex).x * 0.7f + 0.3f);
	float cloudsHeightSampledDensityXZ = (gPerlinNoiseMap.SampleLevel(samDensity, cloudsHeightSamplePosXZ, cascadeIndex).x * 0.7f + 0.3f) * (gWorleyNoiseMap.SampleLevel(samDensity, cloudsHeightSamplePosXZ, cascadeIndex).x * 0.7f + 0.3f);
	
	return cloudsHeightSampledDensityXZ * cloudsHeightSampledDensityZY * cloudsHeightSampledDensityXY * cloudsHorzSampledDensity * cloudsDensityMultiplier;
}

//TODO: add your analytical density function
float SampleCloudsDistributionFunc(float3 PosW)
{
	if (PosW.y > gCloudsLowerBound && PosW.y < gCloudsHigherBound)
	{
		float x = (PosW.y - gCloudsLowerBound) / (gCloudsHigherBound - gCloudsLowerBound);
		return (1 - exp(-50.0f * x)) * exp(-4.0f * x);
	}
	else
		return 0.0f;
}

//TODO: sample density from 3D-texture or other buffer
float SampleDensity(float3 PosW, int cascadeIndex)
{
	//clouds
	float clouds_density = SampleCloudsDistributionFunc(PosW) * SampleCloudsDensityFromTexture(PosW, cascadeIndex);
	if (clouds_density < 0.2f)
		clouds_density = 0.0f;
	//fog
	float fog_density = 0.002f;
	return fog_density + clouds_density;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float4 SampleLightingBufferCascade(int3 curVoxel, float3 samplePos, StructuredBuffer<LightingData>lightingBuffer, int cascadeIndex, int lightIndex, int lightType)
{
	float4 lighting[7];
	int3 lerpVoxel = sign(samplePos - curVoxel) * int3(1, 1, 1) + curVoxel;
	float3 lerpParam = abs(lerpVoxel - samplePos);

	int lerpFrom[4] = {
		GetGridIndex(int3(lerpVoxel.x, curVoxel.y, curVoxel.z), cascadeIndex),
		GetGridIndex(int3(lerpVoxel.x, curVoxel.y, lerpVoxel.z), cascadeIndex),
		GetGridIndex(int3(lerpVoxel.x, lerpVoxel.y, curVoxel.z), cascadeIndex),
		GetGridIndex(int3(lerpVoxel.x, lerpVoxel.y, lerpVoxel.z), cascadeIndex)
	};

	int lerpTo[4] = {
		GetGridIndex(int3(curVoxel.x, curVoxel.y, curVoxel.z), cascadeIndex),
		GetGridIndex(int3(curVoxel.x, curVoxel.y, lerpVoxel.z), cascadeIndex),
		GetGridIndex(int3(curVoxel.x, lerpVoxel.y, curVoxel.z), cascadeIndex),
		GetGridIndex(int3(curVoxel.x, lerpVoxel.y, lerpVoxel.z), cascadeIndex)
	};

	[unroll(4)]
	for (int i = 0; i < 4; ++i)
		lighting[i] = lerp(UnpackFloats(lightingBuffer[lerpFrom[i]].lighting[lightType * 4 + lightIndex / 4][lightIndex % 4]),
						   UnpackFloats(lightingBuffer[lerpTo[i]].lighting[lightType * 4 + lightIndex / 4][lightIndex % 4]),
						   lerpParam.x);
	[unroll(2)]
	for (int i = 4; i < 6; ++i)
		lighting[i] = lerp(lighting[i * 2 - 7], lighting[i * 2 - 8], lerpParam.z);

	lighting[6] = lerp(lighting[5], lighting[4], lerpParam.y);
	return lighting[6];
}

float4 SampleLightingBuffer(float3 PosW, StructuredBuffer<LightingData>lightingBuffer, int cascadeIndex, int lightIndex, int lightType)
{
	int3 curVoxel, nextCascadeVoxel;
	float3 samplePos, nextCascadeSamplePos;
	CalculateGridPositions(PosW, cascadeIndex, samplePos, curVoxel);
	if (cascadeIndex >= GI_CASCADES_COUNT - 1)
		return SampleLightingBufferCascade(curVoxel, samplePos, lightingBuffer, cascadeIndex, lightIndex, lightType);
	CalculateGridPositions(PosW, cascadeIndex + 1, nextCascadeSamplePos, nextCascadeVoxel);

	float4 curCascadeL = SampleLightingBufferCascade(curVoxel, samplePos, lightingBuffer, cascadeIndex, lightIndex, lightType);
	float4 nextCascadeL = SampleLightingBufferCascade(nextCascadeVoxel, nextCascadeSamplePos, lightingBuffer, cascadeIndex + 1, lightIndex, lightType);

	float3 offset = abs(CalculateOffset(PosW, cascadeIndex));
	offset /= GI_VOXEL_GRID_RESOLUTION / 2;
	offset -= 0.5f;
	offset = 2.0f * saturate(offset);

	float4 retL;
	retL = lerp(curCascadeL, nextCascadeL, offset.x);
	retL = lerp(retL, nextCascadeL, offset.y);
	retL = lerp(retL, nextCascadeL, offset.z);
	return retL;
}

float AccumulateDensityToDirLight(float3 PosW, int DirLightIndex)
{
	float3 toLightDir = normalize(-gDirLight[DirLightIndex].Direction);
	float3 CurPosW = PosW;

	int SamplingCascade = 0;
	float AccumulatedDensity = SampleDensity(CurPosW, SamplingCascade);
	while (1)
	{
		int testCascade = ChooseCascade(CurPosW + gGridCellSizes[SamplingCascade].x * toLightDir);
		if (testCascade == GI_CASCADES_COUNT)
			break;
		if (SamplingCascade != testCascade)
			SamplingCascade = testCascade;

		CurPosW = CurPosW + gGridCellSizes[SamplingCascade].x * toLightDir;

		float NextVoxelDensity = SampleDensity(CurPosW, SamplingCascade);
		NextVoxelDensity *= gGridCellSizes[SamplingCascade].x * gInvertedGridCellSizes[0].x;
		AccumulatedDensity += NextVoxelDensity;
	}
	return AccumulatedDensity;
}

float AccumulateDensityToPointLight(float3 PosW, int PointLightIndex)
{
	float3 toLightDir = normalize(gPointLight[PointLightIndex].Position - PosW);
	float3 CurPosW = PosW + toLightDir * EPSILON;

	int SamplingCascade = 0;
	float AccumulatedDensity = SampleDensity(CurPosW, SamplingCascade);
	while (dot(normalize(CurPosW - PosW), normalize(gPointLight[PointLightIndex].Position - PosW)) > 0)
	{
		int testCascade = ChooseCascade(CurPosW + gGridCellSizes[SamplingCascade].x * toLightDir);
		if (testCascade == GI_CASCADES_COUNT)
			break;
		if (SamplingCascade != testCascade)
			SamplingCascade = testCascade;

		CurPosW = CurPosW + gGridCellSizes[SamplingCascade].x * toLightDir;

		float NextVoxelDensity = SampleDensity(CurPosW, SamplingCascade);
		NextVoxelDensity *= gGridCellSizes[SamplingCascade].x * gInvertedGridCellSizes[0].x;
		AccumulatedDensity += NextVoxelDensity;
	}
	return AccumulatedDensity;
}

float AccumulateDensityToSpotLight(float3 PosW, int SpotLightIndex)
{
	float3 toLightDir = normalize(gSpotLight[SpotLightIndex].Position - PosW);
	float3 CurPosW = PosW + toLightDir * EPSILON;

	int SamplingCascade = 0;
	float AccumulatedDensity = SampleDensity(CurPosW, SamplingCascade);
	while (dot(normalize(CurPosW - PosW), normalize(gSpotLight[SpotLightIndex].Position - PosW)) > 0)
	{
		int testCascade = ChooseCascade(CurPosW + gGridCellSizes[SamplingCascade].x * toLightDir);
		if (testCascade == GI_CASCADES_COUNT)
			break;
		if (SamplingCascade != testCascade)
			SamplingCascade = testCascade;

		CurPosW = CurPosW + gGridCellSizes[SamplingCascade].x * toLightDir;

		float NextVoxelDensity = SampleDensity(CurPosW, SamplingCascade);
		NextVoxelDensity *= gGridCellSizes[SamplingCascade].x * gInvertedGridCellSizes[0].x;
		AccumulatedDensity += NextVoxelDensity;
	}
	return AccumulatedDensity;
}

float4 CalculateDirectIllumination(float3 PosW, StructuredBuffer<LightingData>lightingBuffer, int cascadeIndex, float4 albedo)
{
	float PixelDepthV = CalculateViewSpaceDepth(PosW);

	float4 dirLightShadowPosH[CASCADE_COUNT * MAX_DIR_LIGHTS_NUM];
	for (int j = 0; j < gDirLightsNum; ++j)
		for (int i = 0; i < CASCADE_COUNT; ++i)
			dirLightShadowPosH[i + j * CASCADE_COUNT] = mul(float4(PosW, 1.0f), gDirLightShadowTransformArray[i + j * CASCADE_COUNT]);

	float4 spotLightShadowPosH[MAX_SPOT_LIGHTS_NUM];
	for (int i = 0; i < gSpotLightsNum; ++i)
		spotLightShadowPosH[i] = mul(float4(PosW, 1.0f), gSpotLightShadowTransformArray[i]);

	//TODO: add ambient term
	float4 diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);

	for (int i = 0; i < gDirLightsNum; ++i) {
		int curCascade = 0;
		for (int cIndex = 0; cIndex < CASCADE_COUNT; cIndex++)
		{
			if (PixelDepthV < gSplitDistances[cIndex].x)
			{
				curCascade = cIndex;
				break;
			}
		}
		float3 NormalW = -gDirLight[i].Direction;
		float shadow = CalcDirLightShadowFactor(samShadow, gDirLightShadowMapArray, curCascade + i * CASCADE_COUNT, dirLightShadowPosH[curCascade + i * CASCADE_COUNT], gDirLight[i].Direction, PosW, NormalW).x;
		diffuse += shadow * SampleLightingBuffer(PosW, lightingBuffer, cascadeIndex, i, DIR);
	}
	for (int g = 0; g < gPointLightsNum; ++g) {
		float3 NormalW = gPointLight[g].Position - PosW;
		float shadow = CalcPointLightShadowFactor(samLinear, gPointLightCubicShadowMapArray, g, gPointLight[g].Position, PosW, NormalW).x;
		diffuse += shadow * SampleLightingBuffer(PosW, lightingBuffer, cascadeIndex, g, POINT);
	}
	for (int m = 0; m < gSpotLightsNum; ++m) {
		float3 NormalW = gSpotLight[m].Position - PosW;
		float shadow = CalcSpotLightShadowFactor(samLinear, gSpotLightShadowMapArray, m, spotLightShadowPosH[m], gSpotLight[m].Position, PosW, NormalW).x;
		diffuse += shadow * SampleLightingBuffer(PosW, lightingBuffer, cascadeIndex, m, SPOT);
	}
	return diffuse;
}


