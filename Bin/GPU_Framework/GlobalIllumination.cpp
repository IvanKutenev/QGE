#include "GlobalIllumination.h"

GlobalIllumination::GlobalIllumination(ID3D11Device* device, ID3D11DeviceContext* dc, ID3DX11Effect* gengridfx, Camera& camera) : mGridBuffer(0), mGridBufferSRV(0), mGridBufferUAV(0), m64x64RT(0),
	mClearGridFX(0), mClearGridTech(0), mGenerateGridFX(0), mfxClearGridBufferUAV(0), mSHBuffer(0), mSHBufferSRV(0), mSHBufferUAV(0), mPingPongSHBuffer(0), mPingPongSHBufferSRV(0), mPingPongSHBufferUAV(0),
	mfxFinalSHBufferSRV(0), mfxInputSHBufferSRV(0), mfxOutputSHBufferUAV(0), mPropogationsCount(GI_VPL_PROPOGATIONS_COUNT), mAttFactor(GI_ATTENUATION_FACTOR)
{
	md3dDevice =device;
	mDC = dc;
	mGenerateGridFX = gengridfx;
	mCamera = camera;

	BuildViews();
	BuildFX();
	BuildCamera();
}

void GlobalIllumination::Setup()
{
	ClearGrid();
	mDC->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	mDC->CSSetShaderResources(0, 1, pNullSRV);
	mDC->CSSetShader(0, 0, 0);
	mfxGenerateGridBufferUAV->SetUnorderedAccessView(mGridBufferUAV);
	mfxGridCenterPosW->SetRawValue(reinterpret_cast<const float*>(&mGridCenterPosW), 0, sizeof(mGridCenterPosW));
	mfxGridViewProjMatrices->SetRawValue(mGridViewProjMatrices, 0, sizeof(mGridViewProjMatrices));
	mfxInvertedGridCellSizes->SetRawValue(reinterpret_cast<const float*>(&mInvertedGridCellSizes), 0, sizeof(mInvertedGridCellSizes));
	mfxMeshCullingFactor->SetFloat(GI_MESH_CULLING_FACTOR);
}

void GlobalIllumination::SetFinalSHBuffer()
{
	mfxFinalSHBufferSRV->SetResource(mSHBufferSRV);
	mfxGridBufferROSRV->SetResource(mGridBufferSRV);
	mfxGlobalIllumAttFactor->SetFloat(mAttFactor);
};

void GlobalIllumination::Update(Camera& camera)
{
	mCamera = camera;
	BuildCamera();
	BuildViewport();
}

void GlobalIllumination::Compute(ShadowMapping* pSM,
								 UINT dirLightsNum, DirectionalLight* dirLight,
								 UINT spotLightsNum, SpotLight* spotLight,
								 UINT pointLightsNum, PointLight* pointLight)
{
	CreateVPLs(pSM, dirLightsNum, dirLight, spotLightsNum, spotLight, pointLightsNum, pointLight);
	PropagateVPLs();
	mDC->CSSetShader(0, 0, 0);
}

void GlobalIllumination::ClearGrid()
{
	mfxClearGridBufferUAV->SetUnorderedAccessView(mGridBufferUAV);

	for(int i = 0; i < GI_CASCADES_COUNT; ++i)
	{
		mfxClearOnCascadeIndex->SetInt(i);

		D3DX11_TECHNIQUE_DESC techDesc;
		mClearGridTech->GetDesc(&techDesc);

		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			mClearGridTech->GetPassByIndex(p)->Apply(0, mDC);

			mDC->Dispatch(GI_CS_THREAD_GROUPS_NUMBER, GI_CS_THREAD_GROUPS_NUMBER, GI_CS_THREAD_GROUPS_NUMBER);
		}
	}
	mDC->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	mDC->CSSetShaderResources(0, 1, pNullSRV);
	mDC->CSSetShader(0, 0, 0);
}

