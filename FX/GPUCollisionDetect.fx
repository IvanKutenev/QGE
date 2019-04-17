#include "GPUCollisionDefinitions.fx"

cbuffer cbBuildGrid
{
	float4 gBodiesGridCenterPosW[PARTICLES_GRID_CASCADES_COUNT];
	float4 gBodiesGridCellSizes[PARTICLES_GRID_CASCADES_COUNT];
	int gProcessingCascadeIndex;
	int gBodiesCount;
};

//Detect collisions and compute reactions

bool InGridCascadeBounds(int3 gridPos)
{
	return 0 <= gridPos.x < PARTICLES_GRID_RESOLUTION && 0 <= gridPos.y < PARTICLES_GRID_RESOLUTION && 0 <= gridPos.z < PARTICLES_GRID_RESOLUTION;
}

RWStructuredBuffer<MicroParams>gPostIterationParticlesGridRW;
StructuredBuffer<MicroParams>gPreIterationParticlesGridRO;
StructuredBuffer<MacroParams>gBodiesMacroParametersBufferRO;

static const float gSpringCoeff = 2.0f;
static const float gDampingCoeff = 0.1;
static const float gShearCoeff = 0.001f;
static const float gInteractionLenParam = 1.0f;

[numthreads(PARTICLES_GRID_RESOLUTION / CS_THREAD_GROUPS_NUMBER, PARTICLES_GRID_RESOLUTION / CS_THREAD_GROUPS_NUMBER, PARTICLES_GRID_RESOLUTION / CS_THREAD_GROUPS_NUMBER)]
void DetectCollisionsCS(int3 dispatchThreadID : SV_DispatchThreadID)
{
	int3 particlePos = dispatchThreadID, neighborPos;
	int particleIdx = GetParticlesGridIndex(particlePos, gProcessingCascadeIndex), neighborIdx;

	int particleBodyIdx = gPreIterationParticlesGridRO[particleIdx].BodyIndex, neighborBodyIdx;
	gPostIterationParticlesGridRW[particleIdx].BodyIndex = particleBodyIdx;

	if (particleBodyIdx == gBodiesCount)
		return;

	gPostIterationParticlesGridRW[particleIdx].LinearMomentum = 0.0f;
	gPostIterationParticlesGridRW[particleIdx].AngularMomentum = 0.0f;

	float3 particlePosW = gBodiesGridCenterPosW[gProcessingCascadeIndex].xyz + (particlePos - PARTICLES_GRID_RESOLUTION / 2) * gBodiesGridCellSizes[gProcessingCascadeIndex].xyz;
	float3 particleRelPosW = particlePosW - gBodiesMacroParametersBufferRO[particleBodyIdx].CMPosW;
	float3 particleSpeed = gBodiesMacroParametersBufferRO[particleBodyIdx].Speed + cross(gBodiesMacroParametersBufferRO[particleBodyIdx].AngularSpeed, particleRelPosW);

	for (int i = -1; i <= 1; ++i)
	{
		for (int j = -1; j <= 1; ++j)
		{
			for (int k = -1; k <= 1; ++k)
			{
				neighborPos = particlePos + int3(i, j, k);
				if (InGridCascadeBounds(neighborPos))
				{
					neighborIdx = GetParticlesGridIndex(neighborPos, gProcessingCascadeIndex);
					neighborBodyIdx = gPreIterationParticlesGridRO[neighborIdx].BodyIndex;

					if (neighborBodyIdx != particleBodyIdx &&
						neighborBodyIdx != gBodiesCount)
					{
						float3 neighborPosW = gBodiesGridCenterPosW[gProcessingCascadeIndex].xyz + (neighborPos - PARTICLES_GRID_RESOLUTION / 2) * gBodiesGridCellSizes[gProcessingCascadeIndex].xyz;
						//repulsive force
						float d = gBodiesGridCellSizes[gProcessingCascadeIndex].x / gInteractionLenParam;
						float k = gSpringCoeff;
						float3 r_p1_p2 = particlePosW - neighborPosW;
						float3 f_rep = -k * (d - length(r_p1_p2)) * r_p1_p2 / length(r_p1_p2 + 0.0000001f);
						//damping force
						float n = gDampingCoeff;
						float3 neighborRelPosW = neighborPosW - gBodiesMacroParametersBufferRO[neighborBodyIdx].CMPosW;
						float3 neighborSpeed = gBodiesMacroParametersBufferRO[neighborBodyIdx].Speed + cross(gBodiesMacroParametersBufferRO[neighborBodyIdx].AngularSpeed, neighborRelPosW);
						float3 v_p1_p2 = -particleSpeed + neighborSpeed;
						float3 f_damp = gDampingCoeff * v_p1_p2;
						//shear force
						float3 v_p1_p2_t = v_p1_p2 - dot(v_p1_p2, r_p1_p2 / length(r_p1_p2 + 0.0000001f)) * r_p1_p2 / length(r_p1_p2 + 0.0000001f);
						float k_t = gShearCoeff;
						float3 f_shear = k_t * v_p1_p2_t;
						gPostIterationParticlesGridRW[particleIdx].LinearMomentum += (f_rep + f_damp + f_shear) / gBodiesMacroParametersBufferRO[particleBodyIdx].Mass;
						gPostIterationParticlesGridRW[particleIdx].AngularMomentum += cross(particleRelPosW, f_rep + f_damp + f_shear) / gBodiesMacroParametersBufferRO[particleBodyIdx].InertionMomentum;
					}
				}
			}
		}
	}
}

technique11 DetectCollisionsTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, DetectCollisionsCS()));
	}
};