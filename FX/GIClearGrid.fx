#include "GIDefinitions.fx"

RWStructuredBuffer<Voxel>gGridBuffer;

[numthreads(GI_VOXEL_GRID_RESOLUTION / GI_CS_THREAD_GROUPS_NUMBER,
			GI_VOXEL_GRID_RESOLUTION / GI_CS_THREAD_GROUPS_NUMBER,
			GI_VOXEL_GRID_RESOLUTION / GI_CS_THREAD_GROUPS_NUMBER)]
void ClearGridCS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	int3 voxelPos = dispatchThreadID.xyz;
	int gridIdx = GetGridIndex(voxelPos, gProcessingCascadeIndex);

	Voxel gridElement;
	gridElement.colorMask = 0x00000000;
	gridElement.normalMasks = uint4(0, 0, 0, 0);

	gGridBuffer[gridIdx] = gridElement;
}

technique11 ClearGridTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, ClearGridCS()));
	}
};