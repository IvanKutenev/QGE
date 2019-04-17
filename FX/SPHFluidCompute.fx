#include "SPHFluidDefinitions.fx"

[numthreads(NUM_THREADS, 1, 1)]
void BuildGrid(
	uint3 GroupID : SV_GroupID,
	uint3 GroupThreadID : SV_GroupThreadID,
	uint3 DispatchThreadID : SV_DispatchThreadID,
	uint GroupIndex : SV_GroupIndex
	)
{
	uint cellID = CellValue(floor(ParticlesRO[DispatchThreadID.x].Pos.xyz / 2 / gSmoothlen));
	GridRW[DispatchThreadID.x] = uint2(cellID, DispatchThreadID.x);
}

[numthreads(NUM_THREADS, 1, 1)]
void ClearGridIndices(
	uint3 GroupID : SV_GroupID,
	uint3 GroupThreadID : SV_GroupThreadID,
	uint3 DispatchThreadID : SV_DispatchThreadID,
	uint GroupIndex : SV_GroupIndex
	)
{
	GridIndicesRW[DispatchThreadID.x] = uint2(0, 0);
}

[numthreads(NUM_THREADS, 1, 1)]
void BuildGridIndices(
	uint3 GroupID : SV_GroupID,
	uint3 GroupThreadID : SV_GroupThreadID,
	uint3 DispatchThreadID : SV_DispatchThreadID,
	uint GroupIndex : SV_GroupIndex
	)
{
	const uint G_ID = DispatchThreadID.x; // Grid ID to operate on
	uint G_ID_PREV = (G_ID == 0) ? NUM_PARTICLES : G_ID; G_ID_PREV--;
	uint G_ID_NEXT = G_ID + 1;
	if (G_ID_NEXT == NUM_PARTICLES) { G_ID_NEXT = 0; }

	uint cell = GridGetKey(GridRO[G_ID]);
	uint cell_prev = GridGetKey(GridRO[G_ID_PREV]);
	uint cell_next = GridGetKey(GridRO[G_ID_NEXT]);
	
	if (cell != cell_prev)
	{
		// I'm the start of a cell
		GridIndicesRW[cell].x = G_ID;
	}
	if (cell != cell_next)
	{
		// I'm the end of a cell
		GridIndicesRW[cell].y = G_ID + 1;
	}
}

[numthreads(NUM_THREADS, 1, 1)]
void RearrangeParticles(
	uint3 GroupID : SV_GroupID,
	uint3 GroupThreadID : SV_GroupThreadID,
	uint3 DispatchThreadID : SV_DispatchThreadID,
	uint GroupIndex : SV_GroupIndex
	)
{
	ParticlesRW[DispatchThreadID.x] = ParticlesRO[GridGetValue(GridRO[DispatchThreadID.x])];
}

[numthreads(NUM_THREADS, 1, 1)]
void CalcDensity(
	uint3 GroupID : SV_GroupID,
	uint3 GroupThreadID : SV_GroupThreadID,
	uint3 DispatchThreadID : SV_DispatchThreadID,
	uint GroupIndex : SV_GroupIndex
	)
{
	float density = 0;
	const float h_sq = gSmoothlen * gSmoothlen;
	
	float3 pos = ParticlesRO[DispatchThreadID.x].Pos.xyz;

	float3 lbf = GetLeftBottomFarCellPos(pos);
	float3 rtn = GetRightTopNearCellPos(lbf);
	float3 center = GetCellCenter(lbf, rtn);
	float3 ov = GetOctantVector(center, pos);
	float3 offsets[8];
	offsets[0] = float3(0, 0, 0);
	offsets[1] = float3(ov.x, 0, 0);
	offsets[2] = float3(ov.x, ov.y, 0);
	offsets[3] = float3(0, ov.y, 0);
	offsets[4] = float3(ov.x, 0, ov.z);
	offsets[5] = float3(ov.x, ov.y, ov.z);
	offsets[6] = float3(0, ov.y, ov.z);
	offsets[7] = float3(0, 0, ov.z);
	uint cellIDs[8];
	for (int i = 0; i < 8; ++i)
		cellIDs[i] = CellValue(floor((pos + offsets[i] * gSmoothlen) / 2 / gSmoothlen));
	for (int i = 0; i < 8; ++i)
	{
		for (int j = i; j < 8; ++j)
		{
			if (cellIDs[i] == cellIDs[j] && i != j)
				cellIDs[j] = -1;
		}
	}
	
	for (int i = 0; i < 8; ++i)
	{
		if (cellIDs[i] != -1)
		{
			uint2 indexStartEnd = GridIndicesRO[cellIDs[i]];
			for (int j = indexStartEnd.x; j < indexStartEnd.y; ++j)
			{
				float3 ipos = ParticlesRO[j].Pos.xyz;
				float3 diff = ipos - pos;
				float r_sq = dot(diff, diff);
				if (r_sq < h_sq)
				{
					density += Density(r_sq);
				}
			}
		}
	}

	ParticleDensityRW[DispatchThreadID.x].x = density;
}

