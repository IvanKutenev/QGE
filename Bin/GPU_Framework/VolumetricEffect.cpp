#include "VolumetricEffect.h"

VolumetricEffect::VolumetricEffect(ID3D11Device* device, ID3D11DeviceContext* dc,
	FLOAT ClientH, FLOAT ClientW,
	const XMFLOAT4* GridCenterPosW,
	const XMFLOAT4* InvertedGridCellSizes,
	const XMFLOAT4* GridCellSizes,
	Camera cam) : mOutputFogColorSRV(NULL), mOutputFogColorUAV(NULL), mLightingBuffer(NULL), mLightingBufferSRV(NULL), mLightingBufferUAV(NULL), pBlurFilter(NULL)
{
	md3dDevice = device;
	mDC = dc;

	for (int i = 0; i < GI_CASCADES_COUNT; ++i)
	{
		mInvertedGridCellSizes[i] = InvertedGridCellSizes[i];
		mGridCellSizes[i] = GridCellSizes[i];
	}

	pBlurFilter = new BlurFilter(md3dDevice, ClientW / 2, ClientH / 2, DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT, false, 0);

	OnSize(ClientH, ClientW);
	Update(cam, GridCenterPosW, 0);
	BuildBuffers();
	BuildDensityTexturesViews();
	BuildFX();
	BuildInputLayout();
	BuildOffscreenQuad();
}

void VolumetricEffect::BuildFX(void)
{
	mVolumetricFX = d3dHelper::CreateEffectFromMemory(L"FX/VolumetricEffects.cso", md3dDevice);
	
	mComputeLightingTech = mVolumetricFX->GetTechniqueByName("ComputeLightingTech");
	mAccumulateScatteringTech = mVolumetricFX->GetTechniqueByName("AccumulateScatteringTech");
	mDrawFogTech = mVolumetricFX->GetTechniqueByName("DrawFogTech");

	mfxPosWMapSRV = mVolumetricFX->GetVariableByName("gPosWMap")->AsShaderResource();
	mfxOutputFogColorUAV = mVolumetricFX->GetVariableByName("gOutputFogColorRW")->AsUnorderedAccessView();
	mfxOutputFogColorSRV = mVolumetricFX->GetVariableByName("gOutputFogColorRO")->AsShaderResource();
	mfxLightingBufferSRV = mVolumetricFX->GetVariableByName("gLightingBufferRO")->AsShaderResource();
	mfxLightingBufferUAV = mVolumetricFX->GetVariableByName("gLightingBufferRW")->AsUnorderedAccessView();

	mfxWorleyNoiseMapSRV = mVolumetricFX->GetVariableByName("gWorleyNoiseMap")->AsShaderResource();
	mfxPerlinNoiseMapSRV = mVolumetricFX->GetVariableByName("gPerlinNoiseMap")->AsShaderResource();
	mfxDistributionDensityMapSRV = mVolumetricFX->GetVariableByName("gDistributionDensityMap")->AsShaderResource();

	mfxGridCenterPosW = mVolumetricFX->GetVariableByName("gGridCenterPosW");
	mfxGridCellSizes = mVolumetricFX->GetVariableByName("gGridCellSizes");
	mfxInvertedGridCellSizes = mVolumetricFX->GetVariableByName("gInvertedGridCellSizes");
	mfxProcessingCascadeIndex = mVolumetricFX->GetVariableByName("gProcessingCascadeIndex")->AsScalar();
	mfxView = mVolumetricFX->GetVariableByName("gView")->AsMatrix();
	mfxInvProj = mVolumetricFX->GetVariableByName("gInvProj")->AsMatrix();
	mfxInvView = mVolumetricFX->GetVariableByName("gInvView")->AsMatrix();
	mfxEyePosW = mVolumetricFX->GetVariableByName("gEyePosW")->AsVector();
	mfxDirLight = mVolumetricFX->GetVariableByName("gDirLight");
	mfxSpotLight = mVolumetricFX->GetVariableByName("gSpotLight");
	mfxPointLight = mVolumetricFX->GetVariableByName("gPointLight");
	mfxDirLightsNum = mVolumetricFX->GetVariableByName("gDirLightsNum")->AsScalar();
	mfxSpotLightsNum = mVolumetricFX->GetVariableByName("gSpotLightsNum")->AsScalar();
	mfxPointLightsNum = mVolumetricFX->GetVariableByName("gPointLightsNum")->AsScalar();
	mfxDirLightShadowTransformArray = mVolumetricFX->GetVariableByName("gDirLightShadowTransformArray");
	mfxDirLightSplitDistances = mVolumetricFX->GetVariableByName("gSplitDistances");
	mfxDirLightShadowMapArray = mVolumetricFX->GetVariableByName("gDirLightShadowMapArray")->AsShaderResource();
	mfxSpotLightShadowTransformArray = mVolumetricFX->GetVariableByName("gSpotLightShadowTransformArray");
	mfxSpotLightShadowMapArray = mVolumetricFX->GetVariableByName("gSpotLightShadowMapArray")->AsShaderResource();
	mfxPointLightCubicShadowMapArray = mVolumetricFX->GetVariableByName("gPointLightCubicShadowMapArray")->AsShaderResource();
	mfxClientH = mVolumetricFX->GetVariableByName("gClientH")->AsScalar();
	mfxClientW = mVolumetricFX->GetVariableByName("gClientW")->AsScalar();
	mfxTime = mVolumetricFX->GetVariableByName("gTime")->AsScalar();
}

void VolumetricEffect::BuildViews(void)
{
	ReleaseCOM(mOutputFogColorSRV);
	ReleaseCOM(mOutputFogColorUAV);
	
	D3D11_TEXTURE2D_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(texDesc));
	texDesc.Width = mClientW / 2;
	texDesc.Height = mClientH / 2;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	ID3D11Texture2D* Tex = 0;
	HR(md3dDevice->CreateTexture2D(&texDesc, 0, &Tex));
	HR(md3dDevice->CreateShaderResourceView(Tex, 0, &mOutputFogColorSRV));
	HR(md3dDevice->CreateUnorderedAccessView(Tex, 0, &mOutputFogColorUAV));

	ReleaseCOM(Tex);
}

void VolumetricEffect::BuildDensityTexturesViews(void)
{
	mDistributionDensityMapSRV = d3dHelper::CreateTexture2DSRVW(md3dDevice, mDC, L"Textures\\clouds_distribution_density_map_00.dds");
	mWorleyNoiseMapSRV = d3dHelper::CreateTexture2DSRVW(md3dDevice, mDC, L"Textures\\clouds_worley_noise.dds");
	mPerlinNoiseMapSRV = d3dHelper::CreateTexture2DSRVW(md3dDevice, mDC, L"Textures\\clouds_perlin_noise.dds");
}

void VolumetricEffect::BuildBuffers(void)
{
	D3D11_BUFFER_DESC bufdesc;
	ZeroMemory(&bufdesc, sizeof(bufdesc));
	bufdesc.Usage = D3D11_USAGE_DEFAULT;
	bufdesc.ByteWidth = sizeof(LightingData) * GI_VOXELS_COUNT * GI_CASCADES_COUNT;
	bufdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	bufdesc.CPUAccessFlags = 0;
	bufdesc.StructureByteStride = sizeof(LightingData);
	bufdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HR(md3dDevice->CreateBuffer(&bufdesc, NULL, &mLightingBuffer));

	D3D11_UNORDERED_ACCESS_VIEW_DESC bufferuavdesc;
	ZeroMemory(&bufferuavdesc, sizeof(bufferuavdesc));
	bufferuavdesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferuavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	bufferuavdesc.Buffer.FirstElement = 0;
	bufferuavdesc.Buffer.Flags = 0;
	bufferuavdesc.Buffer.NumElements = GI_VOXELS_COUNT * GI_CASCADES_COUNT;

	HR(md3dDevice->CreateUnorderedAccessView(mLightingBuffer, &bufferuavdesc, &mLightingBufferUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC buffersrvdesc;
	ZeroMemory(&buffersrvdesc, sizeof(buffersrvdesc));
	buffersrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	buffersrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	buffersrvdesc.Buffer.ElementWidth = GI_VOXELS_COUNT * GI_CASCADES_COUNT;
	HR(md3dDevice->CreateShaderResourceView(mLightingBuffer, &buffersrvdesc, &mLightingBufferSRV));
}

void VolumetricEffect::BuildOffscreenQuad(void)
{
	FogVertex v[4];

	v[0].PosL = XMFLOAT3(-1.0f, -1.0f, 0.0f);
	v[1].PosL = XMFLOAT3(-1.0f, +1.0f, 0.0f);
	v[2].PosL = XMFLOAT3(+1.0f, +1.0f, 0.0f);
	v[3].PosL = XMFLOAT3(+1.0f, -1.0f, 0.0f);

	v[0].Tex = XMFLOAT2(0.0f, 1.0f);
	v[1].Tex = XMFLOAT2(0.0f, 0.0f);
	v[2].Tex = XMFLOAT2(1.0f, 0.0f);
	v[3].Tex = XMFLOAT2(1.0f, 1.0f);

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(FogVertex) * 4;
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

void VolumetricEffect::BuildInputLayout(void)
{
	D3D11_INPUT_ELEMENT_DESC Desc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3DX11_PASS_DESC passDesc;

	mDrawFogTech->GetPassByIndex(0)->GetDesc(&passDesc);

	HR(md3dDevice->CreateInputLayout(Desc, 2, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize,
		&mInputLayout));
}

void VolumetricEffect::OnSize(FLOAT ClientH, FLOAT ClientW)
{
	mClientH = ClientH;
	mClientW = ClientW;

	InitViewport(&mScreenViewport, 0.0f, 0.0f, mClientW, mClientH, 0.0f, 1.0f);

	if (md3dDevice) {
		if (pBlurFilter)
			pBlurFilter->OnSize(md3dDevice, mClientW / 2, mClientH / 2, DXGI_FORMAT_R32G32B32A32_FLOAT, false, 0);
		BuildViews();
	}
}

void VolumetricEffect::Update(Camera cam, const XMFLOAT4* GridCenterPosW, FLOAT time)
{
	mTime = time;
	mCamera = cam;
	for (int i = 0; i < GI_CASCADES_COUNT; ++i)
	{
		mGridCenterPosW[i] = GridCenterPosW[i];
	}
}

void VolumetricEffect::ComputeLighting(
	UINT dirLightsNum, DirectionalLight* dirLight,
	UINT spotLightsNum, SpotLight* spotLight,
	UINT pointLightsNum, PointLight* pointLight)
{
	mfxLightingBufferUAV->SetUnorderedAccessView(mLightingBufferUAV);
	mfxView->SetMatrix(reinterpret_cast<float*>(&mCamera.View()));

	mfxGridCellSizes->SetRawValue(reinterpret_cast<float*>(&mGridCellSizes), 0, sizeof(mGridCellSizes));
	mfxGridCenterPosW->SetRawValue(reinterpret_cast<float*>(&mGridCenterPosW), 0, sizeof(mGridCenterPosW));
	mfxInvertedGridCellSizes->SetRawValue(reinterpret_cast<float*>(&mInvertedGridCellSizes), 0, sizeof(mInvertedGridCellSizes));

	mfxEyePosW->SetFloatVector(reinterpret_cast<float*>(&(mCamera.GetPosition())));

	mfxDirLightsNum->SetInt(static_cast<int>(dirLightsNum));
	mfxDirLight->SetRawValue(dirLight, 0, sizeof(DirectionalLight) * 16);

	mfxSpotLightsNum->SetInt(static_cast<int>(spotLightsNum));
	mfxSpotLight->SetRawValue(spotLight, 0, sizeof(SpotLight) * 16);

	mfxPointLightsNum->SetInt(static_cast<int>(pointLightsNum));
	mfxPointLight->SetRawValue(pointLight, 0, sizeof(PointLight) * 16);

	mfxTime->SetFloat(mTime);

	D3DX11_TECHNIQUE_DESC techDesc;
	mComputeLightingTech->GetDesc(&techDesc);

	for (int i = 0; i < GI_CASCADES_COUNT; ++i)
	{
		mfxProcessingCascadeIndex->SetInt(i);
		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			mComputeLightingTech->GetPassByIndex(p)->Apply(0, mDC);
			mDC->Dispatch(GI_CS_THREAD_GROUPS_NUMBER, GI_CS_THREAD_GROUPS_NUMBER, GI_CS_THREAD_GROUPS_NUMBER);
		}
	}

	mDC->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	mDC->CSSetShaderResources(0, 1, pNullSRV);
	mDC->CSSetShader(0, 0, 0);
}

void VolumetricEffect::AccumulateScattering(
	UINT dirLightsNum, DirectionalLight* dirLight,
	UINT spotLightsNum, SpotLight* spotLight,
	UINT pointLightsNum, PointLight* pointLight,
	ShadowMapping* pSM,
	ID3D11ShaderResourceView* PosWMapSRV,
	ID3D11Texture2D* Backbuffer)
{
	mfxLightingBufferSRV->SetResource(mLightingBufferSRV);
	
	mfxView->SetMatrix(reinterpret_cast<float*>(&mCamera.View()));
	mfxInvProj->SetMatrix(reinterpret_cast<float*>(&XMMatrixInverse(&XMMatrixDeterminant(mCamera.Proj()), mCamera.Proj())));
	mfxInvView->SetMatrix(reinterpret_cast<float*>(&XMMatrixInverse(&XMMatrixDeterminant(mCamera.View()), mCamera.View())));

	mfxGridCellSizes->SetRawValue(reinterpret_cast<float*>(&mGridCellSizes), 0, sizeof(mGridCellSizes));
	mfxGridCenterPosW->SetRawValue(reinterpret_cast<float*>(&mGridCenterPosW), 0, sizeof(mGridCenterPosW));
	mfxInvertedGridCellSizes->SetRawValue(reinterpret_cast<float*>(&mInvertedGridCellSizes), 0, sizeof(mInvertedGridCellSizes));

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

	mfxPosWMapSRV->SetResource(PosWMapSRV);

	mfxDistributionDensityMapSRV->SetResource(mDistributionDensityMapSRV);
	mfxWorleyNoiseMapSRV->SetResource(mWorleyNoiseMapSRV);
	mfxPerlinNoiseMapSRV->SetResource(mPerlinNoiseMapSRV);

	mfxClientH->SetFloat(mClientH / 2);
	mfxClientW->SetFloat(mClientW / 2);

	mfxTime->SetFloat(mTime);

	mfxOutputFogColorUAV->SetUnorderedAccessView(mOutputFogColorUAV);

	D3DX11_TECHNIQUE_DESC techDesc;
	mAccumulateScatteringTech->GetDesc(&techDesc);

	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		mAccumulateScatteringTech->GetPassByIndex(p)->Apply(0, mDC);

		mDC->Dispatch(mClientW / 2 / NUM_THREADS_X, mClientH / 2 / NUM_THREADS_Y, 1);
	}

	mDC->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	mDC->CSSetShaderResources(0, 1, pNullSRV);
	mDC->CSSetShader(0, 0, 0);
}

void VolumetricEffect::Draw(ID3D11RenderTargetView* rtv)
{
	mDC->OMSetRenderTargets(1, &rtv, 0);
	mDC->RSSetViewports(1, &mScreenViewport);

	mfxOutputFogColorSRV->SetResource(mOutputFogColorSRV);

	const UINT stride = sizeof(FogVertex);
	const UINT offset = 0;

	mDC->IASetInputLayout(mInputLayout);
	mDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mDC->IASetVertexBuffers(0, 1, &mScreenQuadVB, &stride, &offset);
	mDC->IASetIndexBuffer(mScreenQuadIB, DXGI_FORMAT_R16_UINT, 0);

	mDC->OMSetBlendState(pRenderStates->VolumetricBS, 0, 0xffffffff);
	mDC->OMSetDepthStencilState(pRenderStates->NoDepthStencilTestDSS, 0);

	ID3DX11EffectTechnique* tech = mDrawFogTech;
	D3DX11_TECHNIQUE_DESC techDesc;

	tech->GetDesc(&techDesc);
	for (int p = 0; p < techDesc.Passes; ++p)
	{
		tech->GetPassByIndex(p)->Apply(0, mDC);
		mDC->DrawIndexed(6, 0, 0);
	}

	mDC->OMSetBlendState(0, 0, 0xffffffff);
	mDC->OMSetDepthStencilState(0, 0);
}

void VolumetricEffect::ComputeFog(ShadowMapping* pSM,
	UINT dirLightsNum, DirectionalLight* dirLight,
	UINT spotLightsNum, SpotLight* spotLight,
	UINT pointLightsNum, PointLight* pointLight,
	ID3D11ShaderResourceView* PosWMapSRV,
	ID3D11Texture2D* Backbuffer)
{
	ComputeLighting(dirLightsNum, dirLight, spotLightsNum, spotLight, pointLightsNum, pointLight);
	AccumulateScattering(dirLightsNum, dirLight, spotLightsNum, spotLight, pointLightsNum, pointLight, pSM, PosWMapSRV, Backbuffer);
	pBlurFilter->BlurInPlace(mDC, mOutputFogColorSRV, mOutputFogColorUAV, 1);
}