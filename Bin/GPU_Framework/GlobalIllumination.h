#ifndef _GLOBAL_ILLUMINATION_H_
#define _GLOBAL_ILLUMINATION_H_

#include "../Common/d3dUtil.h"
#include "Camera.h"
#include "ShadowMapping.h"

#define GI_VOXEL_GRID_RESOLUTION 40
#define GI_CS_THREAD_GROUPS_NUMBER 4
#define GI_VOXELS_COUNT GI_VOXEL_GRID_RESOLUTION * GI_VOXEL_GRID_RESOLUTION * GI_VOXEL_GRID_RESOLUTION 
#define GI_CASCADES_COUNT 4
#define GI_VPL_PROPOGATIONS_COUNT 20
#define GI_ATTENUATION_FACTOR 2.0f
#define GI_MESH_CULLING_FACTOR 0.5f

struct Voxel
{
	UINT colorMask;
	XMUINT4 normalMasks;
};

struct VPL
{
	XMFLOAT4 redSH;
	XMFLOAT4 greenSH;
	XMFLOAT4 blueSH;
};

//
//TODO: add interpolation between previous and current grid to eliminate flickering!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//

class GlobalIllumination
{
private:
	ID3D11Device* md3dDevice;
	ID3D11DeviceContext* mDC;
	//fx and techniques////////////////////////////////////////////////////////////////////////////
	ID3DX11Effect* mClearGridFX;
	ID3DX11EffectTechnique* mClearGridTech;

	ID3DX11Effect* mGenerateGridFX;

	ID3DX11Effect* mCreateVPLFX;
	ID3DX11EffectTechnique* mCreateVPLTech;

	ID3DX11Effect* mPropagateVPLFX;
	ID3DX11EffectTechnique* mNoOcclusionPropagateVPLTech;
	ID3DX11EffectTechnique* mUseOcclusionPropagateVPLTech;
	//grid parameters//////////////////////////////////////////////////////////////////////////////
	Camera mCamera;
	XMMATRIX mGridViewProjMatrices[6 * GI_CASCADES_COUNT];
	XMFLOAT4 mGridCenterPosW[GI_CASCADES_COUNT];
	XMFLOAT4 mInvertedGridCellSizes[GI_CASCADES_COUNT];
	XMFLOAT4 mGridCellSizes[GI_CASCADES_COUNT];
	//clear voxel grid/////////////////////////////////////////////////////////////////////////////
	ID3DX11EffectUnorderedAccessViewVariable* mfxClearGridBufferUAV;
	ID3DX11EffectScalarVariable* mfxClearOnCascadeIndex;
	//create voxel grid//////////////////////////////////////////////////////////////////////////// 
	ID3D11RenderTargetView* m64x64RT;
	D3D11_VIEWPORT m64x64RTViewport;

	ID3D11Buffer* mGridBuffer;
	ID3D11ShaderResourceView* mGridBufferSRV;
	ID3D11UnorderedAccessView* mGridBufferUAV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxGenerateGridBufferUAV;

	ID3DX11EffectVariable* mfxGridViewProjMatrices;
	ID3DX11EffectVariable* mfxGridCenterPosW;
	ID3DX11EffectVariable* mfxInvertedGridCellSizes;

	ID3DX11EffectScalarVariable* mfxMeshCullingFactor;
	//create VPLs//////////////////////////////////////////////////////////////////////////////////
	ID3DX11EffectMatrixVariable* mfxView;

	ID3D11Buffer* mSHBuffer;
	ID3D11ShaderResourceView* mSHBufferSRV;
	ID3D11UnorderedAccessView* mSHBufferUAV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxCreateVPLSHBufferUAV;

	ID3DX11EffectVariable* mfxGridCellSizes;
	ID3DX11EffectVariable* mfxGridCenterPos;
	ID3DX11EffectShaderResourceVariable* mfxCreateVPLGridBufferSRV;

	ID3DX11EffectVectorVariable* mfxEyePosW;

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

	ID3DX11EffectScalarVariable* mfxCreateVPLOnCascadeIndex;
	//propagate vpls///////////////////////////////////////////////////////////////////////////////
	ID3D11Buffer* mPingPongSHBuffer;
	ID3D11ShaderResourceView* mPingPongSHBufferSRV;
	ID3D11UnorderedAccessView* mPingPongSHBufferUAV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxOutputSHBufferUAV;
	ID3DX11EffectShaderResourceVariable* mfxInputSHBufferSRV;
	ID3DX11EffectShaderResourceVariable* mfxPropagateVPLGridBufferSRV;
	ID3DX11EffectScalarVariable* mfxPropagateOnCascadeIndex;

	UINT mPropogationsCount;
	//apply illumination///////////////////////////////////////////////////////////////////////////
	ID3DX11EffectScalarVariable* mfxGlobalIllumAttFactor;
	ID3DX11EffectShaderResourceVariable* mfxFinalSHBufferSRV;
	ID3DX11EffectShaderResourceVariable* mfxGridBufferROSRV;
	UINT mAttFactor;
private:
	//initialization///////////////////////////////////////////////////////////////////////////////
	void GlobalIllumination::BuildViews();
	void GlobalIllumination::BuildFX();
	void GlobalIllumination::CalculateCellSizes();
	void GlobalIllumination::BuildCamera();
	void GlobalIllumination::BuildViewport();
	//computation steps////////////////////////////////////////////////////////////////////////////
	void GlobalIllumination::ClearGrid();
	void GlobalIllumination::CreateVPLs(ShadowMapping* pSM,
									UINT dirLightsNum, DirectionalLight* dirLight,
									UINT spotLightsNum, SpotLight* spotLight,
									UINT pointLightsNum, PointLight* pointLight);
	void GlobalIllumination::PropagateVPLs();
public:
	GlobalIllumination::GlobalIllumination(ID3D11Device* device, ID3D11DeviceContext* dc, ID3DX11Effect* gengridfx, Camera& camera);
	
	void GlobalIllumination::Update(Camera& camera);
	void GlobalIllumination::Setup();
	void GlobalIllumination::SetFinalSHBuffer();
	void GlobalIllumination::SetPropogationsCount(UINT pc) { InterlockedExchange(reinterpret_cast<LONG*>(&mPropogationsCount), pc); };
	void GlobalIllumination::SetAttFactor(UINT af) { InterlockedExchange(reinterpret_cast<LONG*>(&mAttFactor), af); };
	UINT GlobalIllumination::GetPropogationsCount() const { return mPropogationsCount; };
	UINT GlobalIllumination::GetAttFactor() const { return mAttFactor; };
	void GlobalIllumination::Compute(ShadowMapping* pSM,
									UINT dirLightsNum, DirectionalLight* dirLight,
									UINT spotLightsNum, SpotLight* spotLight,
									UINT pointLightsNum, PointLight* pointLight);

	ID3D11RenderTargetView* GlobalIllumination::Get64x64RT() const { return m64x64RT; };
	D3D11_VIEWPORT GlobalIllumination::Get64x64RTViewport() const { return m64x64RTViewport; };
	ID3D11UnorderedAccessView* GlobalIllumination::GetGridBufferUAV() const { return mGridBufferUAV; };

	const XMFLOAT4* GlobalIllumination::GetGridCenterPosW(void) const { return mGridCenterPosW; };
	const XMFLOAT4* GlobalIllumination::GetInvertedGridCellSizes(void) const { return mInvertedGridCellSizes; };
	const XMFLOAT4* GlobalIllumination::GetGridCellSizes(void) const { return mGridCellSizes; };
};

#endif