[numthreads(NUM_THREADS, 1, 1)]
void CalcNormals(
	uint3 GroupID : SV_GroupID,
	uint3 GroupThreadID : SV_GroupThreadID,
	uint3 DispatchThreadID : SV_DispatchThreadID,
	uint GroupIndex : SV_GroupIndex
	)
{
	float3 normal = 0;
	const float h = gSmoothlen;

	float3 pos = ParticlesRO[DispatchThreadID.x].Pos.xyz;

	float3 lbf = GetLeftBottomFarCellPos(pos);
	float3 rtn = GetRightTopNearCellPos(lbf);
	float3 center = GetCellCenter(lbf, rtn);
	float3 ov = GetOctantVector(center, pos);
	float3 offsets[8];
	offsets[0] = float3(0, 0, 0);
	offsets[1] = float3(ov.x, 0, 0);
	offsets[2] = float3(ov.x, ov.y, 0);
	offsets[3] = float3(0, ov.y, 0);
	offsets[4] = float3(ov.x, 0, ov.z);
	offsets[5] = float3(ov.x, ov.y, ov.z);
	offsets[6] = float3(0, ov.y, ov.z);
	offsets[7] = float3(0, 0, ov.z);
	uint cellIDs[8];
	for (int i = 0; i < 8; ++i)
		cellIDs[i] = CellValue(floor((pos + offsets[i] * gSmoothlen) / 2 / gSmoothlen));
	for (int i = 0; i < 8; ++i)
	{
		for (int j = i; j < 8; ++j)
		{
			if (cellIDs[i] == cellIDs[j] && i != j)
				cellIDs[j] = -1;
		}
	}

	for (int i = 0; i < 8; ++i)
	{
		if (cellIDs[i] != -1)
		{
			uint2 indexStartEnd = GridIndicesRO[cellIDs[i]];
			for (int j = indexStartEnd.x; j < indexStartEnd.y; ++j)
			{
				float3 ipos = ParticlesRO[j].Pos.xyz;
				float idensity = ParticleDensityRO[j].x;
				if (dot(ipos-pos, ipos-pos) < h*h && DispatchThreadID.x != j)
				{
					normal += CalcNormal(pos, ipos, idensity);
				}
			}
		}
	}

	ParticleNormalsRW[DispatchThreadID.x].xyz = normal;
}

[numthreads(NUM_THREADS, 1, 1)]
void CalcForces(
	uint3 GroupID : SV_GroupID,
	uint3 GroupThreadID : SV_GroupThreadID,
	uint3 DispatchThreadID : SV_DispatchThreadID,
	uint GroupIndex : SV_GroupIndex
	)
{
	const float h_sq = gSmoothlen * gSmoothlen;
	
	float3 pos = ParticlesRO[DispatchThreadID.x].Pos.xyz;
	float3 velocity = ParticlesRO[DispatchThreadID.x].Vel.xyz;
	float density = ParticleDensityRO[DispatchThreadID.x].x;
	float pressure = CalculatePressure(density);
	float3 normal = ParticleNormalsRO[DispatchThreadID.x].xyz;

	float3 acc = float3(0, 0, 0);

	float3 lbf = GetLeftBottomFarCellPos(pos);
	float3 rtn = GetRightTopNearCellPos(lbf);
	float3 center = GetCellCenter(lbf, rtn);
	float3 ov = GetOctantVector(center, pos);
	float3 offsets[8];
	offsets[0] = float3(0, 0, 0);
	offsets[1] = float3(ov.x, 0, 0);
	offsets[2] = float3(ov.x, ov.y, 0);
	offsets[3] = float3(0, ov.y, 0);
	offsets[4] = float3(ov.x, 0, ov.z);
	offsets[5] = float3(ov.x, ov.y, ov.z);
	offsets[6] = float3(0, ov.y, ov.z);
	offsets[7] = float3(0, 0, ov.z);
	uint cellIDs[8];
	for (int i = 0; i < 8; ++i)
		cellIDs[i] = CellValue(floor((pos + offsets[i] * gSmoothlen) / 2 / gSmoothlen));
	for (int i = 0; i < 8; ++i)
	{
		for (int j = i; j < 8; ++j)
		{
			if (cellIDs[i] == cellIDs[j] && i != j)
				cellIDs[j] = -1;
		}
	}
	
	for (int i = 0; i < 8; ++i)
	{
		if (cellIDs[i] != -1)
		{
			uint2 indexStartEnd = GridIndicesRO[cellIDs[i]];
			for (int j = indexStartEnd.x; j < indexStartEnd.y; ++j)
			{
				float3 ipos = ParticlesRO[j].Pos.xyz;
				float3 inormal = ParticleNormalsRO[j].xyz;
				float3 diff = ipos - pos;
				float r_sq = dot(diff, diff);
				if (r_sq < h_sq && DispatchThreadID.x != j)
				{
					float3 ivelocity = ParticlesRO[j].Vel.xyz;
					float idensity = ParticleDensityRO[j].x;
					float ipressure = CalculatePressure(idensity);
					float r = sqrt(r_sq);

					// Pressure Term
					acc += CalculateGradPressure(r, pressure, ipressure, idensity, diff);

					// Viscosity Term
					acc += CalculateLapVelocity(r, velocity, ivelocity, idensity);

					// Cohesion term
					float CoordFactor = 2 * gRestDensity / (density + idensity);
					acc += CoordFactor * (CalcCohesionForce(pos, ipos) + CalcCurvatureForce(normal, inormal));
				}
			}
		}
	}

	ParticleForcesRW[DispatchThreadID.x].xyz = acc / density;
}

