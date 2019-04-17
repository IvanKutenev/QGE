#ifndef _ENV_MAP_H_
#define _ENVIRONMENT_MAP_H_

#include "d3dUtil.h"
#include "Camera.h"
#include "GeometryGenerator.h"

class EnvironmentMap
{
private:
	ID3D11Device* mDevice;
	ID3D11DeviceContext* mDC;

	ID3D11Buffer* mEnvMapVB;
	ID3D11Buffer* mEnvMapIB;

	ID3D11ShaderResourceView* mCubeMapSRV;

	ID3DX11EffectShaderResourceVariable* mfxCubeMap;
	ID3DX11EffectMatrixVariable* mfxEnvMapWorldViewProj;

	UINT mIndexCount;

	ID3DX11Effect* mEnvMapFX;
	ID3D11InputLayout* mEnvMapInputLayout;
	ID3DX11EffectTechnique* mEnvMapTech;
private:
	void EnvironmentMap::BuildVertexInputLayout();
	void EnvironmentMap::BuildFX();

	void EnvironmentMap::BuildBuffers(const std::wstring& cubemapFilename, float skySphereRadius);
public:
	EnvironmentMap::EnvironmentMap(ID3D11Device* device, ID3D11DeviceContext* dc, std::wstring cubemapFileName, FLOAT skySphereRadius);

	void EnvironmentMap::Draw(const Camera& camera,	ID3D11RenderTargetView** renderTargets,	ID3D11DepthStencilView* depthStencilView);

	ID3D11ShaderResourceView* EnvironmentMap::GetCubeMapSRV();
};

#endif
