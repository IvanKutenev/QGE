#define MAX_BODIES_COUNT 150
#define PPLL_LAYERS_COUNT 100
#define PARTICLES_GRID_CASCADES_COUNT 1
#define PARTICLES_GRID_RESOLUTION 100
#define CS_THREAD_GROUPS_NUMBER 10
#define PARTICLES_COUNT PARTICLES_GRID_RESOLUTION * PARTICLES_GRID_RESOLUTION * PARTICLES_GRID_RESOLUTION

struct BodiesListNode
{
	float4 PosWBodyIndex; //if index == (a >= 0) then it is front face of Bodies[a], else if index == (a < 0) then it is back face of Bodies[-(a + 1)] 
	int NextIndex;
};

struct MacroParams
{
	float Mass;
	float InertionMomentum;
	float3 CMPosW;
	float3 Speed;
	float3 AngularSpeed;
	float4 Quaternion;
};

struct MicroParams // contains info about particles which approximately forms specific rigid body
{
	int BodyIndex;
	float3 LinearMomentum;
	float3 AngularMomentum;
};

int GetParticlesGridIndex(int3 position, int cascadeIndex)
{
	return (cascadeIndex * PARTICLES_COUNT + position.z * PARTICLES_GRID_RESOLUTION * PARTICLES_GRID_RESOLUTION + position.x * PARTICLES_GRID_RESOLUTION + position.y);
}

float3 CalculateParticlesGridOffset(float3 PosW, float3 GridCenterPosW, float GridCellSize)
{
	return (PosW - GridCenterPosW) / GridCellSize;
}

void CalculateParticlesGridPositions(float3 PosW, float3 GridCenterPosW, float GridCellSize, int cascadeIndex, out int3 voxelPos)
{
	float3 offset = CalculateParticlesGridOffset(PosW, GridCenterPosW, GridCellSize);
	offset = round(offset);
	voxelPos = int3(PARTICLES_GRID_RESOLUTION / 2, PARTICLES_GRID_RESOLUTION / 2, PARTICLES_GRID_RESOLUTION / 2) + int3(offset);
}

//sort particles in Z-coordinate increase order

void InsertionSort(inout BodiesListNode a[PPLL_LAYERS_COUNT], uint sz)
{
	[loop]
	for (int i = 1; i < sz; i++)
	{
		BodiesListNode value = a[i];
		[loop]
		for (int j = i - 1; j >= 0 && a[j].PosWBodyIndex.z > value.PosWBodyIndex.z; j--)
			a[j + 1] = a[j];
		a[j + 1] = value;
	}
}