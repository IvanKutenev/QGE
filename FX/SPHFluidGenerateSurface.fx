#include "SPHFluidDefinitions.fx"

[numthreads(NUM_THREADS, 1, 1)]
void ClearSurfaceBufferCS(int3 dispatchThreadId : SV_DispatchThreadId)
{
	int idx = dispatchThreadId.x;
	for (int i = 0; i < 5; ++i)
	{
		gFluidSurfaceRW[idx * 5 + i].v[0].Flag = -1.0f;
		gFluidSurfaceRW[idx * 5 + i].v[1].Flag = -1.0f;
		gFluidSurfaceRW[idx * 5 + i].v[2].Flag = -1.0f;
	}
}

[numthreads(MC_GRID_SIDE_SZ / MC_GRID_BUILD_THREAD_GROUP_COUNT,
	MC_GRID_SIDE_SZ / MC_GRID_BUILD_THREAD_GROUP_COUNT,
	MC_GRID_SIDE_SZ / MC_GRID_BUILD_THREAD_GROUP_COUNT)]
void GenSurfaceCS(int3 dispatchThreadId : SV_DispatchThreadId)
{
	float3 cellPos = gGridOrigin + (dispatchThreadId - MC_GRID_SIDE_SZ / 2.0f) * gGridCellSize;
	//get density value for each corner
	float densityValues[8];
	float3 cornersPos[8];
	float3 cornersNormal[8];
	[loop]
	for (int i = 0; i < 8; ++i)
	{
		cornersPos[i] = cellPos + gCellCornersOffsets[i] * gGridCellSize / 2.0f;
		cornersNormal[i] = GetNormalAtPos(cornersPos[i]);
		densityValues[i] = GetDensityAtPos(cornersPos[i]);
	}
	
	//get cube index
	uint cubeIndex = 0;
	for (int i = 0; i < 8; ++i)
	{
		if (densityValues[i] < gDensityIsoValue)
			cubeIndex |= (1 << i);
	}
	//get edge flag
	uint edgeFlag = gEdgeTable[cubeIndex];
	//interpolate vertices on edges
	FluidVertex v[12];
	for (int i = 0; i < 12; ++i)
	{
		if (edgeFlag & (1 << i) != 0)
		{
			int v0Idx = gEdgesCornersIndices[i][0];
			int v1Idx = gEdgesCornersIndices[i][1];
			float s = (gDensityIsoValue - densityValues[v0Idx]) / (densityValues[v1Idx] - densityValues[v0Idx]);
			v[i].Pos = lerp(cornersPos[v0Idx], cornersPos[v1Idx], s);
			v[i].Normal = lerp(cornersNormal[v0Idx], cornersNormal[v1Idx], s);
			v[i].Flag = 1.0f;
			v[i].Tangent = 1.0f;
		}
	}
	uint counter = 0;
	FluidTriangle t;
	//append resulting triangles
	for (int i = 0; gTriangleVerticesTable[cubeIndex][i] != -1; i += 3)
	{
		t.v[0] = v[gTriangleVerticesTable[cubeIndex][i]];
		t.v[1] = v[gTriangleVerticesTable[cubeIndex][i + 1]];
		t.v[2] = v[gTriangleVerticesTable[cubeIndex][i + 2]];
		counter = gFluidSurfaceRW.IncrementCounter();
		gFluidSurfaceRW[counter] = t;
	}
}

technique11 SPHClearSurfaceBufferTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, ClearSurfaceBufferCS()));
	}
};


technique11 SPHFluidGenerateSurfaceTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, GenSurfaceCS()));
	}
};