void GlobalIllumination::CreateVPLs(ShadowMapping* pSM,
									UINT dirLightsNum, DirectionalLight* dirLight,
									UINT spotLightsNum, SpotLight* spotLight,
									UINT pointLightsNum, PointLight* pointLight)
{
	mfxView->SetMatrix(reinterpret_cast<float*>(&mCamera.View()));
	mfxGridCellSizes->SetRawValue(reinterpret_cast<float*>(&mGridCellSizes), 0, sizeof(mGridCellSizes));
	mfxGridCenterPos->SetRawValue(reinterpret_cast<float*>(&mGridCenterPosW), 0, sizeof(mGridCenterPosW));
	mfxEyePosW->SetFloatVector(reinterpret_cast<float*>(&(mCamera.GetPosition())));
	mfxDirLightsNum->SetInt(static_cast<int>(dirLightsNum));
	mfxDirLight->SetRawValue(dirLight, 0, sizeof(DirectionalLight) * 16);
	mfxSpotLightsNum->SetInt(static_cast<int>(spotLightsNum));
	mfxSpotLight->SetRawValue(spotLight, 0, sizeof(SpotLight) * 16);
	mfxPointLightsNum->SetInt(static_cast<int>(pointLightsNum));
	mfxPointLight->SetRawValue(pointLight, 0, sizeof(PointLight) * 16);
	mfxDirLightShadowMapArray->SetResource(pSM->GetDirLightDistanceMapSRV());
	mfxDirLightShadowTransformArray->SetRawValue(&pSM->mDirLightShadowTransform, 0, sizeof(pSM->mDirLightShadowTransform));
	mfxDirLightSplitDistances->SetRawValue(&pSM->mDirLightSplitDistances, 0, sizeof(pSM->mDirLightSplitDistances));
	mfxSpotLightShadowMapArray->SetResource(pSM->GetSpotLightDistanceMapSRV());
	mfxSpotLightShadowTransformArray->SetRawValue(&pSM->mSpotLightShadowTransform, 0, sizeof(pSM->mSpotLightShadowTransform));
	mfxPointLightCubicShadowMapArray->SetResource(pSM->GetPointLightDistanceMapSRV());

	mfxCreateVPLSHBufferUAV->SetUnorderedAccessView(mSHBufferUAV);
	mfxCreateVPLGridBufferSRV->SetResource(mGridBufferSRV);

	for(int i = 0; i < GI_CASCADES_COUNT; ++i)
	{
		mfxCreateVPLOnCascadeIndex->SetInt(i);
		D3DX11_TECHNIQUE_DESC techDesc;
		mCreateVPLTech->GetDesc(&techDesc);

		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			mCreateVPLTech->GetPassByIndex(p)->Apply(0, mDC);

			mDC->Dispatch(GI_CS_THREAD_GROUPS_NUMBER, GI_CS_THREAD_GROUPS_NUMBER, GI_CS_THREAD_GROUPS_NUMBER);
		}
	}
	mDC->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	mDC->CSSetShaderResources(0, 1, pNullSRV);
}

