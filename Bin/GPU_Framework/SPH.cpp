#include "SPH.h"

SPH::SPH(ID3D11Device* device, ID3D11DeviceContext* dc, ID3DX11Effect* fx,
	ID3DX11EffectMatrixVariable* wvp, ID3DX11EffectMatrixVariable* world,
	ID3DX11EffectMatrixVariable* view, ID3DX11EffectMatrixVariable* proj,
	std::map<DrawSceneType, XMFLOAT2>&RTsSizes, FLOAT ClientH, FLOAT ClientW) :
	mSPH_ComputeFX(0), mSPH_ComputeTech(0),
	mfxSPH_ParticlesBufferUAV(0), mfxSPH_GridBufferUAV(0), mfxSPH_GridIndicesBufferUAV(0),
	mSPH_ParticlesBufferUAV(0), mSPH_GridBufferUAV(0), mSPH_GridIndicesBufferUAV(0),
	mfxSPH_ParticlesBufferSRV(0), mfxSPH_GridBufferSRV(0), mfxSPH_GridIndicesBufferSRV(0),
	mSPH_ParticlesBufferSRV(0), mSPH_GridBufferSRV(0), mSPH_GridIndicesBufferSRV(0),
	mfxWorld(0), mfxWorldViewProj(0), mfxDeltaTime(0), mSPH_TempBufferUAV(0),
	mfxSPH_SortDataBufferUAV(0), mfxSPH_SortInputBufferSRV(0), mSPH_TempBuffer(0),
	mRenderRadius(RENDER_RADIUS), mScale(100.0f), mMass(0.0002), mRadius(RADIUS),
	mSmoothlen(4*RADIUS), mPressureStiffness(50.0f/*20.0f*/), mRestDensity(1000.0f),
	mViscosity(2.0f), mObstacleStiffness(0.6f), mObstacleFriction(0.4f), mDeltaTime(0.005f),
	mSurfaceTensionCoef(20000.0f),
	mSPH_DrawFX(0), mSPH_BuildNormalWMapTech(0), mSPH_BuildPosWMapTech(0), mSPH_DrawSPHFluidTech(0),
	pBlurFilter(0), mSPH_ShadowMapBlurFilter(0),
	mSPH_ParticlesInputLayout(0), mSPH_DrawParticlesInputLayout(0),
	mfxSPH_NormalMap(0),
	mSPH_PosWMapRTV(0), mSPH_PosWMapSRV(0), mSPH_PosWMapUAV(0),
	mSPH_NormalWMapRTV(0), mSPH_NormalWMapSRV(0),
	mSPH_ShadowMapRTV(0), mSPH_ShadowMapSRV(0), mSPH_ShadowMapUAV(0),
	mSPH_PlanarRefractionMapDSV(0), mSPH_PlanarRefractionMapSRV(0), mSPH_PlanarRefractionMapRTV(0),
	mDropFlag(false), mGridOrigin(XMFLOAT3(powf(NUM_PARTICLES, 1.0f / 3.0f) * RADIUS, powf(NUM_PARTICLES, 1.0f / 3.0f) * RADIUS, powf(NUM_PARTICLES, 1.0f / 3.0f) * RADIUS)),
	mGridCellSize(3.0f * powf(NUM_PARTICLES, 1.0f / 3.0f) * RADIUS / (float)MC_GRID_SIDE_SZ), mDensityIsoValue(800.0f),
	mFluidSurface(0), mFluidSurfaceUAV(0), mSPH_GenerateSurfaceFX(0), mSPH_GenerateSurfaceTech(0),
	mfxSPH_FluidSurafceUAV(0), mfxSPH_GridIndicesSRV(0), mfxSPH_ParticlesDensitySRV(0),
	mfxSPH_ParticlesNormalsSRV(0), mfxSPH_GridOrigin(0), mfxSPH_GridCellSize(0), mfxSPH_DensityIsoValue(0),
	mFluidSurfaceCopy(0), mFluidSurfaceDrawable(0), mVertsCount(0)
{
	SPHInit(device, dc, fx, wvp, world, view, proj, RTsSizes, ClientH, ClientW);
}

void SPH::SPHInit(ID3D11Device* device, ID3D11DeviceContext* dc, ID3DX11Effect* fx,
	ID3DX11EffectMatrixVariable* wvp, ID3DX11EffectMatrixVariable* world,
	ID3DX11EffectMatrixVariable* view, ID3DX11EffectMatrixVariable* proj,
	std::map<DrawSceneType, XMFLOAT2>&RTsSizes, FLOAT ClientH, FLOAT ClientW)
{	
	md3dDevice = device;
	md3dImmediateContext = dc;
	mSPH_DrawFX = fx;
	mfxWorldViewProj = wvp;
	mfxWorld = world;
	mfxView = view;
	mfxProj = proj;

	pRenderStates = new RenderStates(md3dDevice);
	mScaleMatrix = XMMatrixScaling(mScale, mScale, mScale);

	InitViewport(&mSPH_ShadowMapVP, 0.0f, 0.0f, RTsSizes[DrawSceneType::DRAW_SCENE_TO_SHADOWMAP_CASCADE_RT].x, RTsSizes[DrawSceneType::DRAW_SCENE_TO_SHADOWMAP_CASCADE_RT].y, 0.0f, 1.0f);
	InitViewport(&mSPH_PlanarRefractionMapVP, 0.0f, 0.0f, RTsSizes[DrawSceneType::DRAW_SCENE_TO_SPH_REFRACTION_RT].x, RTsSizes[DrawSceneType::DRAW_SCENE_TO_SPH_REFRACTION_RT].y, 0.0f, 1.0f);

	mSPH_ShadowMapBlurFilter = new BlurFilter(md3dDevice, RTsSizes[DrawSceneType::DRAW_SCENE_TO_SHADOWMAP_CASCADE_RT].x, RTsSizes[DrawSceneType::DRAW_SCENE_TO_SHADOWMAP_CASCADE_RT].y, DXGI_FORMAT_R32G32B32A32_FLOAT, false, 0);
	pBlurFilter = new BlurFilter(md3dDevice, ClientW, ClientH, DXGI_FORMAT_R32G32B32A32_FLOAT, false, 0);

	mDensityCoef = mMass * 315.0f / (64.0f * XM_PI * pow(mSmoothlen, 9.0f));
	mLapViscosityCoef = mMass * mViscosity * 45.0f / (XM_PI * pow(mSmoothlen, 6.0f));
	mGradPressureCoef = mMass * -45.0f / (XM_PI * pow(mSmoothlen, 6.0));

	SPHBuildFX();
	SPHBuildVertexInputLayouts();
	SPHCreateBuffersAndViews();
	SPHBuildDrawBuffers();
	OnSize(ClientH, ClientW);
}

void SPH::SPHSetup()
{
	mfxMass->SetFloat(mMass);
	mfxRadius->SetFloat(mRadius);
	mfxSmoothlen->SetFloat(mSmoothlen);
	mfxPressureStiffness->SetFloat(mPressureStiffness);
	mfxRestDensity->SetFloat(mRestDensity);
	mfxViscosity->SetFloat(mViscosity);
	mfxObstacleStiffness->SetFloat(mObstacleStiffness);
	mfxObstacleFriction->SetFloat(mObstacleFriction);
	mfxDeltaTime->SetFloat(mDeltaTime);
	mfxDensityCoef->SetFloat(mDensityCoef);
	mfxGradPressureCoef->SetFloat(mGradPressureCoef);
	mfxLapViscosityCoef->SetFloat(mLapViscosityCoef);
	mfxSurfaceTensionCoef->SetFloat(mSurfaceTensionCoef);
	mfxDropFlag->SetBool(mDropFlag);
}

void SPH::Compute() 
{
	SPHSetup();
	SPHBuildGrid();
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	md3dImmediateContext->CSSetShader(0, 0, 0);
	
	SPHSort();
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	md3dImmediateContext->CSSetShader(0, 0, 0);
	
	SPHSetup();
	SPHClearGridIndices();
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	md3dImmediateContext->CSSetShader(0, 0, 0);
	
	SPHSetup();
	SPHBuildGridIndices();
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	md3dImmediateContext->CSSetShader(0, 0, 0);
	
	SPHSetup();
	SPHRearrangeParticles();
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	md3dImmediateContext->CSSetShader(0, 0, 0);

	SPHSetup();
	SPHCalcDensity();
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	md3dImmediateContext->CSSetShader(0, 0, 0);

	SPHSetup();
	SPHCalcNormals();
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	md3dImmediateContext->CSSetShader(0, 0, 0);

	SPHSetup();
	SPHCalcForces();
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	md3dImmediateContext->CSSetShader(0, 0, 0);

	SPHSetup();
	SPHIntegrate();
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	md3dImmediateContext->CSSetShader(0, 0, 0);

	/*SPHClearSurfaceBuffer();
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	md3dImmediateContext->CSSetShader(0, 0, 0);

	SPHGenerateSurface();
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	md3dImmediateContext->CSSetShader(0, 0, 0);*/
}

void SPH::SPHBuildGrid()
{
	D3DX11_TECHNIQUE_DESC techDesc;

	mfxSPH_GridBufferUAV->SetUnorderedAccessView(mSPH_GridBufferUAV);
	mfxSPH_ParticlesBufferSRV->SetResource(mSPH_ParticlesBufferSRV);

	mSPH_BuildGridTech->GetDesc(&techDesc);

	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		mSPH_BuildGridTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);

		md3dImmediateContext->Dispatch(NUM_THREAD_GROUPS, 1, 1);
	}

	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
}

void SPH::SPHClearGridIndices()
{
	D3DX11_TECHNIQUE_DESC techDesc;

	mfxSPH_GridIndicesBufferUAV->SetUnorderedAccessView(mSPH_GridIndicesBufferUAV);

	mSPH_ClearGridIndicesTech->GetDesc(&techDesc);

	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		mSPH_ClearGridIndicesTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);

		md3dImmediateContext->Dispatch(NUM_THREAD_GROUPS, 1, 1);
	}

	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
}

void SPH::SPHBuildGridIndices()
{
	D3DX11_TECHNIQUE_DESC techDesc;

	mfxSPH_GridBufferSRV->SetResource(mSPH_GridBufferSRV);
	mfxSPH_GridIndicesBufferUAV->SetUnorderedAccessView(mSPH_GridIndicesBufferUAV);

	mSPH_BuildGridIndicesTech->GetDesc(&techDesc);

	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		mSPH_BuildGridIndicesTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);

		md3dImmediateContext->Dispatch(NUM_THREAD_GROUPS, 1, 1);
	}

	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
}

void SPH::SPHSort()
{
	for (UINT level = 2; level <= BITONIC_BLOCK_SIZE; level <<= 1)
	{
		mfxLevel->SetInt(level);
		mfxLevelMask->SetInt(level);
		mfxWidth->SetInt(MATRIX_HEIGHT);
		mfxHeight->SetInt(MATRIX_WIDTH);

		mfxSPH_SortDataBufferUAV->SetUnorderedAccessView(mSPH_GridBufferUAV);

		D3DX11_TECHNIQUE_DESC techDesc;
		mSPH_BitonicSortTech->GetDesc(&techDesc);

		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			mSPH_BitonicSortTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
			md3dImmediateContext->Dispatch(NUM_CELLS/BITONIC_BLOCK_SIZE, 1, 1);
		}
		md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
		md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);

	}

	for (UINT level = (BITONIC_BLOCK_SIZE << 1); level <= NUM_CELLS; level <<= 1)
	{
		mfxLevel->SetInt(level/BITONIC_BLOCK_SIZE);
		mfxLevelMask->SetInt((level & ~NUM_CELLS)/BITONIC_BLOCK_SIZE);
		mfxWidth->SetInt(MATRIX_WIDTH);
		mfxHeight->SetInt(MATRIX_HEIGHT);

		mfxSPH_SortDataBufferUAV->SetUnorderedAccessView(mSPH_TempBufferUAV);
		mfxSPH_SortInputBufferSRV->SetResource(mSPH_GridBufferSRV);

		D3DX11_TECHNIQUE_DESC techDesc;
		mSPH_MatrixTransposeTech->GetDesc(&techDesc);

		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			mSPH_MatrixTransposeTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
			md3dImmediateContext->Dispatch(MATRIX_WIDTH / TRANSPOSE_BLOCK_SIZE, MATRIX_HEIGHT / TRANSPOSE_BLOCK_SIZE, 1);
		}

		mSPH_BitonicSortTech->GetDesc(&techDesc);

		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			mSPH_BitonicSortTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
			md3dImmediateContext->Dispatch(NUM_CELLS / BITONIC_BLOCK_SIZE, 1, 1);
		}
		md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
		md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);

		mfxLevel->SetInt(BITONIC_BLOCK_SIZE);
		mfxLevelMask->SetInt(level);
		mfxWidth->SetInt(MATRIX_HEIGHT);
		mfxHeight->SetInt(MATRIX_WIDTH);

		mfxSPH_SortDataBufferUAV->SetUnorderedAccessView(mSPH_GridBufferUAV);
		mfxSPH_SortInputBufferSRV->SetResource(mSPH_TempBufferSRV);

		mSPH_MatrixTransposeTech->GetDesc(&techDesc);

		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			mSPH_MatrixTransposeTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
			md3dImmediateContext->Dispatch(MATRIX_HEIGHT / TRANSPOSE_BLOCK_SIZE, MATRIX_WIDTH / TRANSPOSE_BLOCK_SIZE, 1);
		}

		mSPH_BitonicSortTech->GetDesc(&techDesc);

		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			mSPH_BitonicSortTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);

			md3dImmediateContext->Dispatch(NUM_CELLS / BITONIC_BLOCK_SIZE, 1, 1);
		}
		md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
		md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	}
}

void SPH::SPHRearrangeParticles()
{
	D3DX11_TECHNIQUE_DESC techDesc;

	mfxSPH_GridBufferSRV->SetResource(mSPH_GridBufferSRV);
	mfxSPH_ParticlesBufferSRV->SetResource(mSPH_ParticlesBufferSRV);
	mfxSPH_SortedParticlesBufferUAV->SetUnorderedAccessView(mSPH_SortedParticlesBufferUAV);

	mSPH_RearrangeParticlesTech->GetDesc(&techDesc);

	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		mSPH_RearrangeParticlesTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);

		md3dImmediateContext->Dispatch(NUM_THREAD_GROUPS, 1, 1);
	}
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
}

void SPH::SPHCalcDensity()
{
	D3DX11_TECHNIQUE_DESC techDesc;

	mfxSPH_ParticlesBufferSRV->SetResource(mSPH_SortedParticlesBufferSRV);
	mfxSPH_GridBufferSRV->SetResource(mSPH_GridBufferSRV);
	mfxSPH_GridIndicesBufferSRV->SetResource(mSPH_GridIndicesBufferSRV);

	mfxSPH_ParticleDensityBufferUAV->SetUnorderedAccessView(mSPH_ParticleDensityBufferUAV);

	mSPH_CalcDensityTech->GetDesc(&techDesc);

	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		mSPH_CalcDensityTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);

		md3dImmediateContext->Dispatch(NUM_THREAD_GROUPS, 1, 1);
	}
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
}

void SPH::SPHCalcNormals()
{
	D3DX11_TECHNIQUE_DESC techDesc;

	mfxSPH_ParticlesBufferSRV->SetResource(mSPH_SortedParticlesBufferSRV);
	mfxSPH_GridBufferSRV->SetResource(mSPH_GridBufferSRV);
	mfxSPH_GridIndicesBufferSRV->SetResource(mSPH_GridIndicesBufferSRV);
	mfxSPH_ParticleDensityBufferSRV->SetResource(mSPH_ParticleDensityBufferSRV);

	mfxSPH_ParticleNormalsBufferUAV->SetUnorderedAccessView(mSPH_ParticleNormalsBufferUAV);

	mSPH_CalcNormalTech->GetDesc(&techDesc);

	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		mSPH_CalcNormalTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);

		md3dImmediateContext->Dispatch(NUM_THREAD_GROUPS, 1, 1);
	}
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
}

void SPH::SPHCalcForces()
{
	D3DX11_TECHNIQUE_DESC techDesc;

	mfxSPH_ParticlesBufferSRV->SetResource(mSPH_SortedParticlesBufferSRV);
	mfxSPH_GridBufferSRV->SetResource(mSPH_GridBufferSRV);
	mfxSPH_GridIndicesBufferSRV->SetResource(mSPH_GridIndicesBufferSRV);
	mfxSPH_ParticleNormalsBufferSRV->SetResource(mSPH_ParticleNormalsBufferSRV);

	mfxSPH_ParticleForcesBufferUAV->SetUnorderedAccessView(mSPH_ParticleForcesBufferUAV);
	mfxSPH_ParticleDensityBufferSRV->SetResource(mSPH_ParticleDensityBufferSRV);

	mSPH_CalcForcesTech->GetDesc(&techDesc);

	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		mSPH_CalcForcesTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);

		md3dImmediateContext->Dispatch(NUM_THREAD_GROUPS, 1, 1);
	}
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
}

void SPH::SPHIntegrate()
{
	D3DX11_TECHNIQUE_DESC techDesc;

	mfxSPH_ParticlesBufferSRV->SetResource(mSPH_SortedParticlesBufferSRV);
	mfxSPH_GridBufferSRV->SetResource(mSPH_GridBufferSRV);
	mfxSPH_GridIndicesBufferSRV->SetResource(mSPH_GridIndicesBufferSRV);

	mfxSPH_SortedParticlesBufferUAV->SetUnorderedAccessView(mSPH_ParticlesBufferUAV);
	mfxSPH_ParticleForcesBufferSRV->SetResource(mSPH_ParticleForcesBufferSRV);

	mfxDeltaTime->SetFloat(mDeltaTime);

	mSPH_IntegrateTech->GetDesc(&techDesc);

	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		mSPH_IntegrateTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);

		md3dImmediateContext->Dispatch(NUM_THREAD_GROUPS, 1, 1);
	}
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
}

void SPH::SPHClearSurfaceBuffer()
{
	UINT initialCount = 0;
	md3dImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 1, 1, &mFluidSurfaceUAV, &initialCount);
	md3dImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 1, 1, pNullUAV, &initialCount);
	
	D3DX11_TECHNIQUE_DESC techDesc;
	mfxSPH_FluidSurafceUAV->SetUnorderedAccessView(mFluidSurfaceUAV);
	mSPH_ClearSurfaceBufferTech->GetDesc(&techDesc);
	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		mSPH_ClearSurfaceBufferTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
		md3dImmediateContext->Dispatch(round((float)MC_TRIS_BUF_SZ / (float)NUM_THREADS), 1, 1);
	}

	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
}

void SPH::SPHGenerateSurface()
{
	mfxSPH_FluidSurafceUAV->SetUnorderedAccessView(mFluidSurfaceUAV);
	mfxSPH_GridIndicesSRV->SetResource(mSPH_GridIndicesBufferSRV);
	mfxSPH_ParticlesDensitySRV->SetResource(mSPH_ParticleDensityBufferSRV);
	mfxSPH_ParticleNormalsBufferSRV->SetResource(mSPH_ParticleNormalsBufferSRV);
	mfxSPH_ParticlesSRV->SetResource(mSPH_SortedParticlesBufferSRV);

	mfxSPH_GridOrigin->SetFloatVector(reinterpret_cast<const float*>(&mGridOrigin));
	mfxSPH_GridCellSize->SetFloat(mGridCellSize);
	mfxSPH_DensityIsoValue->SetFloat(mDensityIsoValue);
	mfxSPH_SmoothingLength->SetFloat(mSmoothlen);

	D3DX11_TECHNIQUE_DESC techDesc;
	mSPH_GenerateSurfaceTech->GetDesc(&techDesc);
	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		mSPH_GenerateSurfaceTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
		md3dImmediateContext->Dispatch(MC_GRID_BUILD_THREAD_GROUP_COUNT, MC_GRID_BUILD_THREAD_GROUP_COUNT, MC_GRID_BUILD_THREAD_GROUP_COUNT);
	}

	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);

	md3dImmediateContext->CopyResource(mFluidSurfaceCopy, mFluidSurface);
	D3D11_MAPPED_SUBRESOURCE debugMappedData, viewMappedData;
	md3dImmediateContext->Map(mFluidSurfaceCopy, 0, D3D11_MAP_READ, 0, &debugMappedData);
	md3dImmediateContext->Map(mFluidSurfaceDrawable, 0, D3D11_MAP_WRITE_DISCARD, 0, &viewMappedData);
	FluidTriangle *debug = reinterpret_cast<FluidTriangle*>(debugMappedData.pData);
	FluidVertex *view = reinterpret_cast<FluidVertex*>(viewMappedData.pData);
	for (mVertsCount = 0; mVertsCount < MC_VERTS_BUF_SZ &&
		debug[mVertsCount / 3].v[mVertsCount % 3].Flag.x >= 0.0f &&
		debug[mVertsCount / 3].v[mVertsCount % 3].Flag.y >= 0.0f;
		++mVertsCount)
	{
		view[mVertsCount].Pos = debug[mVertsCount / 3].v[mVertsCount % 3].Pos;
		view[mVertsCount].Normal = debug[mVertsCount / 3].v[mVertsCount % 3].Normal;
		view[mVertsCount].Flag = debug[mVertsCount / 3].v[mVertsCount % 3].Flag;
		view[mVertsCount].Tangent = debug[mVertsCount / 3].v[mVertsCount % 3].Tangent;
	}
	md3dImmediateContext->Unmap(mFluidSurfaceDrawable, 0);
	md3dImmediateContext->Unmap(mFluidSurfaceCopy, 0);
	/*char str[100];
	sprintf(str, "VertsCount = %d", mVertsCount);
	MessageBoxA(NULL, str, str, MB_OK);*/
}

void SPH::Draw(XMMATRIX wvp, XMMATRIX world, XMMATRIX view, XMMATRIX proj, ID3D11DepthStencilView* depthStencilView, ID3D11RenderTargetView** renderTargetView, DrawPassType dpt)
{
	if (dpt != DrawPassType::DRAW_PASS_TYPE_SHADOW_MAP_BUILD)
	{
		SPHBuildPosWMap(wvp, world, view, proj, depthStencilView, mSPH_PosWMapRTV, mScreenViewport);
		SPHBlurPosWMap();
		SPHBuildNormalWMap(proj);
		SPHDrawFluid(view, proj, renderTargetView, mScreenViewport, mSPH_PosWMapSRV);
	}
	else
	{
		SPHBuildPosWMap(wvp, world, view, proj, depthStencilView, mSPH_ShadowMapRTV, mSPH_ShadowMapVP);
		SPHBlurSMDataMap();
		SPHDrawFluid(view, proj, renderTargetView, mSPH_ShadowMapVP, mSPH_ShadowMapSRV);
	}
}

void SPH::SPHBuildPosWMap(XMMATRIX wvp, XMMATRIX world, XMMATRIX view, XMMATRIX proj, ID3D11DepthStencilView* depthStencilView, ID3D11RenderTargetView* renderTargetView, D3D11_VIEWPORT vp)
{
	md3dImmediateContext->OMSetRenderTargets(1, &renderTargetView, depthStencilView);
	md3dImmediateContext->ClearRenderTargetView(renderTargetView, Colors::Black);
	md3dImmediateContext->RSSetViewports(1, &vp);

	md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	md3dImmediateContext->IASetInputLayout(mSPH_ParticlesInputLayout);

	const UINT stride = sizeof(Particle);
	const UINT offset = 0;
	md3dImmediateContext->IASetVertexBuffers(0, 1, &mSPH_ParticlesBuffer, &stride, &offset);

	mfxWorld->SetMatrix(reinterpret_cast<float*>(&(mScaleMatrix*world)));
	mfxView->SetMatrix(reinterpret_cast<float*>(&view));
	mfxProj->SetMatrix(reinterpret_cast<float*>(&proj));
	mfxWorldViewProj->SetMatrix(reinterpret_cast<float*>(&wvp));

	mfxScale->SetFloat(mScale);
	mfxRenderRadius->SetFloat(mRenderRadius);

	mfxSPH_NormalMap->SetResource(mSPH_NormalMapSRV);
	
	D3DX11_TECHNIQUE_DESC techDesc;
	mSPH_BuildPosWMapTech->GetDesc(&techDesc);
	
	for (int p = 0; p < techDesc.Passes; ++p)
	{
		mSPH_BuildPosWMapTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
		md3dImmediateContext->Draw(NUM_PARTICLES, 0);
	}
}

void SPH::SPHBlurPosWMap()
{
	ID3D11RenderTargetView* nullSRV[1] = { 0 };
	md3dImmediateContext->OMSetRenderTargets(1, nullSRV, 0);
	pBlurFilter->BlurInPlace(md3dImmediateContext, mSPH_PosWMapSRV, mSPH_PosWMapUAV, SPH_BLUR_COUNT);
}

void SPH::SPHBlurSMDataMap()
{
	ID3D11RenderTargetView* nullSRV[1] = { 0 };
	md3dImmediateContext->OMSetRenderTargets(1, nullSRV, 0);
	mSPH_ShadowMapBlurFilter->BlurInPlace(md3dImmediateContext, mSPH_ShadowMapSRV, mSPH_ShadowMapUAV, SPH_BLUR_COUNT);
}

void SPH::SPHBuildNormalWMap(XMMATRIX proj)
{
	md3dImmediateContext->OMSetRenderTargets(1, &mSPH_NormalWMapRTV, 0);
	md3dImmediateContext->ClearRenderTargetView(mSPH_NormalWMapRTV, Colors::Black);
	md3dImmediateContext->RSSetViewports(1, &mScreenViewport);

	md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	md3dImmediateContext->IASetInputLayout(mSPH_DrawParticlesInputLayout);

	const UINT stride = sizeof(ParticleDraw);
	const UINT offset = 0;
	md3dImmediateContext->IASetVertexBuffers(0, 1, &mScreenQuadVB, &stride, &offset);
	md3dImmediateContext->IASetIndexBuffer(mScreenQuadIB, DXGI_FORMAT_R16_UINT, 0);

	mfxProj->SetMatrix(reinterpret_cast<float*>(&proj));
	mfxSPH_PosWMap->SetResource(mSPH_PosWMapSRV);

	D3DX11_TECHNIQUE_DESC techDesc;
	mSPH_BuildNormalWMapTech->GetDesc(&techDesc);

	for (int p = 0; p < techDesc.Passes; ++p)
	{
		mSPH_BuildNormalWMapTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
		md3dImmediateContext->DrawIndexed(6, 0, 0);
	}
}

void SPH::SPHDrawFluid(XMMATRIX view, XMMATRIX proj, ID3D11RenderTargetView** renderTargetView, D3D11_VIEWPORT vp,
	ID3D11ShaderResourceView* posWMapSRV)
{
	md3dImmediateContext->OMSetRenderTargets(1, renderTargetView, 0);
	md3dImmediateContext->RSSetViewports(1, &vp);

	md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	md3dImmediateContext->IASetInputLayout(mSPH_DrawParticlesInputLayout);
	md3dImmediateContext->RSSetState(pRenderStates->NoCullRS);

	const UINT stride = sizeof(ParticleDraw);
	const UINT offset = 0;
	md3dImmediateContext->IASetVertexBuffers(0, 1, &mScreenQuadVB, &stride, &offset);
	md3dImmediateContext->IASetIndexBuffer(mScreenQuadIB, DXGI_FORMAT_R16_UINT, 0);

	mfxWorldViewProj->SetMatrix(reinterpret_cast<float*>(&(view*proj)));
	mfxView->SetMatrix(reinterpret_cast<float*>(&view));
	mfxProj->SetMatrix(reinterpret_cast<float*>(&proj));

	mfxSPH_PosWMap->SetResource(posWMapSRV);
	mfxSPH_NormalWMap->SetResource(mSPH_NormalWMapSRV);
	
	D3DX11_TECHNIQUE_DESC techDesc;
	mSPH_DrawSPHFluidTech->GetDesc(&techDesc);

	for (int p = 0; p < techDesc.Passes; ++p)
	{
		mSPH_DrawSPHFluidTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
		md3dImmediateContext->DrawIndexed(6, 0, 0);
	}
}

void SPH::SPHBuildFX()
{
	mSPH_ComputeFX = d3dHelper::CreateEffectFromMemory(L"FX/SPHFluidCompute.cso", md3dDevice);
	mSPH_SortFX = d3dHelper::CreateEffectFromMemory(L"FX/SPHFluidSort.cso", md3dDevice);

	mSPH_ComputeTech = mSPH_ComputeFX->GetTechniqueByName("SPH");

	mSPH_BuildGridTech = mSPH_ComputeFX->GetTechniqueByName("BuildGridTech");
	mSPH_ClearGridIndicesTech = mSPH_ComputeFX->GetTechniqueByName("ClearGridIndicesTech");
	mSPH_BuildGridIndicesTech = mSPH_ComputeFX->GetTechniqueByName("BuildGridIndicesTech");
	mSPH_BitonicSortTech = mSPH_SortFX->GetTechniqueByName("BitonicSortTech");
	mSPH_MatrixTransposeTech = mSPH_SortFX->GetTechniqueByName("MatrixTransposeTech");
	mSPH_RearrangeParticlesTech = mSPH_ComputeFX->GetTechniqueByName("RearrangeParticlesTech");
	mSPH_CalcDensityTech = mSPH_ComputeFX->GetTechniqueByName("CalcDensityTech");
	mSPH_CalcForcesTech = mSPH_ComputeFX->GetTechniqueByName("CalcForcesTech");
	mSPH_IntegrateTech = mSPH_ComputeFX->GetTechniqueByName("IntegrateTech");
	mSPH_CalcNormalTech = mSPH_ComputeFX->GetTechniqueByName("CalcNormalTech");

	mfxSPH_ParticlesBufferSRV = mSPH_ComputeFX->GetVariableByName("ParticlesRO")->AsShaderResource();
	mfxSPH_SortedParticlesBufferUAV = mSPH_ComputeFX->GetVariableByName("ParticlesRW")->AsUnorderedAccessView();
	
	mfxSPH_GridBufferUAV = mSPH_ComputeFX->GetVariableByName("GridRW")->AsUnorderedAccessView();
	mfxSPH_GridBufferSRV = mSPH_ComputeFX->GetVariableByName("GridRO")->AsShaderResource();
	
	mfxSPH_GridIndicesBufferUAV = mSPH_ComputeFX->GetVariableByName("GridIndicesRW")->AsUnorderedAccessView();
	mfxSPH_GridIndicesBufferSRV = mSPH_ComputeFX->GetVariableByName("GridIndicesRO")->AsShaderResource();
	
	mfxSPH_SortInputBufferSRV = mSPH_SortFX->GetVariableByName("Input")->AsShaderResource();
	mfxSPH_SortDataBufferUAV = mSPH_SortFX->GetVariableByName("Data")->AsUnorderedAccessView();
	
	mfxSPH_ParticleDensityBufferUAV = mSPH_ComputeFX->GetVariableByName("ParticleDensityRW")->AsUnorderedAccessView();
	mfxSPH_ParticleDensityBufferSRV = mSPH_ComputeFX->GetVariableByName("ParticleDensityRO")->AsShaderResource();

	mfxSPH_ParticleForcesBufferUAV = mSPH_ComputeFX->GetVariableByName("ParticleForcesRW")->AsUnorderedAccessView();
	mfxSPH_ParticleForcesBufferSRV = mSPH_ComputeFX->GetVariableByName("ParticleForcesRO")->AsShaderResource();

	mfxSPH_ParticleNormalsBufferUAV = mSPH_ComputeFX->GetVariableByName("ParticleNormalsRW")->AsUnorderedAccessView();
	mfxSPH_ParticleNormalsBufferSRV = mSPH_ComputeFX->GetVariableByName("ParticleNormalsRO")->AsShaderResource();

	mfxLevel = mSPH_SortFX->GetVariableByName("gLevel")->AsScalar();
	mfxLevelMask = mSPH_SortFX->GetVariableByName("gLevelMask")->AsScalar();
	mfxWidth = mSPH_SortFX->GetVariableByName("gWidth")->AsScalar();
	mfxHeight = mSPH_SortFX->GetVariableByName("gHeight")->AsScalar();

	mfxDeltaTime = mSPH_ComputeFX->GetVariableByName("gDeltaTime")->AsScalar();
	mfxMass = mSPH_ComputeFX->GetVariableByName("gMass")->AsScalar();
	mfxRadius = mSPH_ComputeFX->GetVariableByName("gRadius")->AsScalar();
	mfxSmoothlen = mSPH_ComputeFX->GetVariableByName("gSmoothlen")->AsScalar();
	mfxPressureStiffness = mSPH_ComputeFX->GetVariableByName("gPressureStiffness")->AsScalar();
	mfxRestDensity = mSPH_ComputeFX->GetVariableByName("gRestDensity")->AsScalar();
	mfxViscosity = mSPH_ComputeFX->GetVariableByName("gViscosity")->AsScalar();
	mfxObstacleStiffness = mSPH_ComputeFX->GetVariableByName("gObstacleStiffness")->AsScalar();
	mfxObstacleFriction = mSPH_ComputeFX->GetVariableByName("gObstacleFriction")->AsScalar();
	mfxDensityCoef = mSPH_ComputeFX->GetVariableByName("gDensityCoef")->AsScalar();
	mfxGradPressureCoef = mSPH_ComputeFX->GetVariableByName("gGradPressureCoef")->AsScalar();
	mfxLapViscosityCoef = mSPH_ComputeFX->GetVariableByName("gLapViscosityCoef")->AsScalar();
	mfxSurfaceTensionCoef = mSPH_ComputeFX->GetVariableByName("gSurfaceTensionCoef")->AsScalar();
	mfxDropFlag = mSPH_ComputeFX->GetVariableByName("gDropFlag")->AsScalar();

	mSPH_BuildPosWMapTech = mSPH_DrawFX->GetTechniqueByName("BuildPosWMapTech");
	mfxSPH_NormalMap = mSPH_DrawFX->GetVariableByName("gWaterParticleNormalMap")->AsShaderResource();
	mfxRenderRadius = mSPH_DrawFX->GetVariableByName("gRadius")->AsScalar();
	mfxScale = mSPH_DrawFX->GetVariableByName("gScale")->AsScalar();

	mSPH_BuildNormalWMapTech = mSPH_DrawFX->GetTechniqueByName("BuildNormalWMapTech");
	mfxSPH_PosWMap = mSPH_DrawFX->GetVariableByName("gPosWMap")->AsShaderResource();

	mSPH_DrawSPHFluidTech = mSPH_DrawFX->GetTechniqueByName("DrawSPHFluidTech");
	mfxSPH_NormalWMap = mSPH_DrawFX->GetVariableByName("gNormalWMap")->AsShaderResource();

	mSPH_GenerateSurfaceFX = d3dHelper::CreateEffectFromMemory(L"FX/SPHFluidGenerateSurface.cso", md3dDevice);
	mSPH_GenerateSurfaceTech = mSPH_GenerateSurfaceFX->GetTechniqueByName("SPHFluidGenerateSurfaceTech");
	mSPH_ClearSurfaceBufferTech = mSPH_GenerateSurfaceFX->GetTechniqueByName("SPHClearSurfaceBufferTech");
	mfxSPH_FluidSurafceUAV = mSPH_GenerateSurfaceFX->GetVariableByName("gFluidSurfaceRW")->AsUnorderedAccessView();
	mfxSPH_GridIndicesSRV = mSPH_GenerateSurfaceFX->GetVariableByName("GridIndicesRO")->AsShaderResource();
	mfxSPH_ParticlesDensitySRV = mSPH_GenerateSurfaceFX->GetVariableByName("ParticleDensityRO")->AsShaderResource();
	mfxSPH_ParticlesNormalsSRV = mSPH_GenerateSurfaceFX->GetVariableByName("ParticleNormalsRO")->AsShaderResource();
	mfxSPH_ParticlesSRV = mSPH_GenerateSurfaceFX->GetVariableByName("ParticlesRO")->AsShaderResource();
	mfxSPH_GridOrigin = mSPH_GenerateSurfaceFX->GetVariableByName("gGridOrigin")->AsVector();
	mfxSPH_GridCellSize = mSPH_GenerateSurfaceFX->GetVariableByName("gGridCellSize")->AsScalar();
	mfxSPH_DensityIsoValue = mSPH_GenerateSurfaceFX->GetVariableByName("gDensityIsoValue")->AsScalar();
	mfxSPH_SmoothingLength = mSPH_GenerateSurfaceFX->GetVariableByName("gSmoothlen")->AsScalar();
}

void SPH::SPHBuildVertexInputLayouts() 
{
	D3DX11_PASS_DESC passDesc;
	static const D3D11_INPUT_ELEMENT_DESC ParticleDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "VELOCITY", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "ACCELERATION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	mSPH_BuildPosWMapTech->GetPassByIndex(0)->GetDesc(&passDesc);
	HR(md3dDevice->CreateInputLayout(ParticleDesc, 3, passDesc.pIAInputSignature,
		passDesc.IAInputSignatureSize, &(mSPH_ParticlesInputLayout)));
	
	static const D3D11_INPUT_ELEMENT_DESC ParticleDrawDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	mSPH_BuildNormalWMapTech->GetPassByIndex(0)->GetDesc(&passDesc);
	HR(md3dDevice->CreateInputLayout(ParticleDrawDesc, 2, passDesc.pIAInputSignature,
		passDesc.IAInputSignatureSize, &(mSPH_DrawParticlesInputLayout)));
}

void SPH::SPHBuildDrawBuffers()
{
	ParticleDraw v[4];

	v[0].Pos = XMFLOAT3(-1.0f, -1.0f, 0.0f);
	v[1].Pos = XMFLOAT3(-1.0f, +1.0f, 0.0f);
	v[2].Pos = XMFLOAT3(+1.0f, +1.0f, 0.0f);
	v[3].Pos = XMFLOAT3(+1.0f, -1.0f, 0.0f);

	v[0].Tex = XMFLOAT2(0.0f, 1.0f);
	v[1].Tex = XMFLOAT2(0.0f, 0.0f);
	v[2].Tex = XMFLOAT2(1.0f, 0.0f);
	v[3].Tex = XMFLOAT2(1.0f, 1.0f);

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(ParticleDraw) * 4;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = v;

	HR(md3dDevice->CreateBuffer(&vbd, &vinitData, &mScreenQuadVB));

	USHORT indices[6] =
	{
		0, 1, 2,
		0, 2, 3
	};

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(USHORT) * 6;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.StructureByteStride = 0;
	ibd.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = indices;

	HR(md3dDevice->CreateBuffer(&ibd, &iinitData, &mScreenQuadIB));
}

void SPH::OnSize(FLOAT ClientH, FLOAT ClientW)
{
	InitViewport(&mScreenViewport, 0.0f, 0.0f, ClientW, ClientH, 0.0f, 1.0f);
	SPHBuildOffscreenTextureViews(ClientH, ClientW);
	
	if (pBlurFilter)
		pBlurFilter->OnSize(md3dDevice, ClientW, ClientH, DXGI_FORMAT_R32G32B32A32_FLOAT, false, 0);
}

void SPH::SPHBuildOffscreenTextureViews(FLOAT ClientH, FLOAT ClientW)
{
	ReleaseCOM(mSPH_PosWMapRTV);
	ReleaseCOM(mSPH_PosWMapSRV);
	ReleaseCOM(mSPH_PosWMapUAV);

	ReleaseCOM(mSPH_ShadowMapRTV);
	ReleaseCOM(mSPH_ShadowMapSRV);
	ReleaseCOM(mSPH_ShadowMapUAV);

	ReleaseCOM(mSPH_NormalWMapRTV);
	ReleaseCOM(mSPH_NormalWMapSRV);

	ReleaseCOM(mSPH_PlanarRefractionMapRTV);
	ReleaseCOM(mSPH_PlanarRefractionMapSRV);
	ReleaseCOM(mSPH_PlanarRefractionMapDSV);

	D3D11_TEXTURE2D_DESC depthTexDesc;
	ZeroMemory(&depthTexDesc, sizeof(depthTexDesc));
	depthTexDesc.Width = ClientW;
	depthTexDesc.Height = ClientH;
	depthTexDesc.MipLevels = 1;
	depthTexDesc.ArraySize = 1;
	depthTexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	depthTexDesc.SampleDesc.Count = 1;
	depthTexDesc.SampleDesc.Quality = 0;
	depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
	depthTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS;
	depthTexDesc.CPUAccessFlags = 0;
	depthTexDesc.MiscFlags = 0;

	ID3D11Texture2D* DepthTex = 0;
	HR(md3dDevice->CreateTexture2D(&depthTexDesc, 0, &DepthTex));
	HR(md3dDevice->CreateShaderResourceView(DepthTex, 0, &mSPH_PosWMapSRV));
	HR(md3dDevice->CreateRenderTargetView(DepthTex, 0, &mSPH_PosWMapRTV));
	HR(md3dDevice->CreateUnorderedAccessView(DepthTex, 0, &mSPH_PosWMapUAV));

	ReleaseCOM(DepthTex);

	D3D11_TEXTURE2D_DESC normalTexDesc;
	ZeroMemory(&normalTexDesc, sizeof(normalTexDesc));
	normalTexDesc.Width = ClientW;
	normalTexDesc.Height = ClientH;
	normalTexDesc.MipLevels = 1;
	normalTexDesc.ArraySize = 1;
	normalTexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	normalTexDesc.SampleDesc.Count = 1;
	normalTexDesc.SampleDesc.Quality = 0;
	normalTexDesc.Usage = D3D11_USAGE_DEFAULT;
	normalTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	normalTexDesc.CPUAccessFlags = 0;
	normalTexDesc.MiscFlags = 0;

	ID3D11Texture2D* NormalTex = 0;
	HR(md3dDevice->CreateTexture2D(&normalTexDesc, 0, &NormalTex));
	HR(md3dDevice->CreateShaderResourceView(NormalTex, 0, &mSPH_NormalWMapSRV));
	HR(md3dDevice->CreateRenderTargetView(NormalTex, 0, &mSPH_NormalWMapRTV));

	ReleaseCOM(NormalTex);

	D3D11_TEXTURE2D_DESC smTexDesc;
	ZeroMemory(&smTexDesc, sizeof(smTexDesc));
	smTexDesc.Width = mSPH_ShadowMapVP.Width;
	smTexDesc.Height = mSPH_ShadowMapVP.Height;
	smTexDesc.MipLevels = 1;
	smTexDesc.ArraySize = 1;
	smTexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	smTexDesc.SampleDesc.Count = 1;
	smTexDesc.SampleDesc.Quality = 0;
	smTexDesc.Usage = D3D11_USAGE_DEFAULT;
	smTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS;
	smTexDesc.CPUAccessFlags = 0;
	smTexDesc.MiscFlags = 0;

	ID3D11Texture2D* SMTex = 0;
	HR(md3dDevice->CreateTexture2D(&smTexDesc, 0, &SMTex));
	HR(md3dDevice->CreateShaderResourceView(SMTex, 0, &mSPH_ShadowMapSRV));
	HR(md3dDevice->CreateRenderTargetView(SMTex, 0, &mSPH_ShadowMapRTV));
	HR(md3dDevice->CreateUnorderedAccessView(SMTex, 0, &mSPH_ShadowMapUAV));

	ReleaseCOM(SMTex);

	D3D11_TEXTURE2D_DESC prTexDesc;
	prTexDesc.Width = mSPH_PlanarRefractionMapVP.Width;
	prTexDesc.Height = mSPH_PlanarRefractionMapVP.Height;
	prTexDesc.MipLevels = 0;
	prTexDesc.ArraySize = 1;
	prTexDesc.SampleDesc.Count = 1;
	prTexDesc.SampleDesc.Quality = 0;
	prTexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	prTexDesc.Usage = D3D11_USAGE_DEFAULT;
	prTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	prTexDesc.CPUAccessFlags = 0;
	prTexDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	ID3D11Texture2D* PRTex = 0;
	HR(md3dDevice->CreateTexture2D(&prTexDesc, 0, &PRTex));

	D3D11_RENDER_TARGET_VIEW_DESC prRtvDesc;
	prRtvDesc.Format = prTexDesc.Format;
	prRtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	prRtvDesc.Texture2DArray.ArraySize = 1;
	prRtvDesc.Texture2DArray.MipSlice = 0;

	HR(md3dDevice->CreateRenderTargetView(PRTex, &prRtvDesc, &mSPH_PlanarRefractionMapRTV));

	D3D11_SHADER_RESOURCE_VIEW_DESC prSrvDesc;
	prSrvDesc.Format = prTexDesc.Format;
	prSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	prSrvDesc.TextureCube.MostDetailedMip = 0;
	prSrvDesc.TextureCube.MipLevels = -1;

	HR(md3dDevice->CreateShaderResourceView(PRTex, &prSrvDesc, &mSPH_PlanarRefractionMapSRV));

	ReleaseCOM(PRTex);

	D3D11_TEXTURE2D_DESC prDepthTexDesc;
	prDepthTexDesc.Width = mSPH_PlanarRefractionMapVP.Width;
	prDepthTexDesc.Height = mSPH_PlanarRefractionMapVP.Height;
	prDepthTexDesc.MipLevels = 1;
	prDepthTexDesc.ArraySize = 1;
	prDepthTexDesc.SampleDesc.Count = 1;
	prDepthTexDesc.SampleDesc.Quality = 0;
	prDepthTexDesc.Format = DXGI_FORMAT_D32_FLOAT;
	prDepthTexDesc.Usage = D3D11_USAGE_DEFAULT;
	prDepthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	prDepthTexDesc.CPUAccessFlags = 0;
	prDepthTexDesc.MiscFlags = 0;

	ID3D11Texture2D* prDepthTex = 0;
	HR(md3dDevice->CreateTexture2D(&prDepthTexDesc, 0, &prDepthTex));

	D3D11_DEPTH_STENCIL_VIEW_DESC prDsvDesc;
	prDsvDesc.Format = prDepthTexDesc.Format;
	prDsvDesc.Flags = 0;
	prDsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	prDsvDesc.Texture2D.MipSlice = 0;

	HR(md3dDevice->CreateDepthStencilView(prDepthTex, &prDsvDesc, &mSPH_PlanarRefractionMapDSV));

	ReleaseCOM(prDepthTex);
}

void SPH::SPHCreateBuffersAndViews()
{
	//normal map srv, buffers, buffers' uavs

	mSPH_NormalMapSRV = d3dHelper::CreateTexture2DSRVW(md3dDevice, md3dImmediateContext, L"Textures\\water_particle.JPG");

	std::vector<Particle>Particles(NUM_PARTICLES);

	float bathParticlesPart = 1023.0f / 1024.0f;
	float dropPartcielsPart = 1.0f - bathParticlesPart;
	float dropOffset = powf(NUM_PARTICLES, 1.0f / 3.0f) * RADIUS - powf(NUM_PARTICLES * dropPartcielsPart, 1.0f / 3.0f) * RADIUS;

	for (int i = 0; i < NUM_PARTICLES * bathParticlesPart; ++i)
	{
		Particles[i].Acc = XMFLOAT4(0, 0, 0, 1.0f);

		Particles[i].Pos = XMFLOAT4((i % (int)powf(NUM_PARTICLES, 1.0f / 3.0f)) * RADIUS * 2.0f,
			(i / (int)powf(NUM_PARTICLES, 2.0f / 3.0f)) * RADIUS * 2.0f,
			(i % (int)powf(NUM_PARTICLES, 2.0f / 3.0f)) / powf(NUM_PARTICLES, 1.0f / 3.0f) * RADIUS * 2.0f,
			1.0f
		);
		Particles[i].Vel = XMFLOAT4(0, 0, 0, 1.0f);
	}

	for (int i = 0; i < NUM_PARTICLES * dropPartcielsPart; ++i)
	{
		Particles[i + NUM_PARTICLES * bathParticlesPart].Acc = XMFLOAT4(0, 0, 0, 1.0f);

		Particles[i + NUM_PARTICLES * bathParticlesPart].Pos = XMFLOAT4(dropOffset + (i % (int)powf(NUM_PARTICLES * dropPartcielsPart, 1.0f / 3.0f)) * RADIUS * 2.0f,
			0.5f + (i / (int)powf(NUM_PARTICLES * dropPartcielsPart, 2.0f / 3.0f)) * RADIUS * 2.0f,
			dropOffset + (i % (int)powf(NUM_PARTICLES * dropPartcielsPart, 2.0f / 3.0f)) / powf(NUM_PARTICLES * dropPartcielsPart, 1.0f / 3.0f) * RADIUS * 2.0f,
			1.0f
		);
		Particles[i + NUM_PARTICLES * bathParticlesPart].Vel = XMFLOAT4(0, 0, 0, 1.0f);
	}

	D3D11_BUFFER_DESC particledesc;
	ZeroMemory(&particledesc, sizeof(particledesc));
	particledesc.Usage = D3D11_USAGE_DEFAULT;
	particledesc.ByteWidth = sizeof(Particle) * NUM_PARTICLES;
	particledesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	particledesc.CPUAccessFlags = 0;
	particledesc.StructureByteStride = sizeof(Particle);
	particledesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	D3D11_SUBRESOURCE_DATA data;
	ZeroMemory(&data, sizeof(data));
	data.pSysMem = &Particles[0];

	HR(md3dDevice->CreateBuffer(&particledesc, &data, &mSPH_ParticlesBuffer));

	D3D11_UNORDERED_ACCESS_VIEW_DESC particleuavdesc;
	ZeroMemory(&particleuavdesc, sizeof(particleuavdesc));
	particleuavdesc.Format = DXGI_FORMAT_UNKNOWN;
	particleuavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	particleuavdesc.Buffer.FirstElement = 0;
	particleuavdesc.Buffer.Flags = 0;
	particleuavdesc.Buffer.NumElements = NUM_PARTICLES;

	HR(md3dDevice->CreateUnorderedAccessView(mSPH_ParticlesBuffer, &particleuavdesc, &mSPH_ParticlesBufferUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC particlesrvdesc;
	ZeroMemory(&particlesrvdesc, sizeof(particlesrvdesc));
	particlesrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	particlesrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	particlesrvdesc.Buffer.ElementWidth = NUM_PARTICLES;
	HR(md3dDevice->CreateShaderResourceView(mSPH_ParticlesBuffer, &particlesrvdesc, &mSPH_ParticlesBufferSRV));

	//////////////////////////

	D3D11_BUFFER_DESC sortedparticledesc;
	ZeroMemory(&sortedparticledesc, sizeof(sortedparticledesc));
	sortedparticledesc.Usage = D3D11_USAGE_DEFAULT;
	sortedparticledesc.ByteWidth = sizeof(Particle) * NUM_PARTICLES;
	sortedparticledesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	sortedparticledesc.CPUAccessFlags = 0;
	sortedparticledesc.StructureByteStride = sizeof(Particle);
	sortedparticledesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HR(md3dDevice->CreateBuffer(&sortedparticledesc, NULL, &mSPH_SortedParticlesBuffer));

	D3D11_UNORDERED_ACCESS_VIEW_DESC particlesorteduavdesc;
	ZeroMemory(&particlesorteduavdesc, sizeof(particlesorteduavdesc));
	particlesorteduavdesc.Format = DXGI_FORMAT_UNKNOWN;
	particlesorteduavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	particlesorteduavdesc.Buffer.FirstElement = 0;
	particlesorteduavdesc.Buffer.Flags = 0;
	particlesorteduavdesc.Buffer.NumElements = NUM_PARTICLES;

	HR(md3dDevice->CreateUnorderedAccessView(mSPH_SortedParticlesBuffer, &particlesorteduavdesc, &mSPH_SortedParticlesBufferUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC sortedparticlesrvdesc;
	ZeroMemory(&sortedparticlesrvdesc, sizeof(sortedparticlesrvdesc));
	sortedparticlesrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	sortedparticlesrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	sortedparticlesrvdesc.Buffer.ElementWidth = NUM_PARTICLES;
	HR(md3dDevice->CreateShaderResourceView(mSPH_SortedParticlesBuffer, &sortedparticlesrvdesc, &mSPH_SortedParticlesBufferSRV));

	//////////////////////////

	D3D11_BUFFER_DESC griddesc;
	ZeroMemory(&griddesc, sizeof(griddesc));
	griddesc.Usage = D3D11_USAGE_DEFAULT;
	griddesc.ByteWidth = sizeof(XMUINT2) * NUM_CELLS;
	griddesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	griddesc.CPUAccessFlags = 0;
	griddesc.StructureByteStride = sizeof(XMUINT2);
	griddesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HR(md3dDevice->CreateBuffer(&griddesc, NULL, &mSPH_GridBuffer));

	D3D11_UNORDERED_ACCESS_VIEW_DESC griduavdesc;
	ZeroMemory(&griduavdesc, sizeof(griduavdesc));
	griduavdesc.Format = DXGI_FORMAT_UNKNOWN;
	griduavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	griduavdesc.Buffer.FirstElement = 0;
	griduavdesc.Buffer.Flags = 0;
	griduavdesc.Buffer.NumElements = NUM_CELLS;

	HR(md3dDevice->CreateUnorderedAccessView(mSPH_GridBuffer, &griduavdesc, &mSPH_GridBufferUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC gridsrvdesc;
	ZeroMemory(&gridsrvdesc, sizeof(gridsrvdesc));
	gridsrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	gridsrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	gridsrvdesc.Buffer.ElementWidth = NUM_CELLS;
	HR(md3dDevice->CreateShaderResourceView(mSPH_GridBuffer, &gridsrvdesc, &mSPH_GridBufferSRV));

	////////////////////////////////

	D3D11_BUFFER_DESC gridinddesc;
	ZeroMemory(&gridinddesc, sizeof(gridinddesc));
	gridinddesc.Usage = D3D11_USAGE_DEFAULT;
	gridinddesc.ByteWidth = sizeof(XMUINT2) * NUM_CELLS;
	gridinddesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	gridinddesc.CPUAccessFlags = 0;
	gridinddesc.StructureByteStride = sizeof(XMUINT2);
	gridinddesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HR(md3dDevice->CreateBuffer(&gridinddesc, NULL, &mSPH_GridIndicesBuffer));

	D3D11_UNORDERED_ACCESS_VIEW_DESC gridinduavdesc;
	ZeroMemory(&gridinduavdesc, sizeof(gridinduavdesc));
	gridinduavdesc.Format = DXGI_FORMAT_UNKNOWN;
	gridinduavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	gridinduavdesc.Buffer.FirstElement = 0;
	gridinduavdesc.Buffer.Flags = 0;
	gridinduavdesc.Buffer.NumElements = NUM_CELLS;

	HR(md3dDevice->CreateUnorderedAccessView(mSPH_GridIndicesBuffer, &gridinduavdesc, &mSPH_GridIndicesBufferUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC gridindsrvdesc;
	ZeroMemory(&gridindsrvdesc, sizeof(gridindsrvdesc));
	gridindsrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	gridindsrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	gridindsrvdesc.Buffer.ElementWidth = NUM_CELLS;
	HR(md3dDevice->CreateShaderResourceView(mSPH_GridIndicesBuffer, &gridindsrvdesc, &mSPH_GridIndicesBufferSRV));

	////////////////////////////////

	D3D11_BUFFER_DESC tempdesc;
	ZeroMemory(&tempdesc, sizeof(tempdesc));
	tempdesc.Usage = D3D11_USAGE_DEFAULT;
	tempdesc.ByteWidth = sizeof(XMUINT2) * NUM_CELLS;
	tempdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	tempdesc.CPUAccessFlags = 0;
	tempdesc.StructureByteStride = sizeof(XMUINT2);
	tempdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HR(md3dDevice->CreateBuffer(&tempdesc, NULL, &mSPH_TempBuffer));

	D3D11_UNORDERED_ACCESS_VIEW_DESC tempuavdesc;
	ZeroMemory(&tempuavdesc, sizeof(tempuavdesc));
	tempuavdesc.Format = DXGI_FORMAT_UNKNOWN;
	tempuavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	tempuavdesc.Buffer.FirstElement = 0;
	tempuavdesc.Buffer.Flags = 0;
	tempuavdesc.Buffer.NumElements = NUM_CELLS;

	HR(md3dDevice->CreateUnorderedAccessView(mSPH_TempBuffer, &tempuavdesc, &mSPH_TempBufferUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC tempsrvdesc;
	ZeroMemory(&tempsrvdesc, sizeof(tempsrvdesc));
	tempsrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	tempsrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	tempsrvdesc.Buffer.ElementWidth = NUM_CELLS;
	HR(md3dDevice->CreateShaderResourceView(mSPH_TempBuffer, &tempsrvdesc, &mSPH_TempBufferSRV));

	///////////////////////////////////

	D3D11_BUFFER_DESC particledensitydesc;
	ZeroMemory(&particledensitydesc, sizeof(particledensitydesc));
	particledensitydesc.Usage = D3D11_USAGE_DEFAULT;
	particledensitydesc.ByteWidth = sizeof(XMFLOAT4) * NUM_PARTICLES;
	particledensitydesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	particledensitydesc.CPUAccessFlags = 0;
	particledensitydesc.StructureByteStride = sizeof(XMFLOAT4);
	particledensitydesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HR(md3dDevice->CreateBuffer(&particledensitydesc, NULL, &mSPH_ParticleDensityBuffer));

	D3D11_UNORDERED_ACCESS_VIEW_DESC particledensityuavdesc;
	ZeroMemory(&particledensityuavdesc, sizeof(particledensityuavdesc));
	particledensityuavdesc.Format = DXGI_FORMAT_UNKNOWN;
	particledensityuavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	particledensityuavdesc.Buffer.FirstElement = 0;
	particledensityuavdesc.Buffer.Flags = 0;
	particledensityuavdesc.Buffer.NumElements = NUM_PARTICLES;

	HR(md3dDevice->CreateUnorderedAccessView(mSPH_ParticleDensityBuffer, &particledensityuavdesc, &mSPH_ParticleDensityBufferUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC particledensitysrvdesc;
	ZeroMemory(&particledensitysrvdesc, sizeof(particledensitysrvdesc));
	particledensitysrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	particledensitysrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	particledensitysrvdesc.Buffer.ElementWidth = NUM_PARTICLES;
	HR(md3dDevice->CreateShaderResourceView(mSPH_ParticleDensityBuffer, &particledensitysrvdesc, &mSPH_ParticleDensityBufferSRV));

	///////////////////////

	D3D11_BUFFER_DESC particleforcesdesc;
	ZeroMemory(&particleforcesdesc, sizeof(particleforcesdesc));
	particleforcesdesc.Usage = D3D11_USAGE_DEFAULT;
	particleforcesdesc.ByteWidth = sizeof(XMFLOAT4) * NUM_PARTICLES;
	particleforcesdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	particleforcesdesc.CPUAccessFlags = 0;
	particleforcesdesc.StructureByteStride = sizeof(XMFLOAT4);
	particleforcesdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HR(md3dDevice->CreateBuffer(&particleforcesdesc, NULL, &mSPH_ParticleForcesBuffer));

	D3D11_UNORDERED_ACCESS_VIEW_DESC particleforcesuavdesc;
	ZeroMemory(&particleforcesuavdesc, sizeof(particleforcesuavdesc));
	particleforcesuavdesc.Format = DXGI_FORMAT_UNKNOWN;
	particleforcesuavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	particleforcesuavdesc.Buffer.FirstElement = 0;
	particleforcesuavdesc.Buffer.Flags = 0;
	particleforcesuavdesc.Buffer.NumElements = NUM_PARTICLES;

	HR(md3dDevice->CreateUnorderedAccessView(mSPH_ParticleForcesBuffer, &particleforcesuavdesc, &mSPH_ParticleForcesBufferUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC particleforcessrvdesc;
	ZeroMemory(&particleforcessrvdesc, sizeof(particleforcessrvdesc));
	particleforcessrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	particleforcessrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	particleforcessrvdesc.Buffer.ElementWidth = NUM_PARTICLES;
	HR(md3dDevice->CreateShaderResourceView(mSPH_ParticleForcesBuffer, &particleforcessrvdesc, &mSPH_ParticleForcesBufferSRV));

	//////////////////////
	D3D11_BUFFER_DESC particlenormalsdesc;
	ZeroMemory(&particlenormalsdesc, sizeof(particlenormalsdesc));
	particlenormalsdesc.Usage = D3D11_USAGE_DEFAULT;
	particlenormalsdesc.ByteWidth = sizeof(XMFLOAT4) * NUM_PARTICLES;
	particlenormalsdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	particlenormalsdesc.CPUAccessFlags = 0;
	particlenormalsdesc.StructureByteStride = sizeof(XMFLOAT4);
	particlenormalsdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HR(md3dDevice->CreateBuffer(&particlenormalsdesc, NULL, &mSPH_ParticleNormalsBuffer));

	D3D11_UNORDERED_ACCESS_VIEW_DESC particlenormalsuavdesc;
	ZeroMemory(&particlenormalsuavdesc, sizeof(particlenormalsuavdesc));
	particlenormalsuavdesc.Format = DXGI_FORMAT_UNKNOWN;
	particlenormalsuavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	particlenormalsuavdesc.Buffer.FirstElement = 0;
	particlenormalsuavdesc.Buffer.Flags = 0;
	particlenormalsuavdesc.Buffer.NumElements = NUM_PARTICLES;

	HR(md3dDevice->CreateUnorderedAccessView(mSPH_ParticleNormalsBuffer, &particlenormalsuavdesc, &mSPH_ParticleNormalsBufferUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC particlenormalssrvdesc;
	ZeroMemory(&particlenormalssrvdesc, sizeof(particlenormalssrvdesc));
	particlenormalssrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	particlenormalssrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	particlenormalssrvdesc.Buffer.ElementWidth = NUM_PARTICLES;
	HR(md3dDevice->CreateShaderResourceView(mSPH_ParticleNormalsBuffer, &particlenormalssrvdesc, &mSPH_ParticleNormalsBufferSRV));

	//////////////////////
	D3D11_BUFFER_DESC fluiddesc;
	ZeroMemory(&fluiddesc, sizeof(fluiddesc));
	fluiddesc.Usage = D3D11_USAGE_DEFAULT;
	fluiddesc.ByteWidth = sizeof(FluidTriangle) * MC_TRIS_BUF_SZ;
	fluiddesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	fluiddesc.CPUAccessFlags = 0;
	fluiddesc.StructureByteStride = sizeof(FluidTriangle);
	fluiddesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HR(md3dDevice->CreateBuffer(&fluiddesc, 0, &mFluidSurface));

	D3D11_UNORDERED_ACCESS_VIEW_DESC fluiduavdesc;
	ZeroMemory(&fluiduavdesc, sizeof(fluiduavdesc));
	fluiduavdesc.Format = DXGI_FORMAT_UNKNOWN;
	fluiduavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	fluiduavdesc.Buffer.FirstElement = 0;
	fluiduavdesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
	fluiduavdesc.Buffer.NumElements = MC_TRIS_BUF_SZ;

	HR(md3dDevice->CreateUnorderedAccessView(mFluidSurface, &fluiduavdesc, &mFluidSurfaceUAV));

	//////////////////////
	D3D11_BUFFER_DESC nodespoolcopydesc;
	nodespoolcopydesc.Usage = D3D11_USAGE_STAGING;
	nodespoolcopydesc.ByteWidth = sizeof(FluidTriangle) * MC_TRIS_BUF_SZ;
	nodespoolcopydesc.BindFlags = 0;
	nodespoolcopydesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	nodespoolcopydesc.StructureByteStride = sizeof(FluidTriangle);
	nodespoolcopydesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	HR(md3dDevice->CreateBuffer(&nodespoolcopydesc, NULL, &mFluidSurfaceCopy));
	
	D3D11_BUFFER_DESC nodespoolviewdesc;
	nodespoolviewdesc.Usage = D3D11_USAGE_DYNAMIC;
	nodespoolviewdesc.ByteWidth = sizeof(FluidVertex) * MC_VERTS_BUF_SZ;
	nodespoolviewdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	nodespoolviewdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	nodespoolviewdesc.StructureByteStride = sizeof(FluidVertex);
	nodespoolviewdesc.MiscFlags = 0;
	HR(md3dDevice->CreateBuffer(&nodespoolviewdesc, NULL, &mFluidSurfaceDrawable));
}