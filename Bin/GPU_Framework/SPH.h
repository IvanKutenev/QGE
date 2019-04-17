#include "d3dUtil.h"
#include "BlurFilter.h"
#include "RenderStates.h"

#ifndef _SPH_H_
#define _SPH_H_

#define NUM_THREADS 128
#define NUM_THREAD_GROUPS 1024
#define NUM_PARTICLES NUM_THREADS*NUM_THREAD_GROUPS
#define NUM_CELLS NUM_PARTICLES

#define RADIUS 0.003
#define RENDER_RADIUS 0.006

#define BITONIC_BLOCK_SIZE 1024
#define TRANSPOSE_BLOCK_SIZE 32
#define MATRIX_WIDTH BITONIC_BLOCK_SIZE
#define MATRIX_HEIGHT NUM_CELLS / BITONIC_BLOCK_SIZE

#define SPH_BLUR_COUNT 2
#define SPH_REFRACTION_MAP_SZ 2048

#define MC_GRID_SIDE_SZ 32
#define MC_GRID_BUILD_THREAD_GROUP_COUNT 8
#define MC_GRID_AREA_SZ MC_GRID_SIDE_SZ * MC_GRID_SIDE_SZ

#define MC_GRID_FILL_RATE 1.0f / 1.0f

#define MC_TRIS_BUF_SZ MC_GRID_FILL_RATE * MC_GRID_SIDE_SZ * MC_GRID_SIDE_SZ * MC_GRID_SIDE_SZ * 5
#define MC_VERTS_BUF_SZ MC_TRIS_BUF_SZ * 3

//particle vertex for computing
struct Particle
{
	XMFLOAT4 Pos;
	XMFLOAT4 Vel;
	XMFLOAT4 Acc;
};

//particle vertex for drawing
struct ParticleDraw
{
	XMFLOAT3 Pos;
	XMFLOAT2 Tex;
};

//marching cubes surface vertex
struct FluidVertex
{
	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
	XMFLOAT2 Flag;
	XMFLOAT3 Tangent;
};

struct FluidTriangle
{
	FluidVertex v[3];
};

class SPH
{
private:
	ID3D11Device* md3dDevice;
	ID3D11DeviceContext* md3dImmediateContext;
private:
	///////////////////computing////////////////////////////////////////
	ID3DX11Effect* mSPH_ComputeFX;
	ID3DX11Effect* mSPH_SortFX;

	ID3DX11EffectTechnique* mSPH_ComputeTech;
	ID3DX11EffectTechnique* mSPH_BuildGridTech;
	ID3DX11EffectTechnique* mSPH_ClearGridIndicesTech;
	ID3DX11EffectTechnique* mSPH_BuildGridIndicesTech;
	ID3DX11EffectTechnique* mSPH_BitonicSortTech;
	ID3DX11EffectTechnique* mSPH_MatrixTransposeTech;
	ID3DX11EffectTechnique* mSPH_RearrangeParticlesTech;
	ID3DX11EffectTechnique* mSPH_CalcDensityTech;
	ID3DX11EffectTechnique* mSPH_CalcForcesTech;
	ID3DX11EffectTechnique* mSPH_IntegrateTech;
	ID3DX11EffectTechnique* mSPH_CalcNormalTech;

	ID3D11Buffer* mSPH_ParticlesBuffer;
	ID3D11UnorderedAccessView* mSPH_ParticlesBufferUAV;
	ID3D11ShaderResourceView* mSPH_ParticlesBufferSRV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxSPH_ParticlesBufferUAV;
	ID3DX11EffectShaderResourceVariable* mfxSPH_ParticlesBufferSRV;

	ID3D11Buffer* mSPH_SortedParticlesBuffer;
	ID3D11UnorderedAccessView* mSPH_SortedParticlesBufferUAV;
	ID3D11ShaderResourceView* mSPH_SortedParticlesBufferSRV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxSPH_SortedParticlesBufferUAV;
	ID3DX11EffectShaderResourceVariable* mfxSPH_SortedParticlesBufferSRV;

	ID3D11Buffer* mSPH_GridBuffer;
	ID3D11UnorderedAccessView* mSPH_GridBufferUAV;
	ID3D11ShaderResourceView* mSPH_GridBufferSRV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxSPH_GridBufferUAV;
	ID3DX11EffectShaderResourceVariable* mfxSPH_GridBufferSRV;
	
	ID3D11Buffer* mSPH_GridIndicesBuffer;
	ID3D11UnorderedAccessView* mSPH_GridIndicesBufferUAV;
	ID3D11ShaderResourceView* mSPH_GridIndicesBufferSRV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxSPH_GridIndicesBufferUAV;
	ID3DX11EffectShaderResourceVariable* mfxSPH_GridIndicesBufferSRV;

	ID3D11Buffer* mSPH_TempBuffer;
	ID3D11UnorderedAccessView* mSPH_TempBufferUAV;
	ID3D11ShaderResourceView* mSPH_TempBufferSRV;
	ID3DX11EffectShaderResourceVariable* mfxSPH_SortInputBufferSRV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxSPH_SortDataBufferUAV;

	ID3D11Buffer* mSPH_ParticleDensityBuffer;
	ID3D11UnorderedAccessView* mSPH_ParticleDensityBufferUAV;
	ID3D11ShaderResourceView* mSPH_ParticleDensityBufferSRV;
	ID3DX11EffectShaderResourceVariable* mfxSPH_ParticleDensityBufferSRV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxSPH_ParticleDensityBufferUAV;

	ID3D11Buffer* mSPH_ParticleForcesBuffer;
	ID3D11UnorderedAccessView* mSPH_ParticleForcesBufferUAV;
	ID3D11ShaderResourceView* mSPH_ParticleForcesBufferSRV;
	ID3DX11EffectShaderResourceVariable* mfxSPH_ParticleForcesBufferSRV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxSPH_ParticleForcesBufferUAV;

	ID3D11Buffer* mSPH_ParticleNormalsBuffer;
	ID3D11UnorderedAccessView* mSPH_ParticleNormalsBufferUAV;
	ID3D11ShaderResourceView* mSPH_ParticleNormalsBufferSRV;
	ID3DX11EffectShaderResourceVariable* mfxSPH_ParticleNormalsBufferSRV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxSPH_ParticleNormalsBufferUAV;

	ID3DX11EffectScalarVariable* mfxLevel;
	ID3DX11EffectScalarVariable* mfxLevelMask;
	ID3DX11EffectScalarVariable* mfxWidth;
	ID3DX11EffectScalarVariable* mfxHeight;
	
	ID3DX11EffectScalarVariable* mfxDropFlag;

	FLOAT mMass;
	ID3DX11EffectScalarVariable* mfxMass;
	FLOAT mRadius;
	ID3DX11EffectScalarVariable* mfxRadius;
	FLOAT mSmoothlen;
	ID3DX11EffectScalarVariable* mfxSmoothlen;
	FLOAT mPressureStiffness;
	ID3DX11EffectScalarVariable* mfxPressureStiffness;
	FLOAT mRestDensity;
	ID3DX11EffectScalarVariable* mfxRestDensity;
	FLOAT mViscosity;
	ID3DX11EffectScalarVariable* mfxViscosity;
	FLOAT mObstacleStiffness;
	ID3DX11EffectScalarVariable* mfxObstacleStiffness;
	FLOAT mObstacleFriction;
	ID3DX11EffectScalarVariable* mfxObstacleFriction;
	FLOAT mDeltaTime;
	ID3DX11EffectScalarVariable* mfxDeltaTime;
	FLOAT mDensityCoef;
	ID3DX11EffectScalarVariable* mfxDensityCoef;
	FLOAT mGradPressureCoef;
	ID3DX11EffectScalarVariable* mfxGradPressureCoef;
	FLOAT mLapViscosityCoef;
	ID3DX11EffectScalarVariable* mfxLapViscosityCoef;
	FLOAT mSurfaceTensionCoef;
	ID3DX11EffectScalarVariable* mfxSurfaceTensionCoef;

	BOOL mDropFlag;
	RenderStates* pRenderStates;
private:
	///////////////////////marching cubes/////////////////////////////
	ID3D11Buffer* mFluidSurface;
	ID3D11Buffer* mFluidSurfaceCopy;
	ID3D11Buffer* mFluidSurfaceDrawable;
	UINT mVertsCount;

	ID3D11UnorderedAccessView* mFluidSurfaceUAV;
	
	XMFLOAT3 mGridOrigin;
	FLOAT mGridCellSize;
	FLOAT mDensityIsoValue;

	ID3DX11Effect* mSPH_GenerateSurfaceFX;
	ID3DX11EffectTechnique* mSPH_ClearSurfaceBufferTech;
	ID3DX11EffectTechnique* mSPH_GenerateSurfaceTech;
	ID3DX11EffectUnorderedAccessViewVariable* mfxSPH_FluidSurafceUAV;
	ID3DX11EffectShaderResourceVariable* mfxSPH_GridIndicesSRV;
	ID3DX11EffectShaderResourceVariable* mfxSPH_ParticlesDensitySRV;
	ID3DX11EffectShaderResourceVariable* mfxSPH_ParticlesNormalsSRV;
	ID3DX11EffectShaderResourceVariable* mfxSPH_ParticlesSRV;
	ID3DX11EffectVectorVariable* mfxSPH_GridOrigin;
	ID3DX11EffectScalarVariable* mfxSPH_GridCellSize;
	ID3DX11EffectScalarVariable* mfxSPH_DensityIsoValue;
	ID3DX11EffectScalarVariable* mfxSPH_SmoothingLength;
private:
	////////////////////////rendering///////////////////////////////////
	D3D11_VIEWPORT mScreenViewport;//
	ID3D11Buffer* mScreenQuadVB;   // everything rendered in screen-space quad
	ID3D11Buffer* mScreenQuadIB;   //

	ID3D11InputLayout* mSPH_ParticlesInputLayout;
	ID3D11InputLayout* mSPH_DrawParticlesInputLayout;

	ID3DX11Effect* mSPH_DrawFX;

	ID3DX11EffectTechnique* mSPH_BuildPosWMapTech;
	ID3DX11EffectTechnique* mSPH_BuildNormalWMapTech;
	ID3DX11EffectTechnique* mSPH_DrawSPHFluidTech;

	ID3DX11EffectMatrixVariable* mfxWorldViewProj;
	ID3DX11EffectMatrixVariable* mfxWorld;
	ID3DX11EffectMatrixVariable* mfxView;
	ID3DX11EffectMatrixVariable* mfxProj;
	
	FLOAT mRenderRadius;
	ID3DX11EffectScalarVariable* mfxRenderRadius;
	
	FLOAT mScale;
	ID3DX11EffectScalarVariable* mfxScale;
	
	XMMATRIX mScaleMatrix;

	BlurFilter* pBlurFilter;//used for smoothing depth

	//particle tex-space normals texture views
	ID3D11ShaderResourceView* mSPH_NormalMapSRV;
	ID3DX11EffectShaderResourceVariable* mfxSPH_NormalMap;
	//world space position texture views
	ID3D11ShaderResourceView* mSPH_PosWMapSRV;
	ID3D11RenderTargetView* mSPH_PosWMapRTV;
	ID3D11UnorderedAccessView* mSPH_PosWMapUAV;
	ID3DX11EffectShaderResourceVariable* mfxSPH_PosWMap;
	//smoothed world space normals texture views
	ID3D11ShaderResourceView* mSPH_NormalWMapSRV;
	ID3D11RenderTargetView* mSPH_NormalWMapRTV;
	ID3DX11EffectShaderResourceVariable* mfxSPH_NormalWMap;
	//draw call final result texture views
	std::map<DrawSceneType, D3D11_VIEWPORT>mSPH_RTViewports;
	//shadow mapping
	ID3D11ShaderResourceView* mSPH_ShadowMapSRV;
	ID3D11RenderTargetView* mSPH_ShadowMapRTV;
	ID3D11UnorderedAccessView* mSPH_ShadowMapUAV;
	D3D11_VIEWPORT mSPH_ShadowMapVP;
	BlurFilter* mSPH_ShadowMapBlurFilter;
	//planar refraction
	ID3D11ShaderResourceView* mSPH_PlanarRefractionMapSRV;
	ID3D11RenderTargetView* mSPH_PlanarRefractionMapRTV;
	ID3D11DepthStencilView* mSPH_PlanarRefractionMapDSV;
	D3D11_VIEWPORT mSPH_PlanarRefractionMapVP;
private:
	//computing
	void SPH::SPHSetup();
	void SPH::SPHBuildGrid();
	void SPH::SPHClearGridIndices();
	void SPH::SPHBuildGridIndices();
	void SPH::SPHSort();
	void SPH::SPHRearrangeParticles();
	void SPH::SPHCalcDensity();
	void SPH::SPHCalcForces();
	void SPH::SPHIntegrate();
	void SPH::SPHCalcNormals();
private:
	//marching cubes
	void SPH::SPHClearSurfaceBuffer();
	void SPH::SPHGenerateSurface();
private:
	//rendering
	void SPH::SPHBuildPosWMap(XMMATRIX wvp, XMMATRIX world, XMMATRIX view, XMMATRIX proj, ID3D11DepthStencilView* depthStencilView,
		ID3D11RenderTargetView* renderTargetView, D3D11_VIEWPORT);
	void SPH::SPHBlurPosWMap();
	void SPH::SPHBlurSMDataMap();
	void SPH::SPHBuildNormalWMap(XMMATRIX proj);
	void SPH::SPHDrawFluid(XMMATRIX view, XMMATRIX proj, ID3D11RenderTargetView** renderTargetView, D3D11_VIEWPORT,
		ID3D11ShaderResourceView* posWMapSRV);
private:
	//initialization
	void SPH::SPHInit(ID3D11Device* device, ID3D11DeviceContext* dc, ID3DX11Effect* fx,
		ID3DX11EffectMatrixVariable* wvp, ID3DX11EffectMatrixVariable* world,
		ID3DX11EffectMatrixVariable* view, ID3DX11EffectMatrixVariable* proj,
		std::map<DrawSceneType, XMFLOAT2>&RTsSizes, FLOAT ClientH, FLOAT ClientW);
private:
	//building buffers and views
	void SPH::SPHBuildFX();
	void SPH::SPHBuildDrawBuffers();
	void SPH::SPHBuildOffscreenTextureViews(FLOAT ClientH, FLOAT ClientW);
	void SPH::SPHBuildVertexInputLayouts();
	void SPH::SPHCreateBuffersAndViews();
public:
	SPH::SPH(ID3D11Device* device, ID3D11DeviceContext* dc, ID3DX11Effect* fx,
		ID3DX11EffectMatrixVariable* wvp, ID3DX11EffectMatrixVariable* world,
		ID3DX11EffectMatrixVariable* view, ID3DX11EffectMatrixVariable* proj,
		std::map<DrawSceneType, XMFLOAT2>&RTsSizes, FLOAT ClientH, FLOAT ClientW);
	//drop a droplet
	void SPH::Drop() { mDropFlag = true; };
	//main compute function
	void SPH::Compute();
	//main draw function
	void SPH::Draw(XMMATRIX wvp, XMMATRIX world, XMMATRIX view, XMMATRIX proj, ID3D11DepthStencilView* depthStencilView,
		ID3D11RenderTargetView** renderTargetView, DrawPassType dpt);
	//resizing offscreen RTs if window was resized
	void SPH::OnSize(FLOAT ClientH, FLOAT ClientW);
	//get/set methods
	ID3D11RenderTargetView* SPH::GetPlanarRefractionMapRTV() const { return mSPH_PlanarRefractionMapRTV; };
	ID3D11DepthStencilView* SPH::GetPlanarRefractionMapDSV() const { return mSPH_PlanarRefractionMapDSV; };
	ID3D11ShaderResourceView* SPH::GetPlanarRefractionMapSRV() const { return mSPH_PlanarRefractionMapSRV; };
	D3D11_VIEWPORT SPH::GetPlanarRefractionMapVP() const { return mSPH_PlanarRefractionMapVP; };
	ID3D11Buffer** SPH::GetIsoSurfaceVB() { return &mFluidSurfaceDrawable; };
	UINT SPH::GetIsoSurfaceVertsCount() { return mVertsCount; };
};

#endif
