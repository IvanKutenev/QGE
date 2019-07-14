#include "SVODefinitions.fx"

cbuffer cbSvoPerFrame {
	float4x4 gInvProj;
	float4x4 gInvView;
	float3 gEyePosW;
	float gClientW;
	float gClientH;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////OCTREE RESET//////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

[numthreads(RESET_SVO_NUM_THREADS, 1, 1)]
void ResetRootNodeCS(int3 dispatchThreadID : SV_DispatchThreadID)
{
	int idx = dispatchThreadID.x;

	[unroll(8)]
	for (int i = 0; i < 8; ++i)
	{
		gNodesPoolRW[idx].childNodesIds[i] = NO_CHILD;
	}

	if (idx == 0)
	{
		gNodesPoolRW[idx].PosW = gSvoRootNodeCenterPosW;
		gNodesPoolRW[idx].svoLevelIdx = 0;
		gNodesPoolRW[idx].voxelBlockStartIdx = 0;
		gNodesPoolRW[idx].parentNodeIdx = NO_PARENT;
	}
	else
	{
		gNodesPoolRW[idx].PosW.xyz = 0.0;
		gNodesPoolRW[idx].svoLevelIdx = SVO_LEVELS_COUNT;
		gNodesPoolRW[idx].voxelBlockStartIdx = -1;
		gNodesPoolRW[idx].parentNodeIdx = NO_PARENT;
	}
}

technique11 ResetRootNodeTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, ResetRootNodeCS()));
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////RAY CASTING///////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RWTexture2D<float4>gRayCastedOutputRW;

[numthreads(RAY_CASTER_NUM_THREADS, RAY_CASTER_NUM_THREADS, 1)]
void CastRaysCS(int3 dispatchThreadID : SV_DispatchThreadID)
{
	//get screen-space position from dispatch thred ID
	float2 PosSS = float2(dispatchThreadID.x / gClientW, dispatchThreadID.y / gClientH);
	//get clip-space position from screen-space position
	float4 PosH;
	PosH.x = 2.0f * PosSS.x - 1.0f;
	PosH.y = -2.0f * PosSS.y + 1.0;
	PosH.z = 1.0f;
	PosH.w = 1.0f;
	//get view-space position from screen-space position assuming depth == 1.0
	float3 PosV = mul(PosH, gInvProj).xyz / mul(PosH, gInvProj).w;
	//get world-space position from view-space position
	float3 PosW = mul(float4(PosV, 1.0f), gInvView);
	//get view ray direction
	float3 d = normalize(PosW - gEyePosW);
	//get view ray origin
	float3 o = gEyePosW + d * FLOAT_EPSILON;
	//trace ray using octree and return color of intersected octree geometry
	float4 res = traceRayPos(o, d, gSvoRayCastLevelIdx + 1);
	if (res.w > 0.0f)
	{
		gRayCastedOutputRW[dispatchThreadID.xy] = SampleOctree(res.xyz, gSvoRayCastLevelIdx, true);
	}
	else
	{
		gRayCastedOutputRW[dispatchThreadID.xy] = gEnvironmentMap.SampleLevel(samEnvMap, d, 0.0f);
	}
}

technique11 CastRaysTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, CastRaysCS()));
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////REFLECTIONS RAY CASTING///////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RWTexture2D<float4>gReflectionMapMaskRW;
RWTexture2D<float4>gReflectionMapRW;

[numthreads(RAY_CASTER_NUM_THREADS, RAY_CASTER_NUM_THREADS, 1)]
void CastReflectionRaysCS(int3 dispatchThreadID : SV_DispatchThreadID)
{
	gReflectionMapMaskRW[dispatchThreadID.xy >> gReflectionMapMaskSampleLevel] = -1.0f;
	gReflectionMapRW[dispatchThreadID.xy] = 0.0f;

	AllMemoryBarrier();

	//get screen-space position from dispatch thred ID
	float2 PosSS = float2(dispatchThreadID.x / gClientW, dispatchThreadID.y / gClientH);
	//get clip-space position from screen-space position
	float4 PosH;
	PosH.x = 2.0f * PosSS.x - 1.0f;
	PosH.y = -2.0f * PosSS.y + 1.0;
	PosH.z = 1.0f;
	PosH.w = 1.0f;
	//get view-space position from screen-space position and depth from g-buffer
	float3 PosV = mul(PosH, gInvProj).xyz / mul(PosH, gInvProj).w;
	PosV *= gNormalDepthMap.SampleLevel(samGBuffer, PosSS, 0.0f).w / PosV.z;
	//get world-space position from view-space position
	float3 PosW = mul(float4(PosV, 1.0f), gInvView);
	//get view-space normal from g-buffer 
	float3 NormalV = gNormalDepthMap.SampleLevel(samGBuffer, PosSS, 0.0f).xyz;
	//get world-space normal from view-space normal
	float3 NormalW = normalize(mul(NormalV, (float3x3)gInvView));
	//get reflection ray direction from view direction and surface normal
	
	float3 d = normalize(reflect(PosW - gEyePosW, NormalW));
	float3 o = PosW + 2.0f * NormalW * getSvoVoxelSize(gSvoRayCastLevelIdx);
	float4 res = traceRayPos(o, d, gSvoRayCastLevelIdx + 1);
	if (res.w > 0.0f)
	{
    gReflectionMapMaskRW[clamp((dispatchThreadID.xy >> gReflectionMapMaskSampleLevel) + int2(0, 1), int2(0, 0), float2(gClientW, gClientH) / pow(2.0f, gReflectionMapMaskSampleLevel))] = 1.0f;
    gReflectionMapMaskRW[clamp((dispatchThreadID.xy >> gReflectionMapMaskSampleLevel) + int2(0, 0), int2(0, 0), float2(gClientW, gClientH) / pow(2.0f, gReflectionMapMaskSampleLevel))] = 1.0f;
    gReflectionMapMaskRW[clamp((dispatchThreadID.xy >> gReflectionMapMaskSampleLevel) + int2(0, -1), int2(0, 0), float2(gClientW, gClientH) / pow(2.0f, gReflectionMapMaskSampleLevel))] = 1.0f;

    gReflectionMapMaskRW[clamp((dispatchThreadID.xy >> gReflectionMapMaskSampleLevel) + int2(1, 1), int2(0, 0), float2(gClientW, gClientH) / pow(2.0f, gReflectionMapMaskSampleLevel))] = 1.0f;
    gReflectionMapMaskRW[clamp((dispatchThreadID.xy >> gReflectionMapMaskSampleLevel) + int2(1, 0), int2(0, 0), float2(gClientW, gClientH) / pow(2.0f, gReflectionMapMaskSampleLevel))] = 1.0f;
    gReflectionMapMaskRW[clamp((dispatchThreadID.xy >> gReflectionMapMaskSampleLevel) + int2(1, -1), int2(0, 0), float2(gClientW, gClientH) / pow(2.0f, gReflectionMapMaskSampleLevel))] = 1.0f;

    gReflectionMapMaskRW[clamp((dispatchThreadID.xy >> gReflectionMapMaskSampleLevel) + int2(-1, 1), int2(0, 0), float2(gClientW, gClientH) / pow(2.0f, gReflectionMapMaskSampleLevel))] = 1.0f;
    gReflectionMapMaskRW[clamp((dispatchThreadID.xy >> gReflectionMapMaskSampleLevel) + int2(-1, 0), int2(0, 0), float2(gClientW, gClientH) / pow(2.0f, gReflectionMapMaskSampleLevel))] = 1.0f;
    gReflectionMapMaskRW[clamp((dispatchThreadID.xy >> gReflectionMapMaskSampleLevel) + int2(-1, -1), int2(0, 0), float2(gClientW, gClientH) / pow(2.0f, gReflectionMapMaskSampleLevel))] = 1.0f;

		gReflectionMapRW[dispatchThreadID.xy] = res;
	}
}

technique11 CastReflectionRaysTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, CastReflectionRaysCS()));
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////CONE TRACING//////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Texture2D<float4>gReflectionMapRO;

[numthreads(RAY_CASTER_NUM_THREADS, RAY_CASTER_NUM_THREADS, 1)]
void TraceReflectionConesCS(int3 dispatchThreadID : SV_DispatchThreadID)
{
	float2 PosSS = float2(dispatchThreadID.x / gClientW, dispatchThreadID.y / gClientH);
	float4 PosH;
	PosH.x = 2.0f * PosSS.x - 1.0f;
	PosH.y = -2.0f * PosSS.y + 1.0;
	PosH.z = 1.0f;
	PosH.w = 1.0f;
	//get view-space position from screen-space position and depth from g-buffer
	float3 PosV = mul(PosH, gInvProj).xyz / mul(PosH, gInvProj).w;
	PosV *= gNormalDepthMap.SampleLevel(samGBuffer, PosSS, 0.0f).w / PosV.z;
	//get world-space position from view-space position
	float3 PosW = mul(float4(PosV, 1.0f), gInvView);
	//get view-space normal from g-buffer 
	float3 NormalV = gNormalDepthMap.SampleLevel(samGBuffer, PosSS, 0.0f).xyz;
	//get world-space normal from view-space normal
	float3 NormalW = normalize(mul(NormalV, (float3x3)gInvView));
	//get reflection ray direction from view direction and surface normal
	float3 d = normalize(reflect(PosW - gEyePosW, NormalW));
	
	if (gReflectionMapRO.SampleLevel(samGBuffer, PosSS, gReflectionMapMaskSampleLevel).w <= 0.0f)
	{
		gRayCastedOutputRW[dispatchThreadID.xy] = gEnvironmentMap.SampleLevel(samEnvMap, d, getEnvMipMapLvl(enclosingSvoLevelIdxEx(coneRadius(SVO_ROOT_NODE_SZ * sqrt(3.0f), gConeTheta) + FLOAT_EPSILON)));
		return;
	}
	
	float3 o = PosW + 2.0f * NormalW * getSvoVoxelSize(gSvoRayCastLevelIdx);
	float4 res = gReflectionMapRO.SampleLevel(samGBuffer, PosSS, 0);
	float maxDist = res.w > 0.0f ? length(res.xyz - o) : SVO_ROOT_NODE_SZ * sqrt(3.0f);
	bool finiteDist = res.w > 0.0f ? true : false;
	gRayCastedOutputRW[dispatchThreadID.xy] = float4(traceSpecularVoxelCone(o, d, maxDist, finiteDist), 1.0f);
}

technique11 TraceReflectionConesTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, TraceReflectionConesCS()));
	}
};