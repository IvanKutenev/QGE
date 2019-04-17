#include "OIT.h"

OIT::OIT(ID3D11Device* device, ID3D11DeviceContext* dc, ID3D11ShaderResourceView* headibSRV, ID3D11ShaderResourceView* fraglbSRV, FLOAT clientH, FLOAT clientW) : mOIT_SRV(0), mOIT_RTV(0)
{
	md3dDevice = device;
	mDC = dc;
	
	OnSize(headibSRV, fraglbSRV, clientH, clientW);
	
	BuildFX();
	BuildOffscreenQuad();
	BuildTextureViews();
	BuildInputLayout();

	pRenderStates = new RenderStates(device);
}

void OIT::OnSize(ID3D11ShaderResourceView* headibSRV, ID3D11ShaderResourceView* fraglbSRV, FLOAT clientH, FLOAT clientW)
{
	ClientH = clientH;
	ClientW = clientW;

	InitViewport(&mScreenViewport, 0.0f, 0.0f, clientW, clientH, 0.0f, 1.0f);

	mHeadIndexBufferSRV = headibSRV;
	mFragmentsListBufferSRV = fraglbSRV;

	BuildTextureViews();
}

void OIT::Compute(void)
{
	ID3D11RenderTargetView* renderTargets[1] = { mOIT_RTV };
	mDC->OMSetRenderTargets(1, renderTargets, 0);
	mDC->ClearRenderTargetView(mOIT_RTV, reinterpret_cast<const float*>(&Colors::Black));
	mDC->RSSetViewports(1, &mScreenViewport);

	mfxClientW->SetInt(ClientW);
	mfxClientH->SetInt(ClientH);

	mfxFragmentsListBufferSRV->SetResource(mFragmentsListBufferSRV);
	mfxHeadIndexBufferSRV->SetResource(mHeadIndexBufferSRV);

	const UINT stride = sizeof(OITVertex);
	const UINT offset = 0;

	mDC->IASetInputLayout(mOITInputLayout);
	mDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mDC->IASetVertexBuffers(0, 1, &mScreenQuadVB, &stride, &offset);
	mDC->IASetIndexBuffer(mScreenQuadIB, DXGI_FORMAT_R16_UINT, 0);

	ID3DX11EffectTechnique* tech = OITDrawTech;
	D3DX11_TECHNIQUE_DESC techDesc;

	tech->GetDesc(&techDesc);
	for (int p = 0; p < techDesc.Passes; ++p)
	{
		tech->GetPassByIndex(p)->Apply(0, mDC);
		mDC->DrawIndexed(6, 0, 0);
	}
}

void OIT::Compute(ID3D11RenderTargetView** rtv)
{
	mDC->OMSetRenderTargets(1, rtv, 0);
	mDC->RSSetViewports(1, &mScreenViewport);

	mfxClientW->SetInt(ClientW);
	mfxClientH->SetInt(ClientH);

	mfxFragmentsListBufferSRV->SetResource(mFragmentsListBufferSRV);
	mfxHeadIndexBufferSRV->SetResource(mHeadIndexBufferSRV);

	const UINT stride = sizeof(OITVertex);
	const UINT offset = 0;

	mDC->IASetInputLayout(mOITInputLayout);
	mDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mDC->IASetVertexBuffers(0, 1, &mScreenQuadVB, &stride, &offset);
	mDC->IASetIndexBuffer(mScreenQuadIB, DXGI_FORMAT_R16_UINT, 0);

	mDC->OMSetBlendState(pRenderStates->OitBS, 0, 0xffffffff);
	mDC->OMSetDepthStencilState(pRenderStates->NoDepthStencilTestDSS, 0);

	ID3DX11EffectTechnique* tech = OITDrawTech;
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

void OIT::BuildFX(void)
{
	mOIT_FX = d3dHelper::CreateEffectFromMemory(L"FX/OIT.cso", md3dDevice);

	OITDrawTech = mOIT_FX->GetTechniqueByName("OITDrawTech");

	mfxHeadIndexBufferSRV = mOIT_FX->GetVariableByName("HeadIndexBuffer")->AsShaderResource();
	mfxFragmentsListBufferSRV = mOIT_FX->GetVariableByName("FragmentsListBuffer")->AsShaderResource();

	mfxClientH = mOIT_FX->GetVariableByName("ClientH")->AsScalar();
	mfxClientW = mOIT_FX->GetVariableByName("ClientW")->AsScalar();
}

void OIT::BuildInputLayout(void)
{
	D3D11_INPUT_ELEMENT_DESC OitDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3DX11_PASS_DESC passDesc;

	OITDrawTech->GetPassByIndex(0)->GetDesc(&passDesc);

	HR(md3dDevice->CreateInputLayout(OitDesc, 2, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize,
		&mOITInputLayout));
}

void OIT::BuildOffscreenQuad(void)
{
	OITVertex v[4];

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
	vbd.ByteWidth = sizeof(OITVertex) * 4;
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

void OIT::BuildTextureViews(void)
{
	ReleaseCOM(mOIT_SRV);
	ReleaseCOM(mOIT_RTV);

	D3D11_TEXTURE2D_DESC oitTexDesc;
	oitTexDesc.Width = ClientW;
	oitTexDesc.Height = ClientH;
	oitTexDesc.MipLevels = 1;
	oitTexDesc.ArraySize = 1;
	oitTexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	oitTexDesc.SampleDesc.Count = 1;
	oitTexDesc.SampleDesc.Quality = 0;
	oitTexDesc.Usage = D3D11_USAGE_DEFAULT;
	oitTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	oitTexDesc.CPUAccessFlags = 0;
	oitTexDesc.MiscFlags = 0;

	ID3D11Texture2D* oitTex = 0;
	HR(md3dDevice->CreateTexture2D(&oitTexDesc, 0, &oitTex));
	HR(md3dDevice->CreateShaderResourceView(oitTex, 0, &mOIT_SRV));
	HR(md3dDevice->CreateRenderTargetView(oitTex, 0, &mOIT_RTV));

	ReleaseCOM(oitTex);
}