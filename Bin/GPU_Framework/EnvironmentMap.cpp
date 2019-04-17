#include "EnvironmentMap.h"

EnvironmentMap::EnvironmentMap(ID3D11Device* device, ID3D11DeviceContext* dc, std::wstring cubemapFileName, FLOAT skySphereRadius)
{
	mDevice = device;
	mDC = dc;

	BuildBuffers(cubemapFileName, skySphereRadius);
	BuildFX();
	BuildVertexInputLayout();
}

ID3D11ShaderResourceView* EnvironmentMap::GetCubeMapSRV()
{
	return mCubeMapSRV;
}

void EnvironmentMap::Draw(const Camera& camera,	ID3D11RenderTargetView** renderTargets, ID3D11DepthStencilView* depthStencilView)
{
	mDC->OMSetRenderTargets(1, renderTargets, depthStencilView);

	XMFLOAT3 eyePos = camera.GetPosition();
	XMMATRIX T = XMMatrixTranslation(eyePos.x, eyePos.y, eyePos.z);

	XMMATRIX WVP = XMMatrixMultiply(T, camera.ViewProj());

	mfxEnvMapWorldViewProj->SetMatrix(reinterpret_cast<const float*>(&WVP));
	mfxCubeMap->SetResource(mCubeMapSRV);

	const UINT stride = sizeof(XMFLOAT3);
	const UINT offset = 0;
	mDC->IASetVertexBuffers(0, 1, &mEnvMapVB, &stride, &offset);
	mDC->IASetIndexBuffer(mEnvMapIB, DXGI_FORMAT_R16_UINT, 0);
	mDC->IASetInputLayout(mEnvMapInputLayout);
	mDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3DX11_TECHNIQUE_DESC techDesc;
	mEnvMapTech->GetDesc(&techDesc);

	for (int p = 0; p < techDesc.Passes; ++p)
	{
		mEnvMapTech->GetPassByIndex(p)->Apply(0, mDC);
		mDC->DrawIndexed(mIndexCount, 0, 0);
	}
}

void EnvironmentMap::BuildBuffers(const std::wstring& cubemapFilename, float skySphereRadius)
{
	mCubeMapSRV = d3dHelper::CreateTexture2DSRVW(mDevice, mDC, cubemapFilename.c_str());

	GeometryGenerator::MeshData sphere;
	GeometryGenerator geoGen;
	geoGen.CreateSphere(skySphereRadius, 30, 30, sphere);

	std::vector<XMFLOAT3> vertices(sphere.Vertices.size());

	for (int i = 0; i < sphere.Vertices.size(); ++i)
	{
		vertices[i] = sphere.Vertices[i].Position;
	}

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(XMFLOAT3)* vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &vertices[0];

	mDevice->CreateBuffer(&vbd, &vinitData, &mEnvMapVB);

	mIndexCount = sphere.Indices.size();

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(USHORT)* mIndexCount;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.StructureByteStride = 0;
	ibd.MiscFlags = 0;

	std::vector<USHORT> indices16;
	indices16.assign(sphere.Indices.begin(), sphere.Indices.end());

	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &indices16[0];

	mDevice->CreateBuffer(&ibd, &iinitData, &mEnvMapIB);
}

void EnvironmentMap::BuildFX()
{
	mEnvMapFX = d3dHelper::CreateEffectFromMemory(L"FX/EnvMap.cso", mDevice);

	mEnvMapTech = mEnvMapFX->GetTechniqueByName("EnvMapTech");
	mfxEnvMapWorldViewProj = mEnvMapFX->GetVariableByName("gWorldViewProj")->AsMatrix();
	mfxCubeMap = mEnvMapFX->GetVariableByName("gCubeMap")->AsShaderResource();
}

void EnvironmentMap::BuildVertexInputLayout()
{
	static const D3D11_INPUT_ELEMENT_DESC SkyDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	D3DX11_PASS_DESC passDesc;

	mEnvMapTech->GetPassByIndex(0)->GetDesc(&passDesc);

	HR(mDevice->CreateInputLayout(SkyDesc, 1, passDesc.pIAInputSignature,
		passDesc.IAInputSignatureSize, &mEnvMapInputLayout));
}