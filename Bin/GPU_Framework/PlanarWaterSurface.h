#ifndef _PLANAR_WATER_SURFACE_H_
#define _PLANAR_WATER_SURFACE_H_

#include "d3dUtil.h"
#include "Camera.h"
#include "MeshManager.h"

class PlanarWaterSurface
{
private:
	//Interfaces
	ID3D11Device* md3dDevice;
	ID3D11DeviceContext* mDC;
	//FX
	ID3DX11Effect* mDrawFX;
	ID3DX11EffectTechnique* mWavesTech;
	ID3DX11EffectTechnique* mBuildShadowMapWavesTech;
	ID3DX11EffectTechnique* mBuildNormalDepthMapWavesTech;
	ID3DX11EffectTechnique* mBuildAmbientMapWavesTech;
	ID3DX11EffectTechnique* mBuildDiffuseMapWavesTech;
	ID3DX11EffectTechnique* mBuildDepthMapWavesTech;
	ID3DX11EffectTechnique* mBuildWSPositionMapWavesTech;
	//Parameters
	XMFLOAT3 mLookPositionL;
	XMFLOAT3 mLookPositionW;
	XMFLOAT3 mWaterNormalL;
	XMFLOAT3 mWaterNormalW;

	XMVECTOR mLookPositionLXM;
	XMVECTOR mLookPositionWXM;
	XMVECTOR mWaterNormalLXM;
	XMVECTOR mWaterNormalWXM;

	XMMATRIX mWaterWorldMatrixXM;

	XMFLOAT4X4 mWaterWorldMatrix;

	XMMATRIX mWaterReflectionView;

	float mWaterDist;
	float mWaterHeight;

	ID3D11RenderTargetView* mWaterReflectionRTV;
	ID3D11ShaderResourceView* mWaterReflectionSRV;
	ID3D11DepthStencilView* mWaterReflectionDSV;
	D3D11_VIEWPORT mWaterReflectionViewport;

	ID3D11RenderTargetView* mWaterRefractionRTV;
	ID3D11ShaderResourceView* mWaterRefractionSRV;
	ID3D11DepthStencilView* mWaterRefractionDSV;
	D3D11_VIEWPORT mWaterRefractionViewport;

	MeshManager* pMeshManager;
	Camera mWaterReflectionCamera;

	int mWaterMeshIndex;

	ID3DX11EffectShaderResourceVariable* mfxWaterReflectionTex;
	ID3DX11EffectShaderResourceVariable* mfxWaterRefractionTex;
	ID3DX11EffectScalarVariable* mfxDrawWaterReflection;
	ID3DX11EffectScalarVariable* mfxDrawWaterRefraction;
	ID3DX11EffectScalarVariable* mfxWaterHeight;

	bool mDrawWaterReflection;
	bool mDrawWaterRefraction;

	int mWaterRenderTargetResolution;
private:
	VOID PlanarWaterSurface::BuildFX(VOID);

	VOID PlanarWaterSurface::WaterComputeLookPos(Camera &cam);
	VOID PlanarWaterSurface::BuildWaterReflectionCamera(Camera &cam);
	VOID PlanarWaterSurface::BuildWaterReflectionViews(VOID);
	VOID PlanarWaterSurface::BuildWaterRefractionViews(VOID);
public:
	PlanarWaterSurface::PlanarWaterSurface(ID3D11Device* device, ID3DX11Effect* drawFX, MeshManager* meshManager, FLOAT waterHeight, UINT waterRenderTargetResolution,
		XMFLOAT3 waterNormal, XMFLOAT2 waterSurfaceSize);
	VOID PlanarWaterSurface::Update(Camera &cam);

	D3D11_VIEWPORT PlanarWaterSurface::GetReflectionViewport(VOID) const { return mWaterReflectionViewport; };
	D3D11_VIEWPORT PlanarWaterSurface::GetRefractionViewport(VOID) const { return mWaterRefractionViewport; };

	ID3D11ShaderResourceView* PlanarWaterSurface::GetWaterReflectionSRV(VOID) const { return mWaterReflectionSRV; };
	ID3D11RenderTargetView* PlanarWaterSurface::GetWaterReflectionRTV(VOID) const { return mWaterReflectionRTV; };
	ID3D11DepthStencilView* PlanarWaterSurface::GetWaterReflectionDSV(VOID) const { return mWaterReflectionDSV; };

	ID3D11ShaderResourceView* PlanarWaterSurface::GetWaterRefractionSRV(VOID) const { return mWaterRefractionSRV; };
	ID3D11RenderTargetView* PlanarWaterSurface::GetWaterRefractionRTV(VOID) const { return mWaterRefractionRTV; };
	ID3D11DepthStencilView* PlanarWaterSurface::GetWaterRefractionDSV(VOID) const { return mWaterRefractionDSV; };

	UINT PlanarWaterSurface::GetWaterMeshIndex(VOID) const { return mWaterMeshIndex; };
	UINT PlanarWaterSurface::GetWaterRenderTargetResolution(void) const { return mWaterRenderTargetResolution; };

	Camera PlanarWaterSurface::GetReflectionCamera(VOID) const { return mWaterReflectionCamera; };

	VOID PlanarWaterSurface::SetResources(VOID);
	VOID PlanarWaterSurface::SelectDrawTech(DrawPassType dpt, DrawShadowType dst);

	VOID PlanarWaterSurface::SetDrawReflectionFlag(BOOL f) { mDrawWaterReflection = f; };
	VOID PlanarWaterSurface::SetDrawRefractionFlag(BOOL f) { mDrawWaterRefraction = f; };

	BOOL PlanarWaterSurface::GetDrawReflectionFlag() { return mDrawWaterReflection; };
	BOOL PlanarWaterSurface::GetDrawRefractionFlag() { return mDrawWaterRefraction; };
};

#endif