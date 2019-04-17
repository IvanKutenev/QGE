#include "GIDefinitions.fx"

StructuredBuffer<Voxel>gGridBuffer;
StructuredBuffer<VPL>gInputSHBuffer;
RWStructuredBuffer<VPL>gOutputSHBuffer;

[numthreads(GI_VOXEL_GRID_RESOLUTION / GI_CS_THREAD_GROUPS_NUMBER,
			GI_VOXEL_GRID_RESOLUTION / GI_CS_THREAD_GROUPS_NUMBER,
			GI_VOXEL_GRID_RESOLUTION / GI_CS_THREAD_GROUPS_NUMBER)]
void PropagateVPLCS(uint3 dispatchThreadID : SV_DispatchThreadID, uniform bool useOcclusion)
{
	int3 voxelPos = dispatchThreadID.xyz;

	float4 sumRedSHCoeffs = gInputSHBuffer[GetGridIndex(voxelPos, gProcessingCascadeIndex)].redSH;
	float4 sumGreenSHCoeffs = gInputSHBuffer[GetGridIndex(voxelPos, gProcessingCascadeIndex)].greenSH;
	float4 sumBlueSHCoeffs = gInputSHBuffer[GetGridIndex(voxelPos, gProcessingCascadeIndex)].blueSH;

	[unroll(6)]
	for(int i = 0; i < 6; ++i)
	{
		int3 samplePos = voxelPos + neighborCellsOffsets[i];

		if((samplePos.x < 0) || (samplePos.x > GI_VOXEL_GRID_RESOLUTION - 1) ||
			(samplePos.y < 0) || (samplePos.y > GI_VOXEL_GRID_RESOLUTION - 1) ||
			(samplePos.z < 0) || (samplePos.z > GI_VOXEL_GRID_RESOLUTION - 1))
			continue;
		
		const float4 neighborRedSHCoeffs = gInputSHBuffer[GetGridIndex(samplePos, gProcessingCascadeIndex)].redSH;
		const float4 neighborGreenSHCoeffs = gInputSHBuffer[GetGridIndex(samplePos, gProcessingCascadeIndex)].greenSH;
		const float4 neighborBlueSHCoeffs = gInputSHBuffer[GetGridIndex(samplePos, gProcessingCascadeIndex)].blueSH;

		float4 occlusionCoeffs = float4(0.0f, 0.0f, 0.0f, 0.0f);

		if(useOcclusion)
		{
			int gridIndex = GetGridIndex(samplePos, gProcessingCascadeIndex);
			Voxel voxel = gGridBuffer[gridIndex];

			if((voxel.colorMask & 31u) != 0)
			{
				float3 occlusionNormal = GetClosestNormal(voxel.normalMasks, -neighborCellsDirections[i]);
				occlusionCoeffs = ClampedCosineCoeffs(occlusionNormal);
			}
		}
		[unroll(6)]
		for(int j = 0; j < 6; ++j)
		{
			float3 neighborCellCenter = neighborCellsDirections[i];
			float3 facePosition = neighborCellsDirections[j] * 0.5f;
			float3 dir = facePosition - neighborCellCenter;
			float len = length(dir);
			dir = normalize(dir);
			
			float solidAngle = 0.0f;
			if(len > 0.5f)
				solidAngle = (len >= 1.5f ? SOLID_ANGLE_A : SOLID_ANGLE_B);
			
			float4 dirSH = SH(dir);

			float3 flux;
			flux.r = dot(neighborRedSHCoeffs, dirSH);
			flux.g = dot(neighborGreenSHCoeffs, dirSH);
			flux.b = dot(neighborBlueSHCoeffs, dirSH);

			flux = max(0.0f, flux) * solidAngle;
			
			if(useOcclusion)
			{
				float occlusion = 1.0f - saturate(dot(occlusionCoeffs, dirSH));
				flux *= occlusion;
			}
			
			float4 coeffs = faceSHCoeffs[j];
			sumRedSHCoeffs += coeffs*flux.r;
			sumGreenSHCoeffs += coeffs*flux.g;
			sumBlueSHCoeffs += coeffs*flux.b;
		}
	}
	gOutputSHBuffer[GetGridIndex(voxelPos, gProcessingCascadeIndex)].redSH = sumRedSHCoeffs;
	gOutputSHBuffer[GetGridIndex(voxelPos, gProcessingCascadeIndex)].greenSH = sumGreenSHCoeffs;
	gOutputSHBuffer[GetGridIndex(voxelPos, gProcessingCascadeIndex)].blueSH = sumBlueSHCoeffs;
}

technique11 NoOcclusionPropagateVPLTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, PropagateVPLCS(false)));
	}
};

technique11 UseOcclusionPropagateVPLTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, PropagateVPLCS(true)));
	}
};