[numthreads(NUM_THREADS, 1, 1)]
void Integrate(
	uint3 GroupID : SV_GroupID,
	uint3 GroupThreadID : SV_GroupThreadID,
	uint3 DispatchThreadID : SV_DispatchThreadID,
	uint GroupIndex : SV_GroupIndex
	)
{
	float3 position = ParticlesRO[DispatchThreadID.x].Pos.xyz;
	float3 velocity = ParticlesRO[DispatchThreadID.x].Vel.xyz;
	float3 acceleration = ParticleForcesRO[DispatchThreadID.x].xyz;

	// Apply gravity
	if(ParticlesRO[DispatchThreadID.x].Pos.y < 0.4f || gDropFlag)
		acceleration += gGravity;

	// Integrate
	float cubeSz = pow(NUM_PARTICLES, 1.0f / 3.0f) * 2.0f * gRadius;
	float centerX = cubeSz / 2.0f;
	float centerZ = cubeSz / 2.0f;
	float lenX = cubeSz * 1.1f;
	float lenZ = cubeSz * 1.1f;
	velocity += gDeltaTime * acceleration;
	position += gDeltaTime * velocity;
	if (position.x <= centerX - lenX / 2.0f)
	{
		velocity.x *= -gObstacleStiffness;
		position += normalize(velocity) * abs(position.x - (centerX - lenX / 2.0f));
	}

	if (position.x >= centerX + lenX / 2.0f)
	{
		velocity.x *= -gObstacleStiffness;
		position += normalize(velocity) * abs(position.x - (centerX + lenX / 2.0f));
	}
	if (position.z <= centerZ - lenZ / 2.0f)
	{
		velocity.z *= -gObstacleStiffness;
		position += normalize(velocity) * abs(position.z - (centerZ - lenZ / 2.0f));
	}

	if (position.z >= centerZ + lenZ / 2.0f)
	{
		velocity.z *= -gObstacleStiffness;
		position += normalize(velocity) * abs(position.z - (centerZ + lenZ / 2.0f));
	}

	if (position.y <= 0.0f)
	{
		velocity.y *= -gObstacleStiffness;
		position += normalize(velocity) * abs(position.y);
	}

	// Update
	ParticlesRW[DispatchThreadID.x].Pos.xyz = position;
	ParticlesRW[DispatchThreadID.x].Vel.xyz = velocity;
}

technique11 BuildGridTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, BuildGrid()));
	}
};

technique11 ClearGridIndicesTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, ClearGridIndices()));
	}
}

technique11 BuildGridIndicesTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, BuildGridIndices()));
	}
}

technique11 RearrangeParticlesTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, RearrangeParticles()));
	}
}

technique11 CalcDensityTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, CalcDensity()));
	}
}

technique11 CalcNormalTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, CalcNormals()));
	}
};

technique11 CalcForcesTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, CalcForces()));
	}
}

technique11 IntegrateTech
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, Integrate()));
	}
}