void GlobalIllumination::PropagateVPLs()
{
	//no occlusion pass
	mfxPropagateVPLGridBufferSRV->SetResource(mGridBufferSRV);
	mfxInputSHBufferSRV->SetResource(mSHBufferSRV);
	mfxOutputSHBufferUAV->SetUnorderedAccessView(mPingPongSHBufferUAV);

	for(int i = 0; i < GI_CASCADES_COUNT; ++i)
	{
		mfxPropagateOnCascadeIndex->SetInt(i);
		D3DX11_TECHNIQUE_DESC techDesc;
		mNoOcclusionPropagateVPLTech->GetDesc(&techDesc);

		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			mNoOcclusionPropagateVPLTech->GetPassByIndex(p)->Apply(0, mDC);

			mDC->Dispatch(GI_CS_THREAD_GROUPS_NUMBER, GI_CS_THREAD_GROUPS_NUMBER, GI_CS_THREAD_GROUPS_NUMBER);
		}
	}
	mDC->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	mDC->CSSetShaderResources(0, 1, pNullSRV);

	//passes with occlusion

	ID3D11ShaderResourceView* inputSRVs[2] = { mPingPongSHBufferSRV, mSHBufferSRV};
	ID3D11UnorderedAccessView* outputUAVs[2] = { mSHBufferUAV, mPingPongSHBufferUAV};

	int inputIdx = 0, outputIdx = 0;
	for(int i = 0; i < mPropogationsCount; ++i)
	{
		mfxPropagateVPLGridBufferSRV->SetResource(mGridBufferSRV);
		mfxInputSHBufferSRV->SetResource(inputSRVs[inputIdx]);
		mfxOutputSHBufferUAV->SetUnorderedAccessView(outputUAVs[outputIdx]);
		
		for(int i = 0; i < GI_CASCADES_COUNT; ++i)
		{
			mfxPropagateOnCascadeIndex->SetInt(i);
			D3DX11_TECHNIQUE_DESC techDesc;
			mUseOcclusionPropagateVPLTech->GetDesc(&techDesc);

			for (UINT p = 0; p < techDesc.Passes; ++p)
			{
				mUseOcclusionPropagateVPLTech->GetPassByIndex(p)->Apply(0, mDC);

				mDC->Dispatch(GI_CS_THREAD_GROUPS_NUMBER, GI_CS_THREAD_GROUPS_NUMBER, GI_CS_THREAD_GROUPS_NUMBER);
			}
		}
		mDC->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
		mDC->CSSetShaderResources(0, 1, pNullSRV);

		inputIdx = (inputIdx + 1) % 2;
		outputIdx = (outputIdx + 1) % 2;
	}
}

void GlobalIllumination::BuildViews()
{
	ReleaseCOM(m64x64RT);
	ReleaseCOM(mGridBuffer);
	ReleaseCOM(mGridBufferSRV);
	ReleaseCOM(mGridBufferUAV);
	ReleaseCOM(mPingPongSHBuffer);
	ReleaseCOM(mSHBuffer);
	ReleaseCOM(mPingPongSHBufferSRV);
	ReleaseCOM(mSHBufferSRV);
	ReleaseCOM(mPingPongSHBufferUAV);
	ReleaseCOM(mSHBufferUAV);

	D3D11_TEXTURE2D_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(texDesc));
	texDesc.Width = 64;
	texDesc.Height = 64;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	ID3D11Texture2D* Tex = 0;
	HR(md3dDevice->CreateTexture2D(&texDesc, 0, &Tex));
	HR(md3dDevice->CreateRenderTargetView(Tex, 0, &m64x64RT));

	ReleaseCOM(Tex);

	D3D11_BUFFER_DESC gridbufdesc;
	ZeroMemory(&gridbufdesc, sizeof(gridbufdesc));
	gridbufdesc.Usage = D3D11_USAGE_DEFAULT;
	gridbufdesc.ByteWidth = sizeof(Voxel) * GI_VOXELS_COUNT * GI_CASCADES_COUNT;
	gridbufdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	gridbufdesc.CPUAccessFlags = 0;
	gridbufdesc.StructureByteStride = sizeof(Voxel);
	gridbufdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HR(md3dDevice->CreateBuffer(&gridbufdesc, NULL, &mGridBuffer));

	D3D11_UNORDERED_ACCESS_VIEW_DESC gridbufferuavdesc;
	ZeroMemory(&gridbufferuavdesc, sizeof(gridbufferuavdesc));
	gridbufferuavdesc.Format = DXGI_FORMAT_UNKNOWN;
	gridbufferuavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	gridbufferuavdesc.Buffer.FirstElement = 0;
	gridbufferuavdesc.Buffer.Flags = 0;
	gridbufferuavdesc.Buffer.NumElements = GI_VOXELS_COUNT * GI_CASCADES_COUNT;

	HR(md3dDevice->CreateUnorderedAccessView(mGridBuffer, &gridbufferuavdesc, &mGridBufferUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC gridbuffersrvdesc;
	ZeroMemory(&gridbuffersrvdesc, sizeof(gridbuffersrvdesc));
	gridbuffersrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	gridbuffersrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	gridbuffersrvdesc.Buffer.ElementWidth = GI_VOXELS_COUNT * GI_CASCADES_COUNT;
	HR(md3dDevice->CreateShaderResourceView(mGridBuffer, &gridbuffersrvdesc, &mGridBufferSRV));

	D3D11_BUFFER_DESC vplbufdesc;
	ZeroMemory(&vplbufdesc, sizeof(vplbufdesc));
	vplbufdesc.Usage = D3D11_USAGE_DEFAULT;
	vplbufdesc.ByteWidth = sizeof(VPL) * GI_VOXELS_COUNT * GI_CASCADES_COUNT;
	vplbufdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	vplbufdesc.CPUAccessFlags = 0;
	vplbufdesc.StructureByteStride = sizeof(VPL);
	vplbufdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HR(md3dDevice->CreateBuffer(&vplbufdesc, NULL, &mSHBuffer));
	HR(md3dDevice->CreateBuffer(&vplbufdesc, NULL, &mPingPongSHBuffer));

	D3D11_UNORDERED_ACCESS_VIEW_DESC vplbufferuavdesc;
	ZeroMemory(&vplbufferuavdesc, sizeof(vplbufferuavdesc));
	vplbufferuavdesc.Format = DXGI_FORMAT_UNKNOWN;
	vplbufferuavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	vplbufferuavdesc.Buffer.FirstElement = 0;
	vplbufferuavdesc.Buffer.Flags = 0;
	vplbufferuavdesc.Buffer.NumElements = GI_VOXELS_COUNT * GI_CASCADES_COUNT;

	HR(md3dDevice->CreateUnorderedAccessView(mSHBuffer, &vplbufferuavdesc, &mSHBufferUAV));
	HR(md3dDevice->CreateUnorderedAccessView(mPingPongSHBuffer, &vplbufferuavdesc, &mPingPongSHBufferUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC vplbuffersrvdesc;
	ZeroMemory(&vplbuffersrvdesc, sizeof(vplbuffersrvdesc));
	vplbuffersrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	vplbuffersrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	vplbuffersrvdesc.Buffer.ElementWidth = GI_VOXELS_COUNT * GI_CASCADES_COUNT;
	HR(md3dDevice->CreateShaderResourceView(mSHBuffer, &vplbuffersrvdesc, &mSHBufferSRV));
	HR(md3dDevice->CreateShaderResourceView(mPingPongSHBuffer, &vplbuffersrvdesc, &mPingPongSHBufferSRV));
}

void GlobalIllumination::BuildFX()
{
	mClearGridFX = d3dHelper::CreateEffectFromMemory(L"FX/GIClearGrid.cso", md3dDevice);
	mClearGridTech = mClearGridFX->GetTechniqueByName("ClearGridTech");
	mfxClearGridBufferUAV = mClearGridFX->GetVariableByName("gGridBuffer")->AsUnorderedAccessView();
	mfxClearOnCascadeIndex = mClearGridFX->GetVariableByName("gProcessingCascadeIndex")->AsScalar();

	mfxGenerateGridBufferUAV = mGenerateGridFX->GetVariableByName("gGridBuffer")->AsUnorderedAccessView();
	mfxGridCenterPosW = mGenerateGridFX->GetVariableByName("gGridCenterPosW");
	mfxGridViewProjMatrices = mGenerateGridFX->GetVariableByName("gGridViewProj");
	mfxInvertedGridCellSizes = mGenerateGridFX->GetVariableByName("gInvertedGridCellSizes");
	mfxFinalSHBufferSRV = mGenerateGridFX->GetVariableByName("gSHBuffer")->AsShaderResource();
	mfxGridBufferROSRV = mGenerateGridFX->GetVariableByName("gGridBufferRO")->AsShaderResource();
	mfxGlobalIllumAttFactor = mGenerateGridFX->GetVariableByName("gGlobalIllumAttFactor")->AsScalar();
	mfxMeshCullingFactor = mGenerateGridFX->GetVariableByName("gMeshCullingFactor")->AsScalar();

	mCreateVPLFX = d3dHelper::CreateEffectFromMemory(L"FX/GICreateVPL.cso", md3dDevice);
	mCreateVPLTech = mCreateVPLFX->GetTechniqueByName("CreateVPLTech");
	mfxView = mCreateVPLFX->GetVariableByName("gView")->AsMatrix();
	mfxGridCellSizes = mCreateVPLFX->GetVariableByName("gGridCellSizes");
	mfxGridCenterPos = mCreateVPLFX->GetVariableByName("gGridCenterPosW");
	mfxCreateVPLSHBufferUAV = mCreateVPLFX->GetVariableByName("gSHBuffer")->AsUnorderedAccessView();
	mfxCreateVPLGridBufferSRV = mCreateVPLFX->GetVariableByName("gGridBuffer")->AsShaderResource();
	mfxEyePosW = mCreateVPLFX->GetVariableByName("gEyePosW")->AsVector();
	mfxDirLight = mCreateVPLFX->GetVariableByName("gDirLight");
	mfxSpotLight = mCreateVPLFX->GetVariableByName("gSpotLight");
	mfxPointLight = mCreateVPLFX->GetVariableByName("gPointLight");
	mfxDirLightsNum = mCreateVPLFX->GetVariableByName("gDirLightsNum")->AsScalar();
	mfxSpotLightsNum = mCreateVPLFX->GetVariableByName("gSpotLightsNum")->AsScalar();
	mfxPointLightsNum = mCreateVPLFX->GetVariableByName("gPointLightsNum")->AsScalar();
	mfxDirLightShadowTransformArray = mCreateVPLFX->GetVariableByName("gDirLightShadowTransformArray");
	mfxDirLightSplitDistances = mCreateVPLFX->GetVariableByName("gSplitDistances");
	mfxDirLightShadowMapArray = mCreateVPLFX->GetVariableByName("gDirLightShadowMapArray")->AsShaderResource();
	mfxSpotLightShadowTransformArray = mCreateVPLFX->GetVariableByName("gSpotLightShadowTransformArray");
	mfxSpotLightShadowMapArray = mCreateVPLFX->GetVariableByName("gSpotLightShadowMapArray")->AsShaderResource();
	mfxPointLightCubicShadowMapArray = mCreateVPLFX->GetVariableByName("gPointLightCubicShadowMapArray")->AsShaderResource();
	mfxCreateVPLOnCascadeIndex = mCreateVPLFX->GetVariableByName("gProcessingCascadeIndex")->AsScalar();

	mPropagateVPLFX = d3dHelper::CreateEffectFromMemory(L"FX/GIPropagateVPL.cso", md3dDevice);
	mNoOcclusionPropagateVPLTech = mPropagateVPLFX->GetTechniqueByName("NoOcclusionPropagateVPLTech");
	mUseOcclusionPropagateVPLTech = mPropagateVPLFX->GetTechniqueByName("UseOcclusionPropagateVPLTech");
	mfxPropagateVPLGridBufferSRV = mPropagateVPLFX->GetVariableByName("gGridBuffer")->AsShaderResource();
	mfxInputSHBufferSRV = mPropagateVPLFX->GetVariableByName("gInputSHBuffer")->AsShaderResource();
	mfxOutputSHBufferUAV = mPropagateVPLFX->GetVariableByName("gOutputSHBuffer")->AsUnorderedAccessView();
	mfxPropagateOnCascadeIndex = mPropagateVPLFX->GetVariableByName("gProcessingCascadeIndex")->AsScalar();
}

void GlobalIllumination::CalculateCellSizes()
{
	for (int i = 1; i <= GI_CASCADES_COUNT; ++i)
	{
		float f = (float)i / (float)GI_CASCADES_COUNT;
		float l = mCamera.GetNearZ() * pow(mCamera.GetFarZ() / mCamera.GetNearZ(), f);
		float u = mCamera.GetNearZ() + (mCamera.GetFarZ() - mCamera.GetNearZ()) * f;
		FLOAT mCellSize = (l * SplitLamda + u * (1.0f - SplitLamda)) * 2.0f / GI_VOXEL_GRID_RESOLUTION;
		mGridCellSizes[i - 1] = XMFLOAT4(mCellSize, mCellSize, mCellSize, mCellSize);
	}
}

void GlobalIllumination::BuildCamera()
{
	CalculateCellSizes();

	for(int i = 0; i < GI_CASCADES_COUNT; ++i)
	{
		FLOAT mCellSize = mGridCellSizes[i].x;
		mInvertedGridCellSizes[i] = XMFLOAT4(1.0f / mCellSize, 1.0f / mCellSize, 1.0f / mCellSize, 1.0f / mCellSize);

		//back-to-front
		XMVECTOR eyePosBF = mCamera.GetPositionXM() + XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f) * (GI_VOXEL_GRID_RESOLUTION / 2 * mCellSize + mCamera.GetNearZ());
		//right-to-left
		XMVECTOR eyePosRL = mCamera.GetPositionXM() + XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f) * (GI_VOXEL_GRID_RESOLUTION / 2 * mCellSize + mCamera.GetNearZ());
		//top-to-bottom
		XMVECTOR eyePosTB = mCamera.GetPositionXM() + XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f) * (GI_VOXEL_GRID_RESOLUTION / 2 * mCellSize + mCamera.GetNearZ());
		//front-to-back
		XMVECTOR eyePosFB = mCamera.GetPositionXM() + XMVectorSet(0.0f, 0.0f, -1.0f, 1.0f) * (GI_VOXEL_GRID_RESOLUTION / 2 * mCellSize + mCamera.GetNearZ());
		//left-to-right
		XMVECTOR eyePosLR = mCamera.GetPositionXM() + XMVectorSet(-1.0f, 0.0f, 0.0f, 1.0f) * (GI_VOXEL_GRID_RESOLUTION / 2 * mCellSize + mCamera.GetNearZ());
		//bottom-to-top
		XMVECTOR eyePosBT = mCamera.GetPositionXM() + XMVectorSet(0.0f, -1.0f, 0.0f, 1.0f) * (GI_VOXEL_GRID_RESOLUTION / 2 * mCellSize + mCamera.GetNearZ());

		XMMATRIX ViewBF = XMMatrixLookAtLH(eyePosBF, mCamera.GetPositionXM(), XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
		XMMATRIX ViewRL = XMMatrixLookAtLH(eyePosRL, mCamera.GetPositionXM(), XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
		XMMATRIX ViewTB = XMMatrixLookAtLH(eyePosTB, mCamera.GetPositionXM(), XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f));
		XMMATRIX ViewFB = XMMatrixLookAtLH(eyePosFB, mCamera.GetPositionXM(), XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
		XMMATRIX ViewLR = XMMatrixLookAtLH(eyePosLR, mCamera.GetPositionXM(), XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
		XMMATRIX ViewBT = XMMatrixLookAtLH(eyePosBT, mCamera.GetPositionXM(), XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f));
	
		XMMATRIX P = XMMatrixOrthographicLH(GI_VOXEL_GRID_RESOLUTION * mCellSize, GI_VOXEL_GRID_RESOLUTION * mCellSize, mCamera.GetNearZ(), mCamera.GetNearZ() + mCellSize * GI_VOXEL_GRID_RESOLUTION);

		mGridViewProjMatrices[i * 6] = ViewBF * P;
		mGridViewProjMatrices[1 + i * 6] = ViewRL * P;
		mGridViewProjMatrices[2 + i * 6] = ViewTB * P;
		mGridViewProjMatrices[3 + i * 6] = ViewFB * P;
		mGridViewProjMatrices[4 + i * 6] = ViewLR * P;
		mGridViewProjMatrices[5 + i * 6] = ViewBT * P;

		XMStoreFloat4(&mGridCenterPosW[i], mCamera.GetPositionXM());
	}
}

void GlobalIllumination::BuildViewport()
{
	InitViewport(&m64x64RTViewport, 0.0f, 0.0f, 64, 64, 0.0f, 1.0f);
}