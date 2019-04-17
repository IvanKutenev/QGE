//flags
#define VOXEL_EMPTY_FLAG 0
#define VOXEL_FILLED_FLAG 1
#define NO_CHILD -1 
#define NO_PARENT -1
#define NO_GEO_FOUND 0xffffffff
#define OUT_OF_NODE_BOUNDS -1
//magic constants
#define FLOAT_EPSILON 0.001f
//SVO parameters
#define SVO_LEVELS_COUNT 10
#define SVO_VOXEL_BLOCK_SIDE_SZ 2
static const float SVO_BUILD_RT_SZ = (2 << (SVO_LEVELS_COUNT - 1)) * SVO_VOXEL_BLOCK_SIDE_SZ;
static const int SVO_VOXEL_BLOCK_SZ = SVO_VOXEL_BLOCK_SIDE_SZ * SVO_VOXEL_BLOCK_SIDE_SZ * SVO_VOXEL_BLOCK_SIDE_SZ;
#define SVO_ROOT_NODE_SZ 500.0
//configuration perameters
#define MAX_TRACE_ITERATIONS 512
#define RAY_CASTER_NUM_THREADS 8
#define RESET_SVO_NUM_THREADS 512
#define MAX_AVG_ITER_COUNT 20

//sampler state for g-buffer
SamplerState samGBuffer
{
	Filter = MIN_MAG_MIP_POINT;

	AddressU = CLAMP;
	AddressV = CLAMP;
};
// sampler state for environment map
SamplerState samEnvMap
{
	Filter = ANISOTROPIC;
	MaxAnisotropy = 16;

	AddressU = WRAP;
	AddressV = WRAP;
};

struct SvoBuildGsOut
{
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
	float2 Tex : TEXCOORD0;
};

struct SvoDebugDrawVsIn
{
	float3 PosW : POSITION;
	int svoLevelIdx : LEVEL_INDEX;
};

struct SvoDebugDrawGsOut
{
	float4 PosH : SV_POSITION;
};

struct SvoNode
{
	float3 PosW;
	int svoLevelIdx;
	int parentNodeIdx;
	int childNodesIds[8];
	int voxelBlockStartIdx;
};

struct SvoVoxel
{
	uint colorMask;
};

StructuredBuffer<SvoNode>gNodesPoolRO;
StructuredBuffer<SvoVoxel>gVoxelsPoolRO;
StructuredBuffer<int>gVoxelsFlagsRO;
globallycoherent RWStructuredBuffer<SvoNode>gNodesPoolRW;
globallycoherent RWStructuredBuffer<SvoVoxel>gVoxelsPoolRW;
globallycoherent RWStructuredBuffer<int>gVoxelsFlagsRW;

// normals and depth g-buffer
Texture2D gNormalDepthMap;
// environment map
TextureCube gEnvironmentMap;

cbuffer cbSVOBuildInformation
{
	int gCurSVOLevelIdx;
};

cbuffer cbSVODescription
{
	float4x4 gSVOViewProj[6];
	float3 gSvoRootNodeCenterPosW;
	int3 gChildOffsets[8] = {int3(-1, -1, -1), int3(-1, -1, 1) , int3(-1, 1, -1) , int3(-1, 1, 1), //offsets from center of parent node
							 int3(1, -1, -1), int3(1, -1, 1) , int3(1, 1, -1) , int3(1, 1, 1) };
};

cbuffer cbSVOAveragingInfo
{
	int gSvoMinAveragingLevelIdx;
};

cbuffer cbConeTracingSettings
{
	// max cone tracing iterations count
	int gMaxIterCount;
	// cone tracing mask sample level
	int gReflectionMapMaskSampleLevel;
	// constant cone theta for all geometry for testing purposes
	float gConeTheta;
	// alpha tweaking coefficients for artifact-free reflections
	float gAlphaClampValue;
	float gAlphaMultiplier;
	// cone tracing step tweak coefficients for performance purposes
	float gMinStepMultiplier;
	float gMaxStepMultiplier;
	// weighting coefficient, in future builds can be replaced with per-node visibilty
	float gWeightMultiplier;
	// amount of generted environment map mipmaps
	float gEnvironmentMapMipsCount;
};

cbuffer cbSVOPerRayCastingIteration
{
	int gSvoRayCastLevelIdx;
};

static const float3 cameraViewDirections[6] =
{
	float3(0.0f, 0.0f, -1.0f),
	float3(-1.0f, 0.0f, 0.0f),
	float3(0.0f, -1.0f, 0.0f),
	float3(0.0f, 0.0f, 1.0f),
	float3(1.0f, 0.0f, 0.0f),
	float3(0.0f, 1.0f, 0.0f)
};

uint PackUnormFloat4(float4 val) {
	uint4 iVal = uint4(255.0f * val);
	return iVal.x << 24 | iVal.y << 16 | iVal.z << 8 | iVal.w;
}

float4 UnpackUnormFloat4(uint packed)
{
	float4 val = float4((packed >> 24) & 0xff, (packed >> 16) & 0xff, (packed >> 8) & 0xff, packed & 0xff);
	return val / 255.0f;
}

void ComputeMovingAverage(int voxelIdx, float4 val)
{
	uint newVal = PackUnormFloat4(float4(val.xyz, 1.0f / 255.0f));
	uint prevVal = 0;
	uint curVal = 1;
	[unroll(MAX_AVG_ITER_COUNT)]
	for(int i = 0; i < MAX_AVG_ITER_COUNT; ++i)
	{
		InterlockedCompareExchange(gVoxelsPoolRW[voxelIdx].colorMask, prevVal, newVal, curVal);
		if (prevVal == curVal)
			return;
		prevVal = curVal;
		float4 unpacked = UnpackUnormFloat4(curVal);
		float3 data = unpacked.xyz;
		float counter = unpacked.w * 255.0f;
		data = (data * counter + val.xyz) / (counter + 1);
		newVal = PackUnormFloat4(float4(data, (counter + 1) / 255.0f));
	}
}

int GetViewMatrixIndex(float3 normal)
{
	float dotProducts[6];
	[unroll(6)]
	for (int i = 0; i < 6; ++i)
	{
		dotProducts[i] = dot(-cameraViewDirections[i], normal);
	}
	float maximum = -1.0f;
	int maxidx = 0;
	[unroll(6)]
	for (int i = 0; i < 6; ++i)
	{
		if (maximum < dotProducts[i])
		{
			maximum = dotProducts[i];
			maxidx = i;
		}
	}
	return maxidx;
}

float getSvoNodeSize(int svoLevelIdx)
{
	return SVO_ROOT_NODE_SZ * exp2(-float(svoLevelIdx));
}

float getSvoVoxelSize(int svoLevelIdx)
{
	return getSvoNodeSize(svoLevelIdx) / float(SVO_VOXEL_BLOCK_SIDE_SZ);
}

int getSvoNodeChildIdxByOffset(int3 offset)
{
	int3 mappedOffset = int3(offset.x == 1? 1 : 0, offset.y == 1 ? 1 : 0, offset.z == 1 ? 1 : 0);
	return mappedOffset.x << 2 + mappedOffset.y << 1 + mappedOffset.z;
}

int3 getSvoNodeOffsetByIdx(int idx)
{
	return gChildOffsets[idx];
}

bool inBounds(float3 centerPosW, float cubeSize, float3 testPosW)
{
	return (centerPosW.x - cubeSize / 2.0 <= testPosW.x && centerPosW.x + cubeSize / 2.0 > testPosW.x) &&
		   (centerPosW.y - cubeSize / 2.0 <= testPosW.y && centerPosW.y + cubeSize / 2.0 > testPosW.y) &&
           (centerPosW.z - cubeSize / 2.0 <= testPosW.z && centerPosW.z + cubeSize / 2.0 > testPosW.z);
}

//get node index by its position 
int getSvoNodeIndexRW(float3 PosW, int maxLevelsCount)
{
	//start from root node
	int curNodeIdx = 0;
	[unroll(SVO_LEVELS_COUNT)]
	for(int curLevel = 0; curLevel < SVO_LEVELS_COUNT; ++curLevel)
	{
		float chosenChildNodeSz = getSvoNodeSize(gNodesPoolRW[curNodeIdx].svoLevelIdx + 1);
		//determine needed child node
		int chosenChildIdx = NO_CHILD;
		[unroll(8)]
		for (int i = 0; i < 8; ++i)
		{
			if (gNodesPoolRW[curNodeIdx].childNodesIds[i] != NO_CHILD && inBounds(gNodesPoolRW[gNodesPoolRW[curNodeIdx].childNodesIds[i]].PosW, chosenChildNodeSz, PosW))
			{
				chosenChildIdx = i;
			}
		}

		//check whether we alredy reached leaf
		if (chosenChildIdx == NO_CHILD)
			return curNodeIdx;

		//if not reached leaf then go down
		curNodeIdx = gNodesPoolRW[curNodeIdx].childNodesIds[chosenChildIdx];

		if (gNodesPoolRW[curNodeIdx].svoLevelIdx >= min(SVO_LEVELS_COUNT, maxLevelsCount) - 1)
			return curNodeIdx;
	}
	return curNodeIdx;
}

int getSvoNodeIndexRO(float3 PosW, int maxLevelsCount)
{
	//start from root node
	int curNodeIdx = 0;
	[unroll(SVO_LEVELS_COUNT)]
	for (int curLevel = 0; curLevel < SVO_LEVELS_COUNT; ++curLevel)
	{
		float chosenChildNodeSz = getSvoNodeSize(gNodesPoolRO[curNodeIdx].svoLevelIdx + 1);
		//determine needed child node
		int chosenChildIdx = NO_CHILD;
		[unroll(8)]
		for (int i = 0; i < 8; ++i)
		{
			if (gNodesPoolRO[curNodeIdx].childNodesIds[i] != NO_CHILD && inBounds(gNodesPoolRO[gNodesPoolRO[curNodeIdx].childNodesIds[i]].PosW, chosenChildNodeSz, PosW))
			{
				chosenChildIdx = i;
			}
		}

		//check whether we alredy reached leaf
		if (chosenChildIdx == NO_CHILD)
			return curNodeIdx;

		//if not reached leaf then go down
		curNodeIdx = gNodesPoolRO[curNodeIdx].childNodesIds[chosenChildIdx];

		if (gNodesPoolRO[curNodeIdx].svoLevelIdx >= min(SVO_LEVELS_COUNT, maxLevelsCount) - 1)
			return curNodeIdx;
	}
	return curNodeIdx;
}

int getSvoNodeLocalIndex(SvoNode parentNode, float3 PosW)
{
	float childNodeSz = getSvoNodeSize(parentNode.svoLevelIdx + 1);
	[unroll(8)]
	for (int i = 0; i < 8; ++i)
	{
		if (inBounds(parentNode.PosW + getSvoNodeOffsetByIdx(i) * getSvoNodeSize(parentNode.svoLevelIdx) / 4.0, childNodeSz, PosW))
			return i;
	}
	return -1;
}

//add child node to nodesPool[parentidx]
void addSvoNodeChild(int parentIdx, float3 PosW)
{
	int l_childIdx = getSvoNodeLocalIndex(gNodesPoolRW[parentIdx], PosW);
	if (l_childIdx == -1)
		return;

	int tentativeIdx = -1, oldIdx;

	//try to add child node
	InterlockedCompareExchange(gNodesPoolRW[parentIdx].childNodesIds[l_childIdx], NO_CHILD, tentativeIdx, oldIdx);
	//if we added new node indeed, we should initialize it
	if (oldIdx == NO_CHILD)
	{
		//set correct index of node
		int idx = gNodesPoolRW.IncrementCounter();
		InterlockedExchange(gNodesPoolRW[parentIdx].childNodesIds[l_childIdx], idx, oldIdx);
		//set PosW using parent's node position
		gNodesPoolRW[idx].PosW = gNodesPoolRW[parentIdx].PosW + getSvoNodeOffsetByIdx(l_childIdx) * getSvoNodeSize(gNodesPoolRW[parentIdx].svoLevelIdx) / 4.0;
		//set level as parent's + 1 
		gNodesPoolRW[idx].svoLevelIdx = gNodesPoolRW[parentIdx].svoLevelIdx + 1;
		//set reference to parent node
		gNodesPoolRW[idx].parentNodeIdx = parentIdx;
		//initialize childs references with NO_CHILD
		[unroll(8)]
		for (int i = 0; i < 8; ++i)
			gNodesPoolRW[idx].childNodesIds[i] = NO_CHILD;
		gNodesPoolRW[idx].voxelBlockStartIdx = idx * SVO_VOXEL_BLOCK_SZ;
	}
}

//get voxel index by its position
int getSvoVoxelIndexRW(float3 PosW, int maxLevelsCount)
{
	SvoNode node = gNodesPoolRW[getSvoNodeIndexRW(PosW, maxLevelsCount)];
	int3 offset = floor((PosW - node.PosW) / getSvoVoxelSize(node.svoLevelIdx)) + int3(SVO_VOXEL_BLOCK_SIDE_SZ >> 1, SVO_VOXEL_BLOCK_SIDE_SZ >> 1, SVO_VOXEL_BLOCK_SIDE_SZ >> 1);
	//out of octree bounds
	if (offset.x < 0 || offset.y < 0 || offset.z < 0 ||
		offset.x >= SVO_VOXEL_BLOCK_SIDE_SZ || offset.y >= SVO_VOXEL_BLOCK_SIDE_SZ || offset.z >= SVO_VOXEL_BLOCK_SIDE_SZ)
		return OUT_OF_NODE_BOUNDS;
	return node.voxelBlockStartIdx + SVO_VOXEL_BLOCK_SIDE_SZ * SVO_VOXEL_BLOCK_SIDE_SZ * offset.y + SVO_VOXEL_BLOCK_SIDE_SZ * offset.x + offset.z;
}

int getSvoVoxelIndexRO(float3 PosW, int maxLevelsCount)
{
	SvoNode node = gNodesPoolRO[getSvoNodeIndexRO(PosW, maxLevelsCount)];
	int3 offset = floor((PosW - node.PosW) / getSvoVoxelSize(node.svoLevelIdx)) + int3(SVO_VOXEL_BLOCK_SIDE_SZ >> 1, SVO_VOXEL_BLOCK_SIDE_SZ >> 1, SVO_VOXEL_BLOCK_SIDE_SZ >> 1);
	//out of octree bounds
	if (offset.x < 0 || offset.y < 0 || offset.z < 0 ||
		offset.x >= SVO_VOXEL_BLOCK_SIDE_SZ || offset.y >= SVO_VOXEL_BLOCK_SIDE_SZ || offset.z >= SVO_VOXEL_BLOCK_SIDE_SZ)
		return OUT_OF_NODE_BOUNDS;
	return node.voxelBlockStartIdx + SVO_VOXEL_BLOCK_SIDE_SZ * SVO_VOXEL_BLOCK_SIDE_SZ * offset.y + SVO_VOXEL_BLOCK_SIDE_SZ * offset.x + offset.z;
}

int2 getSvoVoxelIndexAndLevelRO(float3 PosW, int maxLevelsCount)
{
	SvoNode node = gNodesPoolRO[getSvoNodeIndexRO(PosW, maxLevelsCount)];
	int3 offset = floor((PosW - node.PosW) / getSvoVoxelSize(node.svoLevelIdx)) + int3(SVO_VOXEL_BLOCK_SIDE_SZ >> 1, SVO_VOXEL_BLOCK_SIDE_SZ >> 1, SVO_VOXEL_BLOCK_SIDE_SZ >> 1);
	//out of octree bounds
	if (offset.x < 0 || offset.y < 0 || offset.z < 0 ||
		offset.x >= SVO_VOXEL_BLOCK_SIDE_SZ || offset.y >= SVO_VOXEL_BLOCK_SIDE_SZ || offset.z >= SVO_VOXEL_BLOCK_SIDE_SZ)
		return int2(OUT_OF_NODE_BOUNDS, 0);
	return int2(node.voxelBlockStartIdx + SVO_VOXEL_BLOCK_SIDE_SZ * SVO_VOXEL_BLOCK_SIDE_SZ * offset.y + SVO_VOXEL_BLOCK_SIDE_SZ * offset.x + offset.z, node.svoLevelIdx);
}

void setVoxelData(float3 PosW, int maxLevelsCount, SvoVoxel voxelVal, int flag)
{
	int voxelIdx = getSvoVoxelIndexRW(PosW, maxLevelsCount);
	if (voxelIdx != OUT_OF_NODE_BOUNDS)
	{
		ComputeMovingAverage(voxelIdx, UnpackUnormFloat4(voxelVal.colorMask));
		gVoxelsFlagsRW[voxelIdx] = flag;
	}
}

SvoVoxel getVoxelData(float3 PosW, int maxLevelsCount)
{
	return gVoxelsPoolRO[getSvoVoxelIndexRO(PosW, maxLevelsCount)];
}

int getVoxelFlag(float3 PosW, int maxLevelsCount)
{
	return gVoxelsFlagsRO[getSvoVoxelIndexRO(PosW, maxLevelsCount)];
}

//returns parameters tmin, tmax in order of visiting, if tmin > tmax => no intersection
float2 intersectBoundary(float3 o, float3 d, float3 centerPos, float size)
{
	float2 t;
	float3 tmin, tmax;

	tmin = (centerPos - size / 2.0 - o) / d;
	tmax = (centerPos + size / 2.0 - o) / d;
	
	t[0] = min(tmin.x, tmax.x);
	t[1] = max(tmin.x, tmax.x);

	t[0] = max(t[0], min(tmin.y, tmax.y));
	t[1] = min(t[1], max(tmin.y, tmax.y)); 

	t[0] = max(t[0], min(tmin.z, tmax.z));
	t[1] = min(t[1], max(tmin.z, tmax.z));

	return t;
}

SvoVoxel traceVoxelBlock(float3 rayOrigin, float3 rayDir, SvoNode curNode)
{
	float voxelSize = getSvoVoxelSize(curNode.svoLevelIdx);
	float halfVoxelSize = voxelSize / 2.0f;
	float3 d = normalize(rayDir);
	float3 o = rayOrigin;
	float t = 0.0f;
	SvoVoxel noGeoFound;
	noGeoFound.colorMask = NO_GEO_FOUND;
	[loop]
	for (int i = 0; i < MAX_TRACE_ITERATIONS; ++i)
	{
		float3 curPosW = o + t * d;

		if (!inBounds(curNode.PosW, getSvoNodeSize(curNode.svoLevelIdx), curPosW))
			return noGeoFound;

		if (getVoxelFlag(curPosW, curNode.svoLevelIdx + 1) == VOXEL_FILLED_FLAG)
			return getVoxelData(curPosW, curNode.svoLevelIdx + 1);

		float3 dist = curPosW - curNode.PosW;
		float3 offset = float3(d.x > 0.0f ? floor(dist.x / halfVoxelSize) : ceil(dist.x / halfVoxelSize),
							   d.y > 0.0f ? floor(dist.y / halfVoxelSize) : ceil(dist.y / halfVoxelSize),
							   d.z > 0.0f ? floor(dist.z / halfVoxelSize) : ceil(dist.z / halfVoxelSize)) * halfVoxelSize;

		t = intersectBoundary(o, d, curNode.PosW + offset, voxelSize)[1] + FLOAT_EPSILON;
	}
	return noGeoFound;
}

SvoVoxel traceRay(float3 rayOrigin, float3 rayDir, int maxTracingLevelsCount)
{
	float3 d = normalize(rayDir);
	float3 o = rayOrigin;
	float t = 0.0;
	SvoNode curNode = gNodesPoolRO[getSvoNodeIndexRO(o, maxTracingLevelsCount)];
	SvoVoxel noGeoFound;
	noGeoFound.colorMask = NO_GEO_FOUND;
	[loop]
	for (int i = 0; i < MAX_TRACE_ITERATIONS; ++i)
	{
		//if we are out of root node => exit
		if (!inBounds(gNodesPoolRO[0].PosW, SVO_ROOT_NODE_SZ, o + t * d))
			return noGeoFound;

		//we are in leaf
		if (curNode.svoLevelIdx == min(maxTracingLevelsCount - 1, SVO_LEVELS_COUNT - 1))
		{
			SvoVoxel res = traceVoxelBlock(o + t * d, d, curNode);
			if (res.colorMask != NO_GEO_FOUND)
				return res;
			
			t = intersectBoundary(o, d, curNode.PosW, getSvoNodeSize(curNode.svoLevelIdx))[1] + FLOAT_EPSILON;
			curNode = gNodesPoolRO[getSvoNodeIndexRO(o + t * d, curNode.svoLevelIdx)];
		}
		//we are in node
		else
		{
			float2 t_min_max;
			float t_min = -1.0f;
			int childNodeIndex = 0;
			bool intersectedChilds = false;
			for (int i = 0; i < 8; ++i)
			{
				if (curNode.childNodesIds[i] == NO_CHILD)
					continue;

				t_min_max = intersectBoundary(o, d, gNodesPoolRO[curNode.childNodesIds[i]].PosW, getSvoNodeSize(curNode.svoLevelIdx + 1));

				if (t_min_max[0] > t_min_max[1] || t_min_max[0] + 0.5f * (t_min_max[1] - t_min_max[0]) < t)
					continue;
		
				if (t_min_max[0] < t_min || t_min < 0.0f)
				{
					childNodeIndex = curNode.childNodesIds[i];
					t_min = t_min_max[0];
				}
				
				intersectedChilds = true;
			}

			if (!intersectedChilds)
			{
				t = intersectBoundary(o, d, curNode.PosW, getSvoNodeSize(curNode.svoLevelIdx))[1] + FLOAT_EPSILON;
				curNode = gNodesPoolRO[getSvoNodeIndexRO(o + t * d, curNode.svoLevelIdx)];
			}
			else
			{
				t = t_min + FLOAT_EPSILON;
				curNode = gNodesPoolRO[childNodeIndex];
			}
		}
	}
	return noGeoFound;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const float MAX_SPECULAR_EXP = 1024.0f;
static const float ROUGHNESS_EPSILON = 0.0000001f;
static const float XI = 0.244f;
static const float DEFAULT_GLOSS = 0.2f;

//(pos, status): status > 0 - success, status < 0 - no geometry
float4 traceVoxelBlockPos(float3 rayOrigin, float3 rayDir, SvoNode curNode)
{
	float voxelSize = getSvoVoxelSize(curNode.svoLevelIdx);
	float halfVoxelSize = voxelSize / 2.0f;
	float3 d = normalize(rayDir);
	float3 o = rayOrigin;
	float t = 0.0f;
	SvoVoxel noGeoFound;
	noGeoFound.colorMask = NO_GEO_FOUND;
	[loop]
	for (int i = 0; i < MAX_TRACE_ITERATIONS; ++i)
	{
		float3 curPosW = o + t * d;

		if (!inBounds(curNode.PosW, getSvoNodeSize(curNode.svoLevelIdx), curPosW))
			return float4(curPosW, -1.0f);

		if (getVoxelFlag(curPosW, curNode.svoLevelIdx + 1) == VOXEL_FILLED_FLAG)
			return float4(curPosW, 1.0f);

		float3 dist = curPosW - curNode.PosW;
		float3 offset = float3(d.x > 0.0f ? floor(dist.x / halfVoxelSize) : ceil(dist.x / halfVoxelSize),
			d.y > 0.0f ? floor(dist.y / halfVoxelSize) : ceil(dist.y / halfVoxelSize),
			d.z > 0.0f ? floor(dist.z / halfVoxelSize) : ceil(dist.z / halfVoxelSize)) * halfVoxelSize;

		t = intersectBoundary(o, d, curNode.PosW + offset, voxelSize)[1] + FLOAT_EPSILON;
	}
	return float4(o + t * d, -1.0f);
}

//(pos, status): status > 0 - success, status < 0 - no geometry
float4 traceReflectionPos(float3 rayOrigin, float3 rayDir, int maxTracingLevelsCount)
{
	float3 d = normalize(rayDir);
	float3 o = rayOrigin;
	float t = 0.0;
	SvoNode curNode = gNodesPoolRO[getSvoNodeIndexRO(o, maxTracingLevelsCount)];
	[loop]
	for (int i = 0; i < MAX_TRACE_ITERATIONS; ++i)
	{
		//if we are out of root node => exit
		if (!inBounds(gNodesPoolRO[0].PosW, SVO_ROOT_NODE_SZ, o + t * d))
			return float4(o + t * d, -1.0f);

		//we are in leaf
		if (curNode.svoLevelIdx == min(maxTracingLevelsCount - 1, SVO_LEVELS_COUNT - 1))
		{
			float4 res = traceVoxelBlockPos(o + t * d, d, curNode);
			if (res.w > 0.0f)
				return res;

			t = intersectBoundary(o, d, curNode.PosW, getSvoNodeSize(curNode.svoLevelIdx))[1] + FLOAT_EPSILON;
			curNode = gNodesPoolRO[getSvoNodeIndexRO(o + t * d, curNode.svoLevelIdx)];
		}
		//we are in node
		else
		{
			float2 t_min_max;
			float t_min = -1.0f;
			int childNodeIndex = 0;
			bool intersectedChilds = false;
			for (int i = 0; i < 8; ++i)
			{
				if (curNode.childNodesIds[i] == NO_CHILD)
					continue;

				t_min_max = intersectBoundary(o, d, gNodesPoolRO[curNode.childNodesIds[i]].PosW, getSvoNodeSize(curNode.svoLevelIdx + 1));

				if (t_min_max[0] > t_min_max[1] || t_min_max[0] + 0.5f * (t_min_max[1] - t_min_max[0]) < t)
					continue;

				if (t_min_max[0] < t_min || t_min < 0.0f)
				{
					childNodeIndex = curNode.childNodesIds[i];
					t_min = t_min_max[0];
				}

				intersectedChilds = true;
			}

			if (!intersectedChilds)
			{
				t = intersectBoundary(o, d, curNode.PosW, getSvoNodeSize(curNode.svoLevelIdx))[1] + FLOAT_EPSILON;
				curNode = gNodesPoolRO[getSvoNodeIndexRO(o + t * d, curNode.svoLevelIdx)];
			}
			else
			{
				t = t_min + FLOAT_EPSILON;
				curNode = gNodesPoolRO[childNodeIndex];
			}
		}
	}
	return float4(o + t * d, -1.0f);
}

float coneRadius(float t, float alpha)
{
	return t * tan(alpha);
}

// computes octree sampling level corresponding to desired cone radius
float enclosingSvoLevelIdxEx(float coneRadius)
{
	return clamp(log2(SVO_ROOT_NODE_SZ / 2.0f / coneRadius / float(SVO_VOXEL_BLOCK_SIDE_SZ)), gSvoMinAveragingLevelIdx, gSvoRayCastLevelIdx);
}

float roughnessToSpecularPower(float sr)
{
	return 2.0f / (sr + ROUGHNESS_EPSILON) / (sr + ROUGHNESS_EPSILON) - 2.0f;
}

float specularPowerToConeAngle(float specularPower)
{
	if (specularPower >= exp2(MAX_SPECULAR_EXP))
		return 0.0f;
	float exponent = 1.0f / (specularPower + 1.0f);
	return acos(pow(XI, exponent));
}

float4 coneSampleGlossWeightedColor(float3 color, float gloss)
{
	return float4(color * gloss, gloss);
}

// returns center position of closest to desired position voxel with desired level
float3 getSvoVoxelNearestCenterPos(SvoNode curNode, float3 samplePosW, int svoLevelIdx)
{
	float voxelSize = getSvoVoxelSize(svoLevelIdx);
	float halfVoxelSize = voxelSize / 2.0f;
	float3 dist = samplePosW - curNode.PosW;
	float3 offset = float3(dist.x >= 0.0f ? 2.0f * floor(dist.x / voxelSize) + 1.0f : 2.0f * ceil(dist.x / voxelSize) - 1.0f,
		dist.y >= 0.0f ? 2.0f * floor(dist.y / voxelSize) + 1.0f : 2.0f * ceil(dist.y / voxelSize) - 1.0f,
		dist.z >= 0.0f ? 2.0f * floor(dist.z / voxelSize) + 1.0f : 2.0f * ceil(dist.z / voxelSize) - 1.0f) * halfVoxelSize;

	return curNode.PosW + offset;
}

// sample octree bilineraly
float4 SampleOctree(float3 samplePosW, int samplingLvlIdx, bool normalize)
{
	SvoNode curNode = gNodesPoolRO[getSvoNodeIndexRO(samplePosW, samplingLvlIdx + 1)];
	float voxelSize = getSvoVoxelSize(curNode.svoLevelIdx);
	float3 sampleVoxelCenterPosW = getSvoVoxelNearestCenterPos(curNode, samplePosW, samplingLvlIdx);
	float3 lerpPos = voxelSize * sign(samplePosW - sampleVoxelCenterPosW) + sampleVoxelCenterPosW;
	float3 lerpParam = saturate(abs(lerpPos - samplePosW) / voxelSize);

	int2 lerpFrom[4] = {
		getSvoVoxelIndexAndLevelRO(float3(lerpPos.x, samplePosW.y, samplePosW.z), curNode.svoLevelIdx + 1),
		getSvoVoxelIndexAndLevelRO(float3(lerpPos.x, samplePosW.y, lerpPos.z), curNode.svoLevelIdx + 1),
		getSvoVoxelIndexAndLevelRO(float3(lerpPos.x, lerpPos.y, samplePosW.z), curNode.svoLevelIdx + 1),
		getSvoVoxelIndexAndLevelRO(float3(lerpPos.x, lerpPos.y, lerpPos.z), curNode.svoLevelIdx + 1)
	};

	int2 lerpTo[4] = {
		getSvoVoxelIndexAndLevelRO(float3(samplePosW.x, samplePosW.y, samplePosW.z), curNode.svoLevelIdx + 1),
		getSvoVoxelIndexAndLevelRO(float3(samplePosW.x, samplePosW.y, lerpPos.z), curNode.svoLevelIdx + 1),
		getSvoVoxelIndexAndLevelRO(float3(samplePosW.x, lerpPos.y, samplePosW.z), curNode.svoLevelIdx + 1),
		getSvoVoxelIndexAndLevelRO(float3(samplePosW.x, lerpPos.y, lerpPos.z), curNode.svoLevelIdx + 1)
	};

	float4 color[7];

	[unroll(4)]
	for (int i = 0; i < 4; ++i)
	{
		color[i] = lerp(float4(UnpackUnormFloat4(gVoxelsPoolRO[lerpFrom[i].x].colorMask).rgb, gVoxelsFlagsRO[lerpFrom[i].x]),
			float4(UnpackUnormFloat4(gVoxelsPoolRO[lerpTo[i].x].colorMask).rgb, gVoxelsFlagsRO[lerpTo[i].x]),
			lerpParam.x);
	}

	[unroll(6)]
	for (int i = 4; i < 6; ++i)
	{
		color[i] = lerp(color[i * 2 - 7], color[i * 2 - 8], lerpParam.z);
	}

	color[6] = lerp(color[5], color[4], lerpParam.y);

	return float4(lerp(color[6].rgb, color[6].rgb / color[6].a, normalize), color[6].a);
}

// sample octree trilinearly
float4 SampleOctreeEx(float3 PosW, float samplingLvl, bool normalize)
{
	float4 v1 = SampleOctree(PosW, ceil(samplingLvl), normalize);
	float4 v2 = SampleOctree(PosW, floor(samplingLvl), normalize);
	return lerp(v1, v2, ceil(samplingLvl) - samplingLvl);
}

// get cone ray parameter from current radius
float coneParam(float r, float alpha)
{
	return r / tan(alpha);
}
// get cone tracing step corresponding to current cone radius
float coneStep(float svoLevelIdx)
{
	if (gSvoMinAveragingLevelIdx < svoLevelIdx < gSvoRayCastLevelIdx)
		return getSvoVoxelSize(svoLevelIdx) * gMinStepMultiplier;
	else
		return getSvoVoxelSize(svoLevelIdx) * gMaxStepMultiplier;
}
// compute environment mipmap index corresponding to octree level
float getEnvMipMapLvl(float svoLevelIdx)
{
	return (gEnvironmentMapMipsCount - 1.0f) * (gSvoRayCastLevelIdx - svoLevelIdx) / (gSvoRayCastLevelIdx - gSvoMinAveragingLevelIdx);
}
// cone tracer
float3 traceSpecularVoxelCone(float3 o, float3 d, float t_max, bool isFiniteDist)
{
	float4 totalColor = 0.0f, voxelColor = 0.0f;
	float t = coneParam(getSvoVoxelSize(gSvoRayCastLevelIdx) * 0.5f, gConeTheta);
	float enclosingLvlIdx = gSvoRayCastLevelIdx;

	[branch]
	if (t_max < t)
	{
		return SampleOctreeEx(o + t_max * d, gSvoRayCastLevelIdx, true).rgb;
	}

	for (int iterCount = 0; t <= t_max && totalColor.a < 1.0f && iterCount < gMaxIterCount; ++iterCount)
	{
		enclosingLvlIdx = enclosingSvoLevelIdxEx(coneRadius(t, gConeTheta) + FLOAT_EPSILON);

		[branch]
		if (t + coneStep(enclosingLvlIdx) > t_max)
			voxelColor = float4(SampleOctreeEx(o + t_max * d, enclosingLvlIdx, true).rgb, 1.0f);
		else
			voxelColor = SampleOctreeEx(o + t * d, enclosingLvlIdx, false);

		totalColor += gWeightMultiplier * voxelColor.a * (1.0f - totalColor.a) * float4(voxelColor.rgb, 1.0f);

		t += coneStep(enclosingLvlIdx);
	}

	totalColor.a = saturate(totalColor.a);

	if (totalColor.a > 0.0f)
		totalColor.rgb /= totalColor.a;

	return lerp(gEnvironmentMap.SampleLevel(samEnvMap, d, getEnvMipMapLvl(enclosingSvoLevelIdxEx(coneRadius(SVO_ROOT_NODE_SZ * sqrt(3.0f), gConeTheta) + FLOAT_EPSILON))), totalColor.rgb, isFiniteDist ? 1.0f : saturate((totalColor.a - gAlphaClampValue) * gAlphaMultiplier));
}