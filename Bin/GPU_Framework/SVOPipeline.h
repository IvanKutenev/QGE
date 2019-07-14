#ifndef _SVO_PIPELINE_H_
#define _SVO_PIPELINE_H_

#include "../Common/d3dUtil.h"
#include "../Bin/GPU_Framework/Camera.h"

//flags
#define VOXEL_EMPTY_FLAG 0
#define VOXEL_FILLED_FLAG 1
#define NO_CHILD 0 
#define NO_PARENT -1
#define NO_GEO_FOUND 0.0
#define OUT_OF_NODE_BOUNDS -1
//magic constants
#define FLOAT_EPSILON 0.00000001
//SVO parameters
#define SVO_LEVELS_COUNT 9
#define SVO_VOXEL_BLOCK_SIDE_SZ 2
#define SVO_BUILD_RT_SZ (2 << (SVO_LEVELS_COUNT - 1)) * SVO_VOXEL_BLOCK_SIDE_SZ
#define SVO_VOXEL_BLOCK_SZ SVO_VOXEL_BLOCK_SIDE_SZ * SVO_VOXEL_BLOCK_SIDE_SZ * SVO_VOXEL_BLOCK_SIDE_SZ
#define SVO_ROOT_NODE_SZ 500.0f
#define SVO_MIN_AVG_LEVEL_IDX 5
//configuration parameters
#define RAY_CASTER_NUM_THREADS 8.0f
#define RAY_CASTER_MAX_LEVEL_IDX SVO_LEVELS_COUNT - 1
#define RESET_SVO_NUM_THREADS 512.0f
#define GEN_MIPS_NUM_THREADS 512.0f
#define REFLECTION_MAP_MASK_SAMPLE_LEVEL 5 // must be equal or greater thean 1
#define ENV_MAP_MIP_LEVELS_COUNT 10 
//buffers parameters
#define MAX_NODES_COUNT 1.0 / 7.0 * (pow(8, SVO_LEVELS_COUNT + 1) - 1)
#define MAX_VOXELS_COUNT MAX_NODES_COUNT * SVO_VOXEL_BLOCK_SZ
#define MAX_FILL_FRACTION 10.0f
#define NODES_COUNT round(MAX_NODES_COUNT / MAX_FILL_FRACTION)
#define VOXELS_COUNT round(MAX_VOXELS_COUNT / MAX_FILL_FRACTION)
//ray tracing parameters
#define CONE_THETA 0.06f
#define MAX_STEP_MULTIPLIER 1.0f
#define MIN_STEP_MULTIPLIER 0.5f
#define WEIGHT_MULTIPLIER 0.25f
#define ALPHA_CLAMP_VALUE 0.5f
#define ALPHA_MULTIPLIER 2.0f
#define MAX_ITER_COUNT 128

//TODO:
// --write voxels if and only if they are bigger than pix by pix square after screen projection!

struct SvoDebugDrawVsIn
{
	XMFLOAT3 PosW;
	INT svoLevelIdx;
};

struct SvoNode
{
	XMFLOAT3 PosW;
	INT svoLevelIdx;
	INT parentNodeIdx;
	INT childNodesIds[8];
	INT voxelBlockStartIdx;
};

struct SvoVoxel
{
	UINT colorMask;
};

class SvoPipeline
{
private:
	ID3D11Device* md3dDevice;
	ID3D11DeviceContext* md3dImmediateContext;

	///<debug>
	ID3D11Buffer* mNodesPoolCopy;

	UINT mFilledNodesCount;
	ID3D11Buffer* mNodesPoolDebugDraw;
	ID3D11InputLayout* mSvoDebugDrawInputLayout;
	ID3DX11EffectTechnique* mSvoDebugDrawTech;
	ID3DX11EffectMatrixVariable* mfxViewProj;
	///</debug>

	ID3D11Buffer* mNodesPool;
	ID3D11ShaderResourceView* mNodesPoolSRV;
	ID3D11UnorderedAccessView* mNodesPoolUAV;
	ID3D11Buffer* mVoxelsPool;
	ID3D11ShaderResourceView* mVoxelsPoolSRV;
	ID3D11UnorderedAccessView* mVoxelsPoolUAV;
	ID3D11Buffer* mVoxelsFlags;
	ID3D11ShaderResourceView* mVoxelsFlagsSRV;
	ID3D11UnorderedAccessView* mVoxelsFlagsUAV;
	
	ID3D11Texture2D* mRayCastedOutput;
	ID3D11Texture2D* mRayCastedOutputCopy;

	ID3D11ShaderResourceView* mEnvironmentMapSRV;

	ID3D11ShaderResourceView* mRayCastedOutputSRV;
	ID3D11ShaderResourceView* mRayCastedOutputCopySRV;
	ID3D11ShaderResourceView* mReflectionMapMaskSRV;
	ID3D11ShaderResourceView* mReflectionMapSRV;

	ID3D11UnorderedAccessView* mRayCastedOutputUAV;
	ID3D11UnorderedAccessView* mReflectionMapMaskUAV;
	ID3D11UnorderedAccessView* mReflectionMapUAV;
	
	ID3D11ShaderResourceView* mNormalDepthMapSRV;

	ID3D11RenderTargetView* m64x64RT;
	D3D11_VIEWPORT m64x64RTViewport;

	XMMATRIX mSVOViewProjMatrices[6];
	XMFLOAT3 mSvoRootNodeCenterPosW;

	FLOAT mClientW;
	FLOAT mClientH;
	
	Camera mCamera;

	//SVO building
	ID3DX11Effect* mBuildSvoFX;

	ID3DX11EffectScalarVariable* build_mfxSvoMinAveragingLevelIdx;

	ID3DX11EffectVariable* build_mfxSVOViewProjMatrices;
	
	ID3DX11EffectVectorVariable* build_mfxSvoRootNodeCenterPosW;

	ID3DX11EffectShaderResourceVariable* build_mfxVoxelsPoolSRV;
	ID3DX11EffectShaderResourceVariable* build_mfxNodesPoolSRV;

	ID3DX11EffectUnorderedAccessViewVariable* build_mfxNodesPoolUAV;
	ID3DX11EffectUnorderedAccessViewVariable* build_mfxVoxelsPoolUAV;
	ID3DX11EffectUnorderedAccessViewVariable* build_mfxVoxelsFlagsUAV;
	
	//SVO ray casting
	ID3DX11Effect* mSvoTraceRaysFX;
	ID3DX11EffectScalarVariable* trace_mfxSvoMinAveragingLevelIdx;
	ID3DX11EffectScalarVariable* trace_mfxSvoRayCastLevelIdx;
	ID3DX11EffectScalarVariable* trace_mfxClientW;
	ID3DX11EffectScalarVariable* trace_mfxClientH;
	ID3DX11EffectScalarVariable* trace_mfxReflectionMapMaskSampleLevel;
	ID3DX11EffectScalarVariable* trace_mfxConeTheta;
	ID3DX11EffectScalarVariable* trace_mfxAlphaClampValue;
	ID3DX11EffectScalarVariable* trace_mfxMinStepMultiplier;
	ID3DX11EffectScalarVariable* trace_mfxMaxStepMultiplier;
	ID3DX11EffectScalarVariable* trace_mfxWeightMultiplier;
	ID3DX11EffectScalarVariable* trace_mfxAlphaMultiplier;
	ID3DX11EffectScalarVariable* trace_mfxEnvironmentMapMipsCount;
	ID3DX11EffectScalarVariable* trace_mfxMaxIterCount;

	ID3DX11EffectVectorVariable* trace_mfxEyePosW;
	ID3DX11EffectMatrixVariable* trace_mfxInvProj;
	ID3DX11EffectMatrixVariable* trace_mfxInvView;
	ID3DX11EffectVectorVariable* trace_mfxSvoRootNodeCenterPosW;
	
	ID3DX11EffectUnorderedAccessViewVariable* trace_mfxNodesPoolUAV;
	ID3DX11EffectUnorderedAccessViewVariable* trace_mfxRayCastedOutputUAV;
	ID3DX11EffectUnorderedAccessViewVariable* trace_mfxReflectionMapUAV;
	ID3DX11EffectUnorderedAccessViewVariable* trace_mfxReflectionMapMaskUAV;
	
	ID3DX11EffectShaderResourceVariable* trace_mfxNodesPoolSRV;
	ID3DX11EffectShaderResourceVariable* trace_mfxVoxelsPoolSRV;
	ID3DX11EffectShaderResourceVariable* trace_mfxVoxelsFlagsSRV;
	ID3DX11EffectShaderResourceVariable* trace_mfxNormalDepthMapSRV;
	ID3DX11EffectShaderResourceVariable* trace_mfxReflectionMapSRV;
	ID3DX11EffectShaderResourceVariable* trace_mfxEnvironmentMapSRV;

	ID3DX11EffectTechnique* mResetRootNodeTech;
	ID3DX11EffectTechnique* mCastRaysTech;
	ID3DX11EffectTechnique* mCastReflectionRaysTech;
	ID3DX11EffectTechnique* mTraceReflectionConesTech;	
private:
	void SvoPipeline::BuildFX();
	void SvoPipeline::BuildBuffers();
	void SvoPipeline::BuildViews();
	void SvoPipeline::BuildViewport();
	void SvoPipeline::BuildInputLayout();
	void SvoPipeline::BuildCamera();
	void SvoPipeline::ResetNodesPool();
	void SvoPipeline::ResetVoxelsPool();
	void SvoPipeline::CopyTree();
public:
	//constructor
	SvoPipeline::SvoPipeline(ID3D11Device* device, ID3D11DeviceContext* dc, ID3DX11Effect* buildSvoFx,
		ID3D11ShaderResourceView* normalDepthSRV, ID3D11ShaderResourceView* environmentMapSRV, FLOAT clientW, FLOAT clientH);
	//cleans octree structure
	void SvoPipeline::Reset();
	//sets up views needed for octree building
	void SvoPipeline::Setup();
	//ray caster for octree traverse debugging
	void SvoPipeline::SvoCastRays();
	//generates single bounce reflection map 
	void SvoPipeline::SvoCastReflectionRays();
	//traces specular reflections cones, utilizes output of SvoCastReflectionRays()
	void SvoPipeline::SvoTraceReflectionCones();
	//resizes g-buffer
	void SvoPipeline::OnSize(ID3D11ShaderResourceView* normalDepthSRV, FLOAT clientW, FLOAT clientH);
	//updates octree build cameras using player's camera
	void SvoPipeline::Update(Camera camera);
	//visualizes octree nodes for octree build debugging
	void SvoPipeline::ViewTree(ID3D11RenderTargetView* rtv, ID3D11DepthStencilView* dsv, D3D11_VIEWPORT vp);
	//sets parameters for octree nodes visualization
	void SvoPipeline::SetTestParams();
	//get methods
	ID3D11RenderTargetView* SvoPipeline::Get64x64RT() const { return m64x64RT; };
	D3D11_VIEWPORT SvoPipeline::Get64x64RTViewport() const { return m64x64RTViewport; };
	ID3D11ShaderResourceView* SvoPipeline::GetRayCasterOutput() { return mRayCastedOutputSRV; };
	ID3D11ShaderResourceView* SvoPipeline::GetReflectionMapSRV() { return mReflectionMapSRV; };
	ID3D11ShaderResourceView* SvoPipeline::GetReflectionMapMaskSRV() { return mReflectionMapMaskSRV; };
	ID3D11UnorderedAccessView* SvoPipeline::GetNodesPoolUAV() { return mNodesPoolUAV; };
	ID3D11UnorderedAccessView* SvoPipeline::GetVoxelsPoolUAV() { return mVoxelsPoolUAV; };
	ID3D11UnorderedAccessView* SvoPipeline::GetVoxelsFlagsUAV() { return mVoxelsFlagsUAV; };
};
#endif