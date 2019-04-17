#ifndef _VOLUMETRIC_EFFECT_H_
#define _VOLUMETRIC_EFFECT_H_

#include "../Common/d3dUtil.h"
#include "GlobalIllumination.h"
#include "ShadowMapping.h"
#include "BlurFilter.h"
#include "RenderStates.h"
#include "Camera.h"

#define NUM_THREADS_X 8
#define NUM_THREADS_Y 8

struct FogVertex
{
	XMFLOAT3 PosL;
	XMFLOAT2 Tex;
};

struct LightingData
{
	XMUINT4 lighting[12];
};

class VolumetricEffect
{
private:
	//device and context///////////////////////////////////////////////////////////////////////////
	ID3D11Device* md3dDevice;
	ID3D11DeviceContext* mDC;
	//grid parameters//////////////////////////////////////////////////////////////////////////////
	XMFLOAT4 mGridCenterPosW[GI_CASCADES_COUNT];
	XMFLOAT4 mInvertedGridCellSizes[GI_CASCADES_COUNT];
	XMFLOAT4 mGridCellSizes[GI_CASCADES_COUNT];
	//output fog color map views///////////////////////////////////////////////////////////////////
	ID3D11ShaderResourceView* mOutputFogColorSRV;
	ID3D11UnorderedAccessView* mOutputFogColorUAV;
	//density and accumulated per voxel light buffer///////////////////////////////////////////////
	ID3D11Buffer* mLightingBuffer;
	ID3D11ShaderResourceView* mLightingBufferSRV;
	ID3D11UnorderedAccessView* mLightingBufferUAV;
	
	ID3D11ShaderResourceView* mDistributionDensityMapSRV;
	ID3D11ShaderResourceView* mWorleyNoiseMapSRV;
	ID3D11ShaderResourceView* mPerlinNoiseMapSRV;
	//screen size//////////////////////////////////////////////////////////////////////////////////
	FLOAT mClientH;
	FLOAT mClientW;
	//timer////////////////////////////////////////////////////////////////////////////////////////
	FLOAT mTime;
	//player camera////////////////////////////////////////////////////////////////////////////////
	Camera mCamera;
	//Illuminate voxels and accumulate light from each light source towards each voxel/////////////
	ID3DX11Effect* mVolumetricFX;
	ID3DX11EffectTechnique* mComputeLightingTech;
	ID3DX11EffectTechnique* mAccumulateScatteringTech;
	ID3DX11EffectTechnique* mDrawFogTech;
	//FX variables/////////////////////////////////////////////////////////////////////////////////
	ID3DX11EffectUnorderedAccessViewVariable* mfxOutputFogColorUAV;
	ID3DX11EffectShaderResourceVariable* mfxOutputFogColorSRV;

	ID3DX11EffectShaderResourceVariable* mfxWorleyNoiseMapSRV;
	ID3DX11EffectShaderResourceVariable* mfxPerlinNoiseMapSRV;
	ID3DX11EffectShaderResourceVariable* mfxDistributionDensityMapSRV;

	ID3DX11EffectShaderResourceVariable* mfxPosWMapSRV;
	ID3DX11EffectShaderResourceVariable* mfxLightingBufferSRV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxLightingBufferUAV;

	ID3DX11EffectVariable* mfxGridCenterPosW;
	ID3DX11EffectVariable* mfxGridCellSizes;
	ID3DX11EffectVariable* mfxInvertedGridCellSizes;
	ID3DX11EffectScalarVariable* mfxProcessingCascadeIndex;

	ID3DX11EffectVectorVariable* mfxEyePosW;
	ID3DX11EffectMatrixVariable* mfxView;
	ID3DX11EffectMatrixVariable* mfxInvProj;
	ID3DX11EffectMatrixVariable* mfxInvView;

	ID3DX11EffectVariable* mfxDirLight;
	ID3DX11EffectVariable* mfxPointLight;
	ID3DX11EffectVariable* mfxSpotLight;

	ID3DX11EffectScalarVariable* mfxDirLightsNum;
	ID3DX11EffectScalarVariable* mfxSpotLightsNum;
	ID3DX11EffectScalarVariable* mfxPointLightsNum;
	
	ID3DX11EffectShaderResourceVariable* mfxDirLightShadowMapArray;
	ID3DX11EffectVariable* mfxDirLightShadowTransformArray;
	ID3DX11EffectVariable* mfxDirLightSplitDistances;

	ID3DX11EffectShaderResourceVariable* mfxSpotLightShadowMapArray;
	ID3DX11EffectVariable* mfxSpotLightShadowTransformArray;

	ID3DX11EffectShaderResourceVariable* mfxPointLightCubicShadowMapArray;
	
	ID3DX11EffectScalarVariable* mfxClientH;
	ID3DX11EffectScalarVariable* mfxClientW;

	ID3DX11EffectScalarVariable* mfxTime;

	//post processing and final rendering
	BlurFilter* pBlurFilter;
	ID3D11Buffer* mScreenQuadVB;
	ID3D11Buffer* mScreenQuadIB;
	ID3D11InputLayout* mInputLayout;
	D3D11_VIEWPORT mScreenViewport;
	RenderStates* pRenderStates;
private:
	void VolumetricEffect::BuildFX(void);
	void VolumetricEffect::BuildDensityTexturesViews(void);
	void VolumetricEffect::BuildViews(void);
	void VolumetricEffect::BuildBuffers(void);
	void VolumetricEffect::BuildInputLayout(void);
	void VolumetricEffect::BuildOffscreenQuad(void);

	void VolumetricEffect::ComputeLighting(
		UINT dirLightsNum, DirectionalLight* dirLight,
		UINT spotLightsNum, SpotLight* spotLight,
		UINT pointLightsNum, PointLight* pointLight);

	void VolumetricEffect::AccumulateScattering(
		UINT dirLightsNum, DirectionalLight* dirLight,
		UINT spotLightsNum, SpotLight* spotLight,
		UINT pointLightsNum, PointLight* pointLight,
		ShadowMapping* pSM,
		ID3D11ShaderResourceView* PosWMapSRV,
		ID3D11Texture2D* Backbuffer);

public:
	VolumetricEffect::VolumetricEffect(ID3D11Device* device, ID3D11DeviceContext* dc,
		FLOAT ClientH, FLOAT ClientW,
		const XMFLOAT4* GridCenterPosW,
		const XMFLOAT4* InvertedGridCellSizes,
		const XMFLOAT4* GridCellSizes,
		Camera cam);

	void VolumetricEffect::ComputeFog(ShadowMapping* pSM,
		UINT dirLightsNum, DirectionalLight* dirLight,
		UINT spotLightsNum, SpotLight* spotLight,
		UINT pointLightsNum, PointLight* pointLight,
		ID3D11ShaderResourceView* PosWMapSRV,
		ID3D11Texture2D* Backbuffer);

	void VolumetricEffect::Draw(ID3D11RenderTargetView* rtv);
	void VolumetricEffect::OnSize(FLOAT ClientH, FLOAT ClientW);
	void VolumetricEffect::Update(Camera cam, const XMFLOAT4* GridCenterPosW, FLOAT time);
	
	ID3D11ShaderResourceView* VolumetricEffect::GetOutputFogColorSRV(void) const { return pBlurFilter->GetBlurredOutput(); };
};

#endif
