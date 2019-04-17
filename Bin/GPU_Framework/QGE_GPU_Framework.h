#ifndef _QGE_GPU_FRAMEWORK_
#define _QGE_GPU_FRAMEWORK_
#include "../Bin/Core/QGE_Core.h"
#include "../Bin/GUI/GUI.h"
#include "../Common/d3dx11Effect.h"
#include "../Common/MathHelper.h"
#include "../Common/GeometryGenerator.h"
#include "RenderStates.h"
#include "BlurFilter.h"
#include "ShadowMapping.h"
#include "Ssao.h"
#include "Materials.h"
#include "MeshManager.h"
#include "SPH.h"
#include "SSLR.h"
#include "Terrain.h"
#include "PPLL.h"
#include "OIT.h"
#include "GlobalIllumination.h"
#include "VolumetricEffect.h"
#include "PlanarWaterSurface.h"
#include "GPUCollision.h"
#include "ParticleSystem.h"
#include "EnvironmentMap.h"
#include "SVOPipeline.h"
#include "../Bin/CPU_Framework/GameLogic.h"

using namespace Materials;

class QGE_GPU_Framework : public QGE_Core
{
public:
	QGE_GPU_Framework(HINSTANCE hInstance);
	~QGE_GPU_Framework();

	bool Init();
	void UpdateScene(float dt);
	void DrawScene();
	void DrawGUI();

	void OnResize();
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
private:
	//Basic
	void BuildFX();

	void Draw(
		Camera& camera, int MeshNotDrawIndex,
		ID3D11RenderTargetView** RTVs, UINT RTVsCount,
		ID3D11UnorderedAccessView** UAVs, UINT UAVsCount, UINT inittialCount,
		ID3D11DepthStencilView* depthStencilView, DrawShadowType shadowType,
		int LightIndex, DrawPassType dpt
		);
	void Draw(
		ID3D11RenderTargetView** renderTargets,
		ID3D11DepthStencilView* depthStencilView
		);
	void DrawMesh(
		Camera& camera,
		DrawPassType dst,
		int MeshIndex
		);
	bool PerformFrustumCulling(
		Camera& camera,
		int MeshIndex,
		int MeshInstanceIndex = 0
		);
	void SelectMeshDrawTech(
		DrawShadowType shadowType,
		DrawPassType dst,
		int MeshIndex
		);
	bool SelectTransparentMeshDrawTech(
		DrawShadowType shadowType,
		DrawPassType dst,
		int MeshIndex
	);
	void SetPerFrameConstants(
		Camera& camera,
		DrawShadowType shadowType,
		int LightIndex,
		DrawPassType dpt
		);

	//Cube mapping
	void BuildCubeFaceCamera(float x, float y, float z);
	
	//Texture debugger
	void DrawDebuggingTexture(
		ID3D11RenderTargetView** renderTargets,
		ID3D11DepthStencilView* depthStencilView,
		ID3D11ShaderResourceView* debuggingTex
		);
	void BuildTextureDebuggerBuffers();
	//D2D GUI
	void D2DGUIDraw(
		ID3D11RenderTargetView** renderTargets,
		ID3D11DepthStencilView* depthStencilView
		);
	void D2DGUIBuildBuffers();

	void RenderD2DGUIdataToTexture();
	//Particle system
	void ParticleSystemDrawRefraction(
		Camera &cam
		);
	//Planar water surface
	void WaterDrawReflection(Camera &cam);
	void WaterDrawRefraction(Camera &cam);
	void WaterDrawReflectionAndRefraction(Camera &cam);
	//SPH
	void SPHFluidDraw(
		Camera& cam,
		DrawPassType dpt,
		ID3D11RenderTargetView** renderTargets,
		ID3D11DepthStencilView* depthStencilView,
		UINT meshNotDrawIdx
		);
	void SPHFluidDrawRefraction(Camera &cam);
	//Offscreen RTs
	void BuildOffscreenViews();
private:
	///////////////////////Draw modes////////////////////////////////////////////////////////////////////
	ID3DX11EffectScalarVariable *UseBlending;
	ID3DX11EffectScalarVariable *UseBlackClipping;
	ID3DX11EffectScalarVariable *UseAlphaClipping;
	ID3DX11EffectScalarVariable *UseNormalMapping;
	ID3DX11EffectScalarVariable *UseDisplacementMapping;
	ID3DX11EffectScalarVariable *mfxReflectionType;
	ID3DX11EffectScalarVariable *mfxRefractionType;
	ID3DX11EffectScalarVariable *mfxDrawPassType;

	bool mUseBlackClippingAtNonBlend;
	bool mUseAlphaClippingAtNonBlend;
	bool mUseBlackClippingAtAddBlend;
	bool mUseAlphaClippingAtAddBlend;
	bool mUseBlackClippingAtBlend;
	bool mUseAlphaClippingAtBlend;
	////////////////////////////////Techniques///////////////////////////////////////////////////////////
	ID3DX11Effect* mFX;																			   
	ID3DX11EffectTechnique* mBasicTech;
	ID3DX11EffectTechnique* mTessTech;
	ID3DX11EffectTechnique* mDrawScreenQuadTech;

	ID3DX11EffectTechnique* mBuildShadowMapBasicTech;
	ID3DX11EffectTechnique* mBuildShadowMapTessTech;

	ID3DX11EffectTechnique* mBuildNormalDepthMapBasicTech;
	ID3DX11EffectTechnique* mBuildNormalDepthMapTessTech;

	ID3DX11EffectTechnique* mBuildAmbientMapBasicTech;
	ID3DX11EffectTechnique* mBuildDiffuseMapBasicTech;

	ID3DX11EffectTechnique* mBuildAmbientMapTessTech;
	ID3DX11EffectTechnique* mBuildDiffuseMapTessTech;

	ID3DX11EffectTechnique* mBuildDepthMapBasicTech;
	ID3DX11EffectTechnique* mBuildDepthMapTessTech;

	ID3DX11EffectTechnique* mBuildWSPositionMapBasicTech;
	ID3DX11EffectTechnique* mBuildWSPositionMapTessTech;

	ID3DX11EffectTechnique* mBuildSpecularMapBasicTech;
	ID3DX11EffectTechnique* mBuildSpecularMapTessTech;

	ID3DX11EffectTechnique* mBuildPPLLBasicTech;
	ID3DX11EffectTechnique* mBuildPPLLTessTech;

	ID3DX11EffectTechnique* mBuildBodiesPPLLBasicTech;
	ID3DX11EffectTechnique* mBuildBodiesPPLLTessTech;

	ID3DX11EffectTechnique* mGenerateGridBasicTech;
	ID3DX11EffectTechnique* mGenerateGridTessTech;

	ID3DX11EffectTechnique* mBuildSvoBasicTech;
	ID3DX11EffectTechnique* mBuildSvoTessTech;
	//////////////////////////Matrices///////////////////////////////////////////////////////////////////
	ID3DX11EffectMatrixVariable* mfxWorldViewProj;
	ID3DX11EffectMatrixVariable* mfxWorld;
	ID3DX11EffectMatrixVariable* mfxView;
	ID3DX11EffectMatrixVariable* mfxProj;
	ID3DX11EffectMatrixVariable* mfxInvWorld;
	ID3DX11EffectMatrixVariable* mfxWorldInvTranspose;
	ID3DX11EffectMatrixVariable* mfxViewProj;
	ID3DX11EffectVectorVariable* mfxEyePosW;													   
																								   
	XMFLOAT3 mEyePosW;																			   
	XMFLOAT4X4 mView;
	XMFLOAT4X4 mProj;

	XMMATRIX I;
	//////////////////////Lighting///////////////////////////////////////////////////////////////////////
	ID3DX11EffectVariable* mfxDirLight;															   
	ID3DX11EffectVariable* mfxPointLight;														   
	ID3DX11EffectVariable* mfxSpotLight;														   
	ID3DX11EffectVariable* mfxMaterial;															   
	ID3DX11EffectScalarVariable* mfxDirLightsNum;												   
	ID3DX11EffectScalarVariable* mfxSpotLightsNum;												   
	ID3DX11EffectScalarVariable* mfxPointLightsNum;												   
																								   
	DirectionalLight mDirLight[MAX_DIR_LIGHTS_NUM];									   
	SpotLight mSpotLight[MAX_SPOT_LIGHTS_NUM];											   
	PointLight mPointLight[MAX_POINT_LIGHTS_NUM];
																								   
	UINT mDirLightsNum;																			   
	UINT mSpotLightsNum;
	UINT mPointLightsNum;
	/////////////////////Texturing///////////////////////////////////////////////////////////////////////
	ID3DX11EffectShaderResourceVariable *TexDiffuseMapArray;
	ID3DX11EffectShaderResourceVariable *ExtTexDiffuseMapArray; // for different-resolution textures
	ID3DX11EffectVariable *TexType;
	ID3DX11EffectShaderResourceVariable *NormalMap;
	ID3DX11EffectShaderResourceVariable *ReflectMap;
	ID3DX11EffectScalarVariable *UseTexture;
	ID3DX11EffectScalarVariable *UseExtTextureArray;
	ID3DX11EffectVariable *TexTransform;
	ID3DX11EffectScalarVariable *TexCount;
	ID3DX11EffectScalarVariable *mfxMaterialType;
	/////////////////////////Camera control//////////////////////////////////////////////////////////////
	BOOL mWalkCamMode;
	POINT mLastMousePos;
	Camera mCamera;
	////////////////////////Time/////////////////////////////////////////////////////////////////////////
	ID3DX11EffectScalarVariable* mfxTime;
	FLOAT mTime;
	FLOAT mTimeSpeed;
	////////////////////////Displacement mapping/////////////////////////////////////////////////////////
	ID3DX11EffectScalarVariable* mfxHeightScale;
	FLOAT mHeightScale;
	////////////////////////Debug texture mode///////////////////////////////////////////////////////////
	ID3D11Buffer* mScreenQuadVB;
	ID3D11Buffer* mScreenQuadIB;

	BOOL mEnableDebugTextureMode;
	////////////////////Tesselation//////////////////////////////////////////////////////////////////////
	ID3DX11EffectScalarVariable* MaxTessFactor;
	ID3DX11EffectScalarVariable* MaxLODdist;
	ID3DX11EffectScalarVariable* MinLODdist;
	ID3DX11EffectScalarVariable* EnableDynamicLOD;
	ID3DX11EffectScalarVariable* ConstantLODTessFactor;

	FLOAT mMaxTessFactor;
	FLOAT mMaxLODdist;
	FLOAT mMinLODdist;
	FLOAT mConstantLODTessFactor;

	BOOL mEnableDynamicLOD;
	//////Offscreen RTs//////////////////////////////////////////////////////////////////////////////////
	ID3D11ShaderResourceView* mAmbientMapSRV;
	ID3D11RenderTargetView* mAmbientMapRTV; 
	ID3D11ShaderResourceView* mDiffuseMapSRV;
	ID3D11RenderTargetView* mDiffuseMapRTV;
	ID3D11ShaderResourceView* mSpecularMapSRV;
	ID3D11RenderTargetView* mSpecularMapRTV;
	ID3D11ShaderResourceView* mOffscreenSRV;
	ID3D11UnorderedAccessView* mOffscreenUAV;
	ID3D11RenderTargetView* mOffscreenRTV;
	ID3D11ShaderResourceView* mWSPositionMapSRV;
	ID3D11RenderTargetView* mWSPositionMapRTV;
	//////Cube mapping///////////////////////////////////////////////////////////////////////////////////
	Camera mCubeMapCamera[6];
	
	ID3DX11EffectShaderResourceVariable* mfxEnvMap;
	ID3DX11EffectVectorVariable* mfxObjectDynamicCubeMapWorld;
	ID3DX11EffectScalarVariable* mfxApproxSceneSz;
	
	FLOAT mApproxSceneSz;
	//////Enviroment mapping/////////////////////////////////////////////////////////////////////////////
	EnvironmentMap* pEnvMap;
	
	BOOL mEnvMapDraw;
	//////////////////////////////Meshes////////////////////////////////////////////////////////////////
	MeshManager *pMeshManager;
	ID3DX11EffectScalarVariable* mfxMeshType;
	////////////////////Terrain//////////////////////////////////////////////////////////////////////////
	Terrain* pTerrain;
	BOOL mTerrainDraw;
	////////////////Particle system//////////////////////////////////////////////////////////////////////	
	std::map<std::string, ParticleSystem*>ParticleSystems;
	
	BOOL mDrawParticleSystemRefraction;
	//////////////Planar reflective/refractive water/////////////////////////////////////////////////////
	PlanarWaterSurface* pPlanarWaterSurface;
	//////Shadows////////////////////////////////////////////////////////////////////////////////////////
	ShadowMapping* pSM;

	ID3DX11EffectScalarVariable* mfxShadowType;

	Camera dirLightCams[CASCADE_COUNT * MAX_DIR_LIGHTS_NUM];
	ID3DX11EffectShaderResourceVariable* mfxDirLightShadowMapArray;
	ID3DX11EffectVariable* mfxDirLightShadowTransformArray;
	ID3DX11EffectVariable* mfxDirLightSplitDistances;
	ID3DX11EffectScalarVariable* mfxDirLightIndex;

	Camera spotLightCams[MAX_SPOT_LIGHTS_NUM];
	ID3DX11EffectShaderResourceVariable* mfxSpotLightShadowMapArray;
	ID3DX11EffectVariable* mfxSpotLightShadowTransformArray;
	ID3DX11EffectScalarVariable* mfxSpotLightIndex;

	Camera pointLightCams[6 * MAX_POINT_LIGHTS_NUM];
	ID3DX11EffectShaderResourceVariable* mfxPointLightCubicShadowMapArray;
	ID3DX11EffectScalarVariable* mfxPointLightIndex;

	BOOL mEnableShadows;
	//////Frustum culling////////////////////////////////////////////////////////////////////////////////
	BOOL mEnableFrustumCulling;
	//////Bounding volumes///////////////////////////////////////////////////////////////////////////////
	ID3DX11EffectScalarVariable* mfxBoundingSphereRadius;
	//////DOF////////////////////////////////////////////////////////////////////////////////////////////

	//////Order-independent transparency/////////////////////////////////////////////////////////////////
	PPLL* pOIT_PPLL;
	OIT* pOIT;
	//////Bloom//////////////////////////////////////////////////////////////////////////////////////////

	//////Grass//////////////////////////////////////////////////////////////////////////////////////////

	//////Motion Blur////////////////////////////////////////////////////////////////////////////////////

	//////GPU-based physics//////////////////////////////////////////////////////////////////////////////
	SPH* pSPH;
	GpuCollision* pGpuCollision;
	GameLogic* pGameLogic;
	BOOL mEnablePhysics;
	//////Render states//////////////////////////////////////////////////////////////////////////////////
	RenderStates* pRenderStates;
	//////Planar refraction//////////////////////////////////////////////////////////////////////////////
	ID3DX11EffectShaderResourceVariable* mfxPlanarRefractionMap;
	//////SSLR///////////////////////////////////////////////////////////////////////////////////////////
	SSLR* pSSLR;
	ID3DX11EffectShaderResourceVariable* SSLRMap;
	BOOL mEnableSSLR;
	ID3DX11EffectScalarVariable* mfxEnableSSLR;
	//////SSAO///////////////////////////////////////////////////////////////////////////////////////////
	Ssao* pSsao;
	ID3DX11EffectShaderResourceVariable* SsaoMap;
	BOOL mEnableSsao;
	ID3DX11EffectScalarVariable* mfxEnableSsao;
	//////Global Illumination////////////////////////////////////////////////////////////////////////////
	BOOL mEnableGI;
	GlobalIllumination* pGI;
	ID3DX11EffectScalarVariable* mfxEnableGI;
	/////Volumetric effects//////////////////////////////////////////////////////////////////////////////
	BOOL mEnableVolFX;
	VolumetricEffect* pVolFX;
	/////SVO/////////////////////////////////////////////////////////////////////////////////////////////
	BOOL mEnableRT;
	ID3DX11EffectScalarVariable* mfxEnableRT;
	ID3DX11EffectShaderResourceVariable* SVORTMap;
	SvoPipeline* pSVO;
};

#endif