#include "GPUCollisionDefinitions.fx"

cbuffer cbBuildGrid
{
	float4 gBodiesGridCenterPosW[PARTICLES_GRID_CASCADES_COUNT];
	float4 gBodiesGridCellSizes[PARTICLES_GRID_CASCADES_COUNT];
	int gProcessingCascadeIndex;
	int gBodiesCount;
};

RWStructuredBuffer<MicroParams>gPreIterationParticlesGridRW;

[numthreads(PARTICLES_GRID_RESOLUTION / CS_THREAD_GROUPS_NUMBER,
	PARTICLES_GRID_RESOLUTION / CS_THREAD_GROUPS_NUMBER,
	PARTICLES_GRID_RESOLUTION / CS_THREAD_GROUPS_NUMBER)]
void ClearParticlesGridCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	int3 gridPos = dispatchThreadID.xyz;
	int gridIdx = GetParticlesGridIndex(gridPos, gProcessingCascadeIndex);

	MicroParams gridElement;
	gridElement.BodyIndex = gBodiesCount;
	gridElement.LinearMomentum = 0.0f;
	gridElement.AngularMomentum = 0.0f;

	gPreIterationParticlesGridRW[gridIdx] = gridElement;
}

technique11 ClearParticlesGridTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, ClearParticlesGridCS()));
	}
};

StructuredBuffer<BodiesListNode>gBodiesListBufferRO;
Texture2D<uint>gHeadIndexBufferRO;

[numthreads(PARTICLES_GRID_RESOLUTION / CS_THREAD_GROUPS_NUMBER, PARTICLES_GRID_RESOLUTION / CS_THREAD_GROUPS_NUMBER, PARTICLES_GRID_RESOLUTION / CS_THREAD_GROUPS_NUMBER)]
void PopulateGridCS(int3 dispatchThreadID : SV_DispatchThreadID)
{
	int2 screenXY = dispatchThreadID.xy;
	if (gHeadIndexBufferRO[screenXY] == gBodiesCount)
		return;

	int Indices[PPLL_LAYERS_COUNT];
	int IndicesCount = 0;
	[unroll(PPLL_LAYERS_COUNT)]
	for (int CurIndex = gHeadIndexBufferRO[screenXY]; CurIndex != gBodiesCount && IndicesCount < PPLL_LAYERS_COUNT; CurIndex = gBodiesListBufferRO[CurIndex].NextIndex)
	{
		Indices[IndicesCount] = CurIndex;
		++IndicesCount;
	}

	BodiesListNode Data[PPLL_LAYERS_COUNT];
	for (int i = 0; i < IndicesCount; ++i)
		Data[i] = gBodiesListBufferRO[Indices[i]];

	InsertionSort(Data, IndicesCount);

	float curVoxelMinZ = gBodiesGridCenterPosW[gProcessingCascadeIndex].z + (dispatchThreadID.z - PARTICLES_GRID_RESOLUTION / 2) * gBodiesGridCellSizes[gProcessingCascadeIndex].z;
	float curVoxelMaxZ = curVoxelMinZ + gBodiesGridCellSizes[gProcessingCascadeIndex].z;

	//accumulate thickness for each body in current voxel and write the thickest body
	uint bodiesThickness[MAX_BODIES_COUNT];
	for (int k = 0; k < gBodiesCount; ++k)
		bodiesThickness[k] = 0.0f;
	bool usedBackFace[PPLL_LAYERS_COUNT];
	for (int i = 0; i < IndicesCount; ++i)
	{
		usedBackFace[i] = false;
		Data[i].PosWBodyIndex.w = -Data[i].PosWBodyIndex.w - 1.0f; 
	}
	for (int i = 0; i < IndicesCount; ++i)
	{
		if (Data[i].PosWBodyIndex.w >= 0) // it is frontface
		{
			for (int j = i + 1; j < IndicesCount && round(Data[i].PosWBodyIndex.w) != round(-Data[j].PosWBodyIndex.w - 1.0f); ++j);
			
			if (j >= IndicesCount) //frontface without paired backface
			{
				if(Data[i].PosWBodyIndex.z >= curVoxelMinZ && Data[i].PosWBodyIndex.z <= curVoxelMaxZ)
					bodiesThickness[round(Data[i].PosWBodyIndex.w)] += curVoxelMaxZ - Data[i].PosWBodyIndex.z;
			}
			else //frontface with paired backface
			{
				usedBackFace[j] = true;
				if (Data[j].PosWBodyIndex.z < curVoxelMinZ || Data[i].PosWBodyIndex.z > curVoxelMaxZ)
					;
				else
					bodiesThickness[round(Data[i].PosWBodyIndex.w)] += max(clamp(Data[j].PosWBodyIndex.z, curVoxelMinZ, curVoxelMaxZ) - clamp(Data[i].PosWBodyIndex.z, curVoxelMinZ, curVoxelMaxZ), 0.0f);
			}
		}
		else if (Data[i].PosWBodyIndex.w < 0 && !usedBackFace[i]) //it is backface with no frontface
		{
			if (Data[i].PosWBodyIndex.z > curVoxelMaxZ)
				;
			else
				bodiesThickness[round(-Data[i].PosWBodyIndex.w - 1.0f)] += max(clamp(Data[i].PosWBodyIndex.z, curVoxelMinZ, curVoxelMaxZ) - curVoxelMinZ, 0.0f);
			usedBackFace[i] = true;
		}
	}
	//determine the thickest body in voxel
	float maxThickness = 0.0f;
	uint chosenBodyIndex = gBodiesCount;
	for (int i = 0; i < gBodiesCount; ++i)
	{
		if (maxThickness < bodiesThickness[i])
		{
			chosenBodyIndex = i;
			maxThickness = bodiesThickness[i];
		}
	}

	if (chosenBodyIndex != gBodiesCount) //write body data to voxel
	{
		int gridIdx = GetParticlesGridIndex(dispatchThreadID.xyz, gProcessingCascadeIndex);
		gPreIterationParticlesGridRW[gridIdx].BodyIndex = chosenBodyIndex;
		gPreIterationParticlesGridRW[gridIdx].LinearMomentum = 0.0f;
		gPreIterationParticlesGridRW[gridIdx].AngularMomentum = 0.0f;
	}
}

technique11 PopulateGridTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, PopulateGridCS()));
	}
};