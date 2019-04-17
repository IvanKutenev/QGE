#include "PPLL.h"

PPLL::PPLL(ID3D11Device* device, ID3D11DeviceContext* dc, ID3DX11Effect* fx, FLOAT clientH, FLOAT clientW) :
mHeadIndexBufferSRV(0), mHeadIndexBufferUAV(0),
FragmentsListBuffer(0), mFragmentsListBufferSRV(0), mFragmentsListBufferUAV(0)
{
	md3dDevice = device;
	mDC = dc;

	mPPLL_FX = fx;
	
	OnSize(clientH, clientW);
	BuildFX();
}

void PPLL::OnSize(FLOAT clientH, FLOAT clientW)
{
	ClientH = clientH;
	ClientW = clientW;
	BuildBuffers();
}

void PPLL::BuildBuffers()
{
	ReleaseCOM(mHeadIndexBufferSRV);
	ReleaseCOM(mHeadIndexBufferUAV);

	ReleaseCOM(FragmentsListBuffer);
	ReleaseCOM(mFragmentsListBufferSRV);
	ReleaseCOM(mFragmentsListBufferUAV);

	D3D11_TEXTURE2D_DESC headTexDesc;
	headTexDesc.Width = ClientW;
	headTexDesc.Height = ClientH;
	headTexDesc.MipLevels = 1;
	headTexDesc.ArraySize = 1;
	headTexDesc.Format = DXGI_FORMAT_R32_UINT;
	headTexDesc.SampleDesc.Count = 1;
	headTexDesc.SampleDesc.Quality = 0;
	headTexDesc.Usage = D3D11_USAGE_DEFAULT;
	headTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	headTexDesc.CPUAccessFlags = 0;
	headTexDesc.MiscFlags = 0;

	ID3D11Texture2D* headTex;
	HR(md3dDevice->CreateTexture2D(&headTexDesc, 0, &headTex));
	HR(md3dDevice->CreateShaderResourceView(headTex, 0, &mHeadIndexBufferSRV));
	HR(md3dDevice->CreateUnorderedAccessView(headTex, 0, &mHeadIndexBufferUAV));
	ReleaseCOM(headTex);

	D3D11_BUFFER_DESC fraglistdesc;
	ZeroMemory(&fraglistdesc, sizeof(fraglistdesc));
	fraglistdesc.Usage = D3D11_USAGE_DEFAULT;
	fraglistdesc.ByteWidth = sizeof(ListNode) * ClientH * ClientW * MAX_PPLL_BUFFER_DEPTH;
	fraglistdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	fraglistdesc.CPUAccessFlags = 0;
	fraglistdesc.StructureByteStride = sizeof(ListNode);
	fraglistdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	HR(md3dDevice->CreateBuffer(&fraglistdesc, NULL, &FragmentsListBuffer));

	D3D11_UNORDERED_ACCESS_VIEW_DESC fraglistuavdesc;
	ZeroMemory(&fraglistuavdesc, sizeof(fraglistuavdesc));
	fraglistuavdesc.Format = DXGI_FORMAT_UNKNOWN;
	fraglistuavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	fraglistuavdesc.Buffer.FirstElement = 0;
	fraglistuavdesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
	fraglistuavdesc.Buffer.NumElements = ClientH * ClientW * MAX_PPLL_BUFFER_DEPTH;
	HR(md3dDevice->CreateUnorderedAccessView(FragmentsListBuffer, &fraglistuavdesc, &mFragmentsListBufferUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC fraglistsrvdesc;
	ZeroMemory(&fraglistsrvdesc, sizeof(fraglistsrvdesc));
	fraglistsrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	fraglistsrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	fraglistsrvdesc.Buffer.ElementWidth = ClientH * ClientW * MAX_PPLL_BUFFER_DEPTH;
	HR(md3dDevice->CreateShaderResourceView(FragmentsListBuffer, &fraglistsrvdesc, &mFragmentsListBufferSRV));
}

void PPLL::Setup()
{
	UINT CleanVal = PPLL_BUFFER_CLEAN_VALUE;
	mDC->ClearUnorderedAccessViewUint(mHeadIndexBufferUAV, &CleanVal);
}

void PPLL::SetResources()
{
	mfxClientW->SetFloat(ClientW);
	mfxClientH->SetFloat(ClientH);

	mfxFragmentsListBufferUAV->SetUnorderedAccessView(mFragmentsListBufferUAV);
	mfxHeadIndexBufferUAV->SetUnorderedAccessView(mHeadIndexBufferUAV);
}

void PPLL::BuildFX()
{
	mfxHeadIndexBufferUAV = mPPLL_FX->GetVariableByName("HeadIndexBuffer")->AsUnorderedAccessView();
	mfxFragmentsListBufferUAV = mPPLL_FX->GetVariableByName("FragmentsListBuffer")->AsUnorderedAccessView();

	mfxClientH = mPPLL_FX->GetVariableByName("clientH")->AsScalar();
	mfxClientW = mPPLL_FX->GetVariableByName("clientW")->AsScalar();
}