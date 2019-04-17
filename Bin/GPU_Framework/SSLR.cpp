#include "SSLR.h"

SSLR::SSLR(ID3D11Device* device, ID3D11DeviceContext* dc, float ClientW, float ClientH,
		ID3D11ShaderResourceView* BackbufferColorMapSRV, ID3D11ShaderResourceView* NormalDepthMapSRV,
		ID3D11ShaderResourceView* WSPositionMapSRV, ID3D11ShaderResourceView* SpecularBufferSRV,
	BOOL ForceMipsCount, UINT ForcedMipsCount, UINT WeightingMode, BOOL enableTraceIntervalRestrictions) : mSSLROutputSRV(0), mSSLROutputRTV(0)
{
	md3dDevice = device;
	mDC = dc;
	SSLR::ClientH = ClientH;
	SSLR::ClientW = ClientW;

	if(ForceMipsCount)
		MIPS_COUNT = ForcedMipsCount;
	else
		MIPS_COUNT = min(floor(log(ClientW) / log(2.0f)), floor(log(ClientH) / log(2.0f)));

	mHiZBufferSRV = 0;
	mHiZBufferRTV = 0;
	mVisibilityBufferRTV = 0;
	mVisibilityBufferSRV = 0;
	mRayTracedPositionMapSRV = 0;
	mRayTracedPositionMapRTV = 0;
	mPreConvolutionBufferSRV = 0;
	mWeightingMode = WeightingMode;
	mEnableTraceIntervalRestrictions = enableTraceIntervalRestrictions;

	OnSize(ClientW, ClientH, BackbufferColorMapSRV, NormalDepthMapSRV, WSPositionMapSRV, SpecularBufferSRV);

	BuildFullScreenQuad();
	BuildFX();
	BuildInputLayout();
}

void SSLR::RenderReflections(XMMATRIX ViewMat, XMMATRIX ProjMat, FLOAT NearZ, FLOAT FarZ)
{
	HiZBuildPass();
	if(mWeightingMode == WEIGHTING_MODE_VISIBILITY)
		PreIntegrationPass(NearZ, FarZ);
	RayTracingPass(ViewMat, ProjMat);
	PreConvolutionPass();
	ConeTracingPass(ViewMat);
}

void SSLR::HiZBuildPass()
{
	for(int i = 1; i < MIPS_COUNT; ++i)
	{
		ID3D11RenderTargetView* renderTargets[1] = { mHiZBufferRTVs[i] };
		mDC->OMSetRenderTargets(1, renderTargets, 0);
		mDC->ClearRenderTargetView(mHiZBufferRTVs[i], reinterpret_cast<const float*>(&Colors::White));
		mDC->RSSetViewports(1, &mScreenViewports[i]);

		mfxInputDepthBufferMip->SetResource(mHiZBufferSRVs[i-1]);

		const UINT stride = sizeof(SSLRVertex);
		const UINT offset = 0;

		mDC->IASetInputLayout(mSSLRHiZInputLayout);
		mDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		mDC->IASetVertexBuffers(0, 1, &mScreenQuadVB, &stride, &offset);
		mDC->IASetIndexBuffer(mScreenQuadIB, DXGI_FORMAT_R16_UINT, 0);

		D3DX11_TECHNIQUE_DESC techDesc;
		SSLRBuildHiZTech->GetDesc(&techDesc);
		for (int p = 0; p < techDesc.Passes; ++p)
		{
			SSLRBuildHiZTech->GetPassByIndex(p)->Apply(0, mDC);
			mDC->DrawIndexed(6, 0, 0);
		}
	}
}

void SSLR::PreIntegrationPass(FLOAT NearZ, FLOAT FarZ)
{
	mDC->ClearRenderTargetView(mVisibilityBufferRTVs[0], reinterpret_cast<const float*>(&Colors::White));
	for (int i = 1; i < MIPS_COUNT; ++i)
	{
		ID3D11RenderTargetView* renderTargets[1] = { mVisibilityBufferRTVs[i] };
		mDC->OMSetRenderTargets(1, renderTargets, 0);
		mDC->ClearRenderTargetView(mVisibilityBufferRTVs[i], reinterpret_cast<const float*>(&Colors::Black));
		mDC->RSSetViewports(1, &mScreenViewports[i]);

		mfxPrevVisibilityBufferMip->SetResource(mVisibilityBufferSRVs[i - 1]);
		mfxPrevHiZBufferMip->SetResource(mHiZBufferSRVs[i - 1]);
		mfxCurHiZBufferMip->SetResource(mHiZBufferSRVs[i]);
		mfxNearZ->SetFloat(NearZ);
		mfxFarZ->SetFloat(FarZ);

		const UINT stride = sizeof(SSLRVertex);
		const UINT offset = 0;

		mDC->IASetInputLayout(mSSLRPIInputLayout);
		mDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		mDC->IASetVertexBuffers(0, 1, &mScreenQuadVB, &stride, &offset);
		mDC->IASetIndexBuffer(mScreenQuadIB, DXGI_FORMAT_R16_UINT, 0);

		D3DX11_TECHNIQUE_DESC techDesc;
		SSLRPreIntegrateTech->GetDesc(&techDesc);
		for (int p = 0; p < techDesc.Passes; ++p)
		{
			SSLRPreIntegrateTech->GetPassByIndex(p)->Apply(0, mDC);
			mDC->DrawIndexed(6, 0, 0);
		}
	}
}

void SSLR::RayTracingPass(XMMATRIX ViewMat, XMMATRIX ProjMat)
{
	ID3D11RenderTargetView* renderTargets[1] = { mRayTracedPositionMapRTV };

	mDC->OMSetRenderTargets(1, renderTargets, 0);
	mDC->ClearRenderTargetView(mRayTracedPositionMapRTV, reinterpret_cast<const float*>(&Colors::Black));
	mDC->RSSetViewports(1, &mScreenViewport);

	const UINT stride = sizeof(SSLRVertex);
	const UINT offset = 0;

	mDC->IASetInputLayout(mSSLRRTInputLayout);
	mDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mDC->IASetVertexBuffers(0, 1, &mScreenQuadVB, &stride, &offset);
	mDC->IASetIndexBuffer(mScreenQuadIB, DXGI_FORMAT_R16_UINT, 0);
		
	D3DX11_TECHNIQUE_DESC techDesc;
	SSLRRayTraceTech->GetDesc(&techDesc);
	for (int p = 0; p < techDesc.Passes; ++p)
	{
		mfxHiZBuffer->SetResource(mHiZBufferSRV);
		mfxNormalDepthMap->SetResource(mNormalDepthMapSRV);
		mfxWSPositionMap->SetResource(mWSPositionMapSRV);
		mfxClientHeight->SetFloat(ClientH);
		mfxClientWidth->SetFloat(ClientW);
		mfxMIPS_COUNT->SetInt(MIPS_COUNT);
		mfxEnableTraceIntervalRestrictions->SetBool(mEnableTraceIntervalRestrictions);
		mfxView->SetMatrix(reinterpret_cast<float*>(&ViewMat));
		mfxProj->SetMatrix(reinterpret_cast<float*>(&ProjMat));

		SSLRRayTraceTech->GetPassByIndex(p)->Apply(0, mDC);
		mDC->DrawIndexed(6, 0, 0);
	}
}

void SSLR::PreConvolutionPass()
{
	ID3D11RenderTargetView* renderTargets[1];
	for (int i = 0; i < MIPS_COUNT - 1; ++i)
	{
		renderTargets[0] = mPingPongMapRTVs[i];
		mDC->OMSetRenderTargets(1, renderTargets, 0);
		mDC->ClearRenderTargetView(mPingPongMapRTVs[i], reinterpret_cast<const float*>(&Colors::LightSteelBlue));
		mDC->RSSetViewports(1, &mScreenViewports[i]);

		mfxInputMap->SetResource(mPreConvolutionBufferSRVs[i]);

		const UINT stride = sizeof(SSLRVertex);
		const UINT offset = 0;

		mDC->IASetInputLayout(mSSLRPCInputLayout);
		mDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		mDC->IASetVertexBuffers(0, 1, &mScreenQuadVB, &stride, &offset);
		mDC->IASetIndexBuffer(mScreenQuadIB, DXGI_FORMAT_R16_UINT, 0);

		D3DX11_TECHNIQUE_DESC techDesc;
		SSLRHorzBlurTech->GetDesc(&techDesc);
		for (int p = 0; p < techDesc.Passes; ++p)
		{
			SSLRHorzBlurTech->GetPassByIndex(p)->Apply(0, mDC);
			mDC->DrawIndexed(6, 0, 0);
		}

		renderTargets[0] = mPreConvolutionBufferRTVs[i + 1];
		mDC->OMSetRenderTargets(1, renderTargets, 0);
		mDC->ClearRenderTargetView(mPreConvolutionBufferRTVs[i + 1], reinterpret_cast<const float*>(&Colors::Black));
		mDC->RSSetViewports(1, &mScreenViewports[i + 1]);

		mfxInputMap->SetResource(mPingPongMapSRVs[i]);

		mDC->IASetInputLayout(mSSLRPCInputLayout);
		mDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		mDC->IASetVertexBuffers(0, 1, &mScreenQuadVB, &stride, &offset);
		mDC->IASetIndexBuffer(mScreenQuadIB, DXGI_FORMAT_R16_UINT, 0);

		SSLRVertBlurTech->GetDesc(&techDesc);
		for (int p = 0; p < techDesc.Passes; ++p)
		{
			SSLRVertBlurTech->GetPassByIndex(p)->Apply(0, mDC);
			mDC->DrawIndexed(6, 0, 0);
		}
	}
}

void SSLR::ConeTracingPass(XMMATRIX ViewMat)
{
	ID3D11RenderTargetView* renderTargets[1] = { mSSLROutputRTV };

	mDC->OMSetRenderTargets(1, renderTargets, 0);
	mDC->ClearRenderTargetView(mSSLROutputRTV, reinterpret_cast<const float*>(&Colors::Black));
	mDC->RSSetViewports(1, &mScreenViewport);

	const UINT stride = sizeof(SSLRVertex);
	const UINT offset = 0;

	mDC->IASetInputLayout(mSSLRCTInputLayout);
	mDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mDC->IASetVertexBuffers(0, 1, &mScreenQuadVB, &stride, &offset);
	mDC->IASetIndexBuffer(mScreenQuadIB, DXGI_FORMAT_R16_UINT, 0);

	D3DX11_TECHNIQUE_DESC techDesc;
	SSLRConeTraceTech->GetDesc(&techDesc);
	for (int p = 0; p < techDesc.Passes; ++p)
	{
		mfxVisibilityBuffer->SetResource(GetVisibilityBufferSRV());
		mfxColorBuf->SetResource(GetPreConvolutionBufferSRV());
		mfxHiZBuf->SetResource(mHiZBufferSRV);
		mfxWSPosMap->SetResource(mWSPositionMapSRV);
		mfxRayTracedMap->SetResource(mRayTracedPositionMapSRV);
		mfxSpecularBuffer->SetResource(mSpecularBufferSRV);

		mfxCHeight->SetFloat(ClientH);
		mfxCWidth->SetFloat(ClientW);
		mfxViewMat->SetMatrix(reinterpret_cast<float*>(&ViewMat));
		mfxMipsCount->SetInt(MIPS_COUNT);
		mfxWeightingMode->SetInt(mWeightingMode);

		SSLRConeTraceTech->GetPassByIndex(p)->Apply(0, mDC);
		mDC->DrawIndexed(6, 0, 0);
	}
}

void SSLR::OnSize(float ClientW, float ClientH, ID3D11ShaderResourceView* BackbufferColorMapSRV, ID3D11ShaderResourceView* NormalDepthMapSRV,
	ID3D11ShaderResourceView* WSPositionMapSRV, ID3D11ShaderResourceView* SpecularBufferSRV)
{
	SSLR::ClientH = ClientH;
	SSLR::ClientW = ClientW;

	for(int i = 0; i < MIPS_COUNT; ++i)
	{
		mScreenViewports.push_back(D3D11_VIEWPORT());

		mScreenViewports[i].TopLeftX = 0;
		mScreenViewports[i].TopLeftY = 0;
		mScreenViewports[i].Width    = static_cast<float>(ClientW / pow(2.0f, i));
		mScreenViewports[i].Height   = static_cast<float>(ClientH / pow(2.0f, i));
		mScreenViewports[i].MinDepth = 0.0f;
		mScreenViewports[i].MaxDepth = 1.0f;
	}
	mScreenViewport = mScreenViewports[0];
	BuildTextureViews(ClientW, ClientH, BackbufferColorMapSRV, NormalDepthMapSRV, WSPositionMapSRV, SpecularBufferSRV);
}

ID3D11ShaderResourceView* SSLR::GetHiZBufferSRV(UINT MipMapNum) const
{
	return mHiZBufferSRVs[MipMapNum];
}

ID3D11RenderTargetView* SSLR::GetHiZBufferRTV(UINT MipMapNum) const
{
	return mHiZBufferRTVs[MipMapNum];
}

ID3D11ShaderResourceView* SSLR::GetVisibilityBufferSRV(UINT MipMapNum) const
{
	return mVisibilityBufferSRVs[MipMapNum];
}

ID3D11ShaderResourceView* SSLR::GetVisibilityBufferSRV() const
{
	return mVisibilityBufferSRV;
}

ID3D11RenderTargetView* SSLR::GetVisibilityBufferRTV(UINT MipMapNum) const
{
	return mVisibilityBufferRTVs[MipMapNum];
}

ID3D11ShaderResourceView* SSLR::GetRayTracedPositionMapSRV() const
{
	return mRayTracedPositionMapSRV;
}

ID3D11RenderTargetView* SSLR::GetRayTracedPositionMapRTV() const
{
	return mRayTracedPositionMapRTV;
}

ID3D11ShaderResourceView* SSLR::GetPreConvolutionBufferSRV(UINT MipMapNum) const
{
	return mPreConvolutionBufferSRVs[MipMapNum];
}

ID3D11ShaderResourceView* SSLR::GetPreConvolutionBufferSRV() const
{
	return mPreConvolutionBufferSRV;
}

ID3D11RenderTargetView* SSLR::GetPreConvolutionBufferRTV(UINT MipMapNum) const
{
	return mPreConvolutionBufferRTVs[MipMapNum];
}

ID3D11ShaderResourceView* SSLR::GetSSLROutputSRV() const
{
	return mSSLROutputSRV;
}

ID3D11RenderTargetView* SSLR::GetSSLROutputRTV() const
{
	return mSSLROutputRTV;
}

void SSLR::BuildFullScreenQuad()
{
	SSLRVertex v[4];

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
	vbd.ByteWidth = sizeof(SSLRVertex) * 4;
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

void SSLR::BuildInputLayout()
{
	D3D11_INPUT_ELEMENT_DESC SSLRDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3DX11_PASS_DESC passDesc;
	
	SSLRBuildHiZTech->GetPassByIndex(0)->GetDesc(&passDesc);
	HR(md3dDevice->CreateInputLayout(SSLRDesc, 2, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize,
		&mSSLRHiZInputLayout));

	SSLRPreIntegrateTech->GetPassByIndex(0)->GetDesc(&passDesc);
	HR(md3dDevice->CreateInputLayout(SSLRDesc, 2, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize,
		&mSSLRPIInputLayout));

	SSLRRayTraceTech->GetPassByIndex(0)->GetDesc(&passDesc);
	HR(md3dDevice->CreateInputLayout(SSLRDesc, 2, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize,
		&mSSLRRTInputLayout));

	SSLRHorzBlurTech->GetPassByIndex(0)->GetDesc(&passDesc);
	HR(md3dDevice->CreateInputLayout(SSLRDesc, 2, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize,
		&mSSLRPCInputLayout));

	SSLRConeTraceTech->GetPassByIndex(0)->GetDesc(&passDesc);
	HR(md3dDevice->CreateInputLayout(SSLRDesc, 2, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize,
		&mSSLRCTInputLayout));
}

void SSLR::BuildFX()
{
	SSLRBuildHiZFX = d3dHelper::CreateEffectFromMemory(L"FX/SSLRBuildHiZ.cso", md3dDevice);

	SSLRBuildHiZTech = SSLRBuildHiZFX->GetTechniqueByName("SSLRBuildHiZTech");
	mfxInputDepthBufferMip = SSLRBuildHiZFX->GetVariableByName("gInputDepthBufferMip")->AsShaderResource();

	SSLRPreIntegrateFX = d3dHelper::CreateEffectFromMemory(L"FX/SSLRPreIntegrate.cso", md3dDevice);

	SSLRPreIntegrateTech = SSLRPreIntegrateFX->GetTechniqueByName("SSLRPreIntegrateTech");
	mfxPrevHiZBufferMip = SSLRPreIntegrateFX->GetVariableByName("gPrevHiZBufferMip")->AsShaderResource();
	mfxCurHiZBufferMip = SSLRPreIntegrateFX->GetVariableByName("gCurHiZBufferMip")->AsShaderResource();
	mfxPrevVisibilityBufferMip = SSLRPreIntegrateFX->GetVariableByName("gPrevVisibilityBufferMip")->AsShaderResource();
	mfxNearZ = SSLRPreIntegrateFX->GetVariableByName("gNearZ")->AsScalar();
	mfxFarZ = SSLRPreIntegrateFX->GetVariableByName("gFarZ")->AsScalar();

	SSLRRayTraceFX = d3dHelper::CreateEffectFromMemory(L"FX/SSLRRayTracing.cso", md3dDevice);

	SSLRRayTraceTech = SSLRRayTraceFX->GetTechniqueByName("SSLRRayTraceTech");
	mfxHiZBuffer = SSLRRayTraceFX->GetVariableByName("gHiZBuffer")->AsShaderResource();
	mfxColorBuffer = SSLRRayTraceFX->GetVariableByName("gColorBuffer")->AsShaderResource();
	mfxNormalDepthMap = SSLRRayTraceFX->GetVariableByName("gNormalDepthMap")->AsShaderResource();
	mfxWSPositionMap = SSLRRayTraceFX->GetVariableByName("gWSPositionMap")->AsShaderResource();
	mfxClientHeight = SSLRRayTraceFX->GetVariableByName("gClientHeight")->AsScalar();
	mfxClientWidth = SSLRRayTraceFX->GetVariableByName("gClientWidth")->AsScalar();
	mfxView = SSLRRayTraceFX->GetVariableByName("gView")->AsMatrix();
	mfxProj = SSLRRayTraceFX->GetVariableByName("gProj")->AsMatrix();
	mfxMIPS_COUNT = SSLRRayTraceFX->GetVariableByName("MIPS_COUNT")->AsScalar();
	mfxEnableTraceIntervalRestrictions = SSLRRayTraceFX->GetVariableByName("gEnableTraceIntervalRestrictions")->AsScalar();

	SSLRPreConvolutionFX = d3dHelper::CreateEffectFromMemory(L"FX/SSLRPreConvolution.cso", md3dDevice);
	SSLRHorzBlurTech = SSLRPreConvolutionFX->GetTechniqueByName("SSLRHorzBlurTech");
	SSLRVertBlurTech = SSLRPreConvolutionFX->GetTechniqueByName("SSLRVertBlurTech");
	mfxInputMap = SSLRPreConvolutionFX->GetVariableByName("gInputMap")->AsShaderResource();

	SSLRConeTraceFX = d3dHelper::CreateEffectFromMemory(L"FX/SSLRConeTracing.cso", md3dDevice);
	SSLRConeTraceTech = SSLRConeTraceFX->GetTechniqueByName("SSLRConeTraceTech");
	mfxCWidth = SSLRConeTraceFX->GetVariableByName("gClientWidth")->AsScalar();
	mfxCHeight = SSLRConeTraceFX->GetVariableByName("gClientHeight")->AsScalar();
	mfxViewMat = SSLRConeTraceFX->GetVariableByName("gView")->AsMatrix();
	mfxVisibilityBuffer = SSLRConeTraceFX->GetVariableByName("gVisibilityBuffer")->AsShaderResource();
	mfxSpecularBuffer = SSLRConeTraceFX->GetVariableByName("gSpecularBuffer")->AsShaderResource();
	mfxColorBuf = SSLRConeTraceFX->GetVariableByName("gColorBuffer")->AsShaderResource();
	mfxHiZBuf = SSLRConeTraceFX->GetVariableByName("gHiZBuffer")->AsShaderResource();
	mfxRayTracedMap = SSLRConeTraceFX->GetVariableByName("gRayTracedMap")->AsShaderResource();
	mfxWSPosMap = SSLRConeTraceFX->GetVariableByName("gWSPositionMap")->AsShaderResource();
	mfxMipsCount = SSLRConeTraceFX->GetVariableByName("MIPS_COUNT")->AsScalar();
	mfxWeightingMode = SSLRConeTraceFX->GetVariableByName("gWeightingMode")->AsScalar();
}

void SSLR::BuildTextureViews(float ClientW, float ClientH, ID3D11ShaderResourceView* BackbufferColorMapSRV, ID3D11ShaderResourceView* NormalDepthMapSRV,
	ID3D11ShaderResourceView* WSPositionMapSRV, ID3D11ShaderResourceView* SpecularBufferSRV)
{
	ReleaseCOM(mHiZBufferSRV);
	ReleaseCOM(mHiZBufferRTV);
	for (int i = 0; i < mHiZBufferSRVs.size(); ++i)
	{
		ReleaseCOM(mHiZBufferSRVs[i]);
		ReleaseCOM(mHiZBufferRTVs[i]);
	}
	ReleaseCOM(mVisibilityBufferSRV);
	ReleaseCOM(mVisibilityBufferRTV);
	for (int i = 0; i < mVisibilityBufferSRVs.size(); ++i)
	{
		ReleaseCOM(mVisibilityBufferSRVs[i]);
		ReleaseCOM(mVisibilityBufferRTVs[i]);
	}
	ReleaseCOM(mRayTracedPositionMapSRV);
	ReleaseCOM(mRayTracedPositionMapRTV);
	for (int i = 0; i < mPingPongMapSRVs.size(); ++i)
	{
		ReleaseCOM(mPingPongMapSRVs[i]);
		ReleaseCOM(mPingPongMapRTVs[i]);
		ReleaseCOM(mPreConvolutionBufferSRVs[i]);
		ReleaseCOM(mPreConvolutionBufferRTVs[i]);
	}
	ReleaseCOM(mSSLROutputSRV);
	ReleaseCOM(mSSLROutputRTV);

	/////////////////////////////////////////
	mNormalDepthMapSRV = NormalDepthMapSRV;
	mWSPositionMapSRV = WSPositionMapSRV;
	mSpecularBufferSRV = SpecularBufferSRV;
	/////////////////////////////////////////

	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = ClientW;
	texDesc.Height = ClientH;
	texDesc.MipLevels = MIPS_COUNT;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	ID3D11Texture2D* HiZDepthTex = 0;
	HR(md3dDevice->CreateTexture2D(&texDesc, 0, &HiZDepthTex));
	HR(md3dDevice->CreateShaderResourceView(HiZDepthTex, 0, &mHiZBufferSRV));
	HR(md3dDevice->CreateRenderTargetView(HiZDepthTex, 0, &mHiZBufferRTV));

	for(int i = 0; i < MIPS_COUNT; ++i)
	{
		mHiZBufferSRVs.push_back(NULL);
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = i;
		srvDesc.Texture2D.MipLevels = 1;
		HR(md3dDevice->CreateShaderResourceView(HiZDepthTex, &srvDesc, &mHiZBufferSRVs[i]));

		mHiZBufferRTVs.push_back(NULL);
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = i;
		HR(md3dDevice->CreateRenderTargetView(HiZDepthTex, &rtvDesc, &mHiZBufferRTVs[i]));
	}

	ReleaseCOM(HiZDepthTex);

	D3D11_TEXTURE2D_DESC visTexDesc;
	visTexDesc.Width = ClientW;
	visTexDesc.Height = ClientH;
	visTexDesc.MipLevels = MIPS_COUNT;
	visTexDesc.ArraySize = 1;
	visTexDesc.Format = DXGI_FORMAT_R32_FLOAT;
	visTexDesc.SampleDesc.Count = 1;
	visTexDesc.SampleDesc.Quality = 0;
	visTexDesc.Usage = D3D11_USAGE_DEFAULT;
	visTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	visTexDesc.CPUAccessFlags = 0;
	visTexDesc.MiscFlags = 0;

	ID3D11Texture2D* VisTex = 0;
	HR(md3dDevice->CreateTexture2D(&visTexDesc, 0, &VisTex));
	HR(md3dDevice->CreateShaderResourceView(VisTex, 0, &mVisibilityBufferSRV));
	HR(md3dDevice->CreateRenderTargetView(VisTex, 0, &mVisibilityBufferRTV));

	for (int i = 0; i < MIPS_COUNT; ++i)
	{
		mVisibilityBufferSRVs.push_back(NULL);
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = i;
		srvDesc.Texture2D.MipLevels = 1;
		HR(md3dDevice->CreateShaderResourceView(VisTex, &srvDesc, &mVisibilityBufferSRVs[i]));

		mVisibilityBufferRTVs.push_back(NULL);
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = i;
		HR(md3dDevice->CreateRenderTargetView(VisTex, &rtvDesc, &mVisibilityBufferRTVs[i]));
	}

	ReleaseCOM(VisTex);

	D3D11_TEXTURE2D_DESC rtColorTexDesc;
	rtColorTexDesc.Width = ClientW;
	rtColorTexDesc.Height = ClientH;
	rtColorTexDesc.MipLevels = 1;
	rtColorTexDesc.ArraySize = 1;
	rtColorTexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	rtColorTexDesc.SampleDesc.Count = 1;
	rtColorTexDesc.SampleDesc.Quality = 0;
	rtColorTexDesc.Usage = D3D11_USAGE_DEFAULT;
	rtColorTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	rtColorTexDesc.CPUAccessFlags = 0;
	rtColorTexDesc.MiscFlags = 0;

	ID3D11Texture2D* rtColorTex = 0;
	HR(md3dDevice->CreateTexture2D(&rtColorTexDesc, 0, &rtColorTex));

	HR(md3dDevice->CreateShaderResourceView(rtColorTex, 0, &mRayTracedPositionMapSRV));
	HR(md3dDevice->CreateRenderTargetView(rtColorTex, 0, &mRayTracedPositionMapRTV));

	ReleaseCOM(rtColorTex);

	D3D11_TEXTURE2D_DESC ppTexDesc;
	ppTexDesc.Width = ClientW;
	ppTexDesc.Height = ClientH;
	ppTexDesc.MipLevels = MIPS_COUNT;
	ppTexDesc.ArraySize = 1;
	ppTexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	ppTexDesc.SampleDesc.Count = 1;
	ppTexDesc.SampleDesc.Quality = 0;
	ppTexDesc.Usage = D3D11_USAGE_DEFAULT;
	ppTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	ppTexDesc.CPUAccessFlags = 0;
	ppTexDesc.MiscFlags = 0;

	ID3D11Texture2D* ppTex = 0;
	HR(md3dDevice->CreateTexture2D(&ppTexDesc, 0, &ppTex));

	ID3D11Texture2D* pcTex = 0;
	HR(md3dDevice->CreateTexture2D(&ppTexDesc, 0, &pcTex));
	HR(md3dDevice->CreateShaderResourceView(pcTex, 0, &mPreConvolutionBufferSRV));

	for (int i = 0; i < MIPS_COUNT; ++i)
	{
		mPingPongMapSRVs.push_back(NULL);
		mPreConvolutionBufferSRVs.push_back(NULL);
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = i;
		srvDesc.Texture2D.MipLevels = 1;
		HR(md3dDevice->CreateShaderResourceView(ppTex, &srvDesc, &mPingPongMapSRVs[i]));
		HR(md3dDevice->CreateShaderResourceView(pcTex, &srvDesc, &mPreConvolutionBufferSRVs[i]));

		mPingPongMapRTVs.push_back(NULL);
		mPreConvolutionBufferRTVs.push_back(NULL);
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = i;
		HR(md3dDevice->CreateRenderTargetView(ppTex, &rtvDesc, &mPingPongMapRTVs[i]));
		HR(md3dDevice->CreateRenderTargetView(pcTex, &rtvDesc, &mPreConvolutionBufferRTVs[i]));
	}

	ReleaseCOM(ppTex);
	ReleaseCOM(pcTex);

	D3D11_TEXTURE2D_DESC soColorTexDesc;
	soColorTexDesc.Width = ClientW;
	soColorTexDesc.Height = ClientH;
	soColorTexDesc.MipLevels = 1;
	soColorTexDesc.ArraySize = 1;
	soColorTexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	soColorTexDesc.SampleDesc.Count = 1;
	soColorTexDesc.SampleDesc.Quality = 0;
	soColorTexDesc.Usage = D3D11_USAGE_DEFAULT;
	soColorTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	soColorTexDesc.CPUAccessFlags = 0;
	soColorTexDesc.MiscFlags = 0;

	ID3D11Texture2D* soColorTex = 0;
	HR(md3dDevice->CreateTexture2D(&soColorTexDesc, 0, &soColorTex));

	HR(md3dDevice->CreateShaderResourceView(soColorTex, 0, &mSSLROutputSRV));
	HR(md3dDevice->CreateRenderTargetView(soColorTex, 0, &mSSLROutputRTV));
	
	ReleaseCOM(soColorTex);
}