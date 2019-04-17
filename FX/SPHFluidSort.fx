//--------------------------------------------------------------------------------------
// File: ComputeShaderSort11.hlsl
//
// This file contains the compute shaders to perform GPU sorting using DirectX 11.
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define BITONIC_BLOCK_SIZE 1024
#define TRANSPOSE_BLOCK_SIZE 32

//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
cbuffer CB
{
	uint gLevel;
	uint gLevelMask;
	uint gWidth;
	uint gHeight;
};

//--------------------------------------------------------------------------------------
// Structured Buffers
//--------------------------------------------------------------------------------------
StructuredBuffer<uint2> Input;
RWStructuredBuffer<uint2> Data;

//--------------------------------------------------------------------------------------
// Bitonic Sort Compute Shader
//--------------------------------------------------------------------------------------
groupshared uint2 shared_data[BITONIC_BLOCK_SIZE];

[numthreads(BITONIC_BLOCK_SIZE, 1, 1)]
void BitonicSort(uint3 Gid : SV_GroupID,
	uint3 DTid : SV_DispatchThreadID,
	uint3 GTid : SV_GroupThreadID,
	uint GI : SV_GroupIndex)
{
	// Load shared data
	shared_data[GI] = Data[DTid.x];
	GroupMemoryBarrierWithGroupSync();

	// Sort the shared data
	for (uint j = gLevel >> 1; j > 0; j >>= 1)
	{
		uint2 result = ((shared_data[GI & ~j].x <= shared_data[GI | j].x) == (bool)(gLevelMask & DTid.x)) ? shared_data[GI ^ j] : shared_data[GI];
		GroupMemoryBarrierWithGroupSync();
		shared_data[GI] = result;
		GroupMemoryBarrierWithGroupSync();
	}

	// Store shared data
	Data[DTid.x] = shared_data[GI];
}

//--------------------------------------------------------------------------------------
// Matrix Transpose Compute Shader
//--------------------------------------------------------------------------------------
groupshared uint2 transpose_shared_data[TRANSPOSE_BLOCK_SIZE * TRANSPOSE_BLOCK_SIZE];

[numthreads(TRANSPOSE_BLOCK_SIZE, TRANSPOSE_BLOCK_SIZE, 1)]
void MatrixTranspose(uint3 Gid : SV_GroupID,
	uint3 DTid : SV_DispatchThreadID,
	uint3 GTid : SV_GroupThreadID,
	uint GI : SV_GroupIndex)
{
	transpose_shared_data[GI] = Input[DTid.y * gWidth + DTid.x];
	GroupMemoryBarrierWithGroupSync();
	uint2 XY = DTid.yx - GTid.yx + GTid.xy;
	Data[XY.y * gHeight + XY.x] = transpose_shared_data[GTid.x * TRANSPOSE_BLOCK_SIZE + GTid.y];
}

technique11 BitonicSortTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, BitonicSort()));
	}
}

technique11 MatrixTransposeTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, MatrixTranspose()));
	}
};
