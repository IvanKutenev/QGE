#ifndef _GPU_COLLISION_H_
#define _GPU_COLLISION_H_

#define UPDATE_DISTANCE 500.0f

#define MAX_BODIES_COUNT 150
#define PPLL_LAYERS_COUNT 100

#define PARTICLES_GRID_CASCADES_COUNT 1
#define CASCADES_SPLIT_LAMBDA 0.5f
#define PARTICLES_GRID_RESOLUTION 100
#define CS_THREAD_GROUPS_NUMBER 10
#define PARTICLES_COUNT PARTICLES_GRID_RESOLUTION * PARTICLES_GRID_RESOLUTION * PARTICLES_GRID_RESOLUTION

#define INFINITY 1000000.0f

#include "../Bin/Common/d3dUtil.h"
#include "../Bin/GPU_Framework/Camera.h"
#include "../Bin/GPU_Framework/MeshManager.h"

struct BodiesListNode
{
	XMFLOAT4 PosWBodyIndex;
	INT NextIndex;
};

struct MacroParams
{
	FLOAT Mass;
	FLOAT InertionMomentum;
	XMFLOAT3 CMPosW;
	XMFLOAT3 Speed;
	XMFLOAT3 AngularSpeed;
	XMFLOAT4 Quaternion;
	
};

struct MicroParams // contains info about particles which approximately forms specific rigid body
{
	INT BodyIndex;
	XMFLOAT3 LinearMomentum;
	XMFLOAT3 AngularMomentum;
};

typedef std::pair<UINT, UINT> MeshInstanceIndex;
typedef UINT BodyIndex;
typedef UINT MeshIndex;
typedef UINT InstanceIndex;

//
//TODOs: 1) add terrain collision using its heightmap and sizing parameters
//		2) add technique for fair CM, mass and momentum of intertia calculation by compute shader
//		3) set body index array (for each instance) by mesh index

class GpuCollision
{
private:
	//for testing
	ID3DX11EffectScalarVariable* testBodiesCount;
	ID3DX11EffectVariable* testCenterPosW;
	ID3DX11EffectVariable* testCellSizes;
	ID3DX11EffectShaderResourceVariable* testParticlesGrid;
	//

	ID3D11Device* mDevice;
	ID3D11DeviceContext* mDC;
	MeshManager* pMeshManager;

	ID3DX11Effect* mBuildBodiesPPLLFX;
	ID3DX11Effect* mGridFX;
	ID3DX11Effect* mDetectCollisionsFX;
	ID3DX11Effect* mComputeMomentumFX;

	//render meshes to per-pixel linked lists
	ID3DX11EffectVariable* mfxBodyIndex;
	ID3DX11EffectUnorderedAccessViewVariable* mfxBodiesPPLLUAV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxHeadIndexBufferUAV;
	//populate voxel grid with meshes particles as it is described above
	ID3DX11EffectTechnique* mfxClearParticlesGridTech;
	ID3DX11EffectTechnique* mfxPopulateGridTech;
	ID3DX11EffectShaderResourceVariable* mfxBodiesPPLLSRV;
	ID3DX11EffectShaderResourceVariable* mfxHeadIndexBufferSRV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxPreIterationParticlesGridUAV;
	ID3DX11EffectVariable* mfxGridCenterPosW;
	ID3DX11EffectVariable* mfxGridCellSizes;
	ID3DX11EffectScalarVariable* mfxProcessingCascadeIndex;
	ID3DX11EffectScalarVariable* mfxBodiesCount;
	//detect collisions and compute per particle momentum
	ID3DX11EffectTechnique* mfxDetectCollisionsTech;
	ID3DX11EffectShaderResourceVariable* mfxPreIterationParticlesGridSRV;
	ID3DX11EffectShaderResourceVariable* mfxBodiesMacroParametersBufferSRV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxPostIterationParticlesGridUAV;
	ID3DX11EffectVariable* mfxDetectBodiesGridCenterPosW;
	ID3DX11EffectVariable* mfxDetectBodiesGridCellSizes;
	ID3DX11EffectScalarVariable* mfxDetectProcessingCascadeIndex;
	ID3DX11EffectScalarVariable* mfxDetectBodiesCount;
	
	//compute per body momentum
	ID3DX11EffectTechnique* mfxComputeMomentumTech;
	ID3DX11EffectVariable* mfxBodiesGridCenterPosW;
	ID3DX11EffectVariable* mfxBodiesGridCellSizes;
	ID3DX11EffectVariable* mfxAabbCornersPosW;
	ID3DX11EffectScalarVariable* mfxDeltaTime;
	ID3DX11EffectShaderResourceVariable* mfxPostIterationParticlesGridSRV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxBodiesMacroParametersBufferUAV;

	//RT to build ppll
	ID3D11RenderTargetView* mOffscreenRTV;
	D3D11_VIEWPORT mOffscreenRTViewport;
	UINT mCurrentPPLLCascadeIndex;
	//bodies list buffer
	ID3D11Buffer* mBodiesMacroParametersCPUBuffer; // sizeof(MacroParams) * BodiesNumber buffer
	ID3D11Buffer* mBodiesMacroParametersGPUBuffer;
	ID3D11ShaderResourceView* mBodiesMacroParametersBufferSRV;
	ID3D11UnorderedAccessView* mBodiesMacroParametersBufferUAV;
	//ppll
	ID3D11Buffer* mBodiesPPLLs[PARTICLES_GRID_CASCADES_COUNT];
	ID3D11Texture2D* mHeadIndexBuffers[PARTICLES_GRID_CASCADES_COUNT];
	ID3D11UnorderedAccessView* mBodiesPPLLsUAVs[PARTICLES_GRID_CASCADES_COUNT];
	ID3D11UnorderedAccessView* mHeadIndexBuffersUAVs[PARTICLES_GRID_CASCADES_COUNT];
	ID3D11ShaderResourceView* mBodiesPPLLsSRVs[PARTICLES_GRID_CASCADES_COUNT];
	ID3D11ShaderResourceView* mHeadIndexBuffersSRVs[PARTICLES_GRID_CASCADES_COUNT];
	//grid
	ID3D11Buffer* mPreIterationParticlesGrid; //cascaded particles/voxel grid which strores info about meshes as particles
	ID3D11ShaderResourceView* mPreIterationParticlesGridSRV;
	ID3D11UnorderedAccessView* mPreIterationParticlesGridUAV;
	ID3D11Buffer* mPostIterationParticlesGrid;
	ID3D11Buffer* mPostIterationParticlesGridCopy;
	ID3D11ShaderResourceView* mPostIterationParticlesGridSRV;
	ID3D11UnorderedAccessView* mPostIterationParticlesGridUAV;
	Camera mCamera;
	Camera mCascadeCamera[PARTICLES_GRID_CASCADES_COUNT];
	XMFLOAT4 mGridCenterPosW[PARTICLES_GRID_CASCADES_COUNT];
	XMFLOAT4 mGridCellSizes[PARTICLES_GRID_CASCADES_COUNT];

	//parameters
	MacroParams mBodiesMacroParams[MAX_BODIES_COUNT];
	//indexing
	UINT mBodiesCount;
	std::map<MeshIndex, std::vector<InstanceIndex>>mMeshesInstancesIndices;
	std::map<MeshInstanceIndex, BodyIndex>mMeshesBodiesIndices; // (Mesh Index, Instance Index) -> Body Index
	std::map<BodyIndex, std::vector<MeshInstanceIndex>>mBodiesMeshesIndices; // Body Index -> (Mesh Index, Instance Index)
private:
	//gpu side
	void GpuCollision::BuildGrid(void);
	void GpuCollision::DetectCollisions(void);
	//cpu side
	void GpuCollision::UpdateBodiesMacroParams(float dt); // gpu -> cpu
	void GpuCollision::SetBodiesMacroParams(MacroParams *macroParams); //cpu -> gpu
	void GpuCollision::UpdateMeshesInstancesMatrices(FLOAT dt);
	//buffers creation
	void GpuCollision::BuildBuffers(void);
	void GpuCollision::BuildFX(void);
	void GpuCollision::CalculateCellSizes();
	void GpuCollision::BuildCamera();
	void GpuCollision::BuildViewport();
public:
	//for testing
	void GpuCollision::SetTestParams(void);
	//
	GpuCollision::GpuCollision(ID3D11Device* device, ID3D11DeviceContext* dc, ID3DX11Effect* buildPPLLFX, MeshManager*);
	void GpuCollision::InitializeBodiesList(std::map<BodyIndex, std::vector<MeshInstanceIndex>>& BodiesMeshesIndices);
	void GpuCollision::InitializeBodiesParams(MacroParams* bodiesParams);

	void GpuCollision::Update(Camera& cam);
	//needed for initializing bodies PPLL
	void GpuCollision::SetCascadeIndex(UINT idx) { mCurrentPPLLCascadeIndex = idx; };
	void GpuCollision::SetPerIterationData(void);
	void GpuCollision::SetPerMeshData(MeshIndex meshIdx);
	void GpuCollision::Setup(void);
	std::vector<BodyIndex> GpuCollision::GetBodiesIndicesByMeshIndex(MeshIndex meshIndex);
	//compute momentum and update matrices
	void GpuCollision::Compute(FLOAT dt);
	std::vector<MeshInstanceIndex> GpuCollision::GetMeshesIndicesListByBodyIndex(BodyIndex bodyIndex) { return mBodiesMeshesIndices[bodyIndex]; };
	ID3D11UnorderedAccessView** GpuCollision::GetBodiesPPLLsUAVs(void) { return mBodiesPPLLsUAVs; };
	ID3D11UnorderedAccessView** GpuCollision::GetHeadIndexBuffersUAVs(void) { return mHeadIndexBuffersUAVs; };
	ID3D11RenderTargetView** GpuCollision::GetOffscreenRTV(void) { return &mOffscreenRTV; };
	D3D11_VIEWPORT GpuCollision::GetOffscreenRTViewport(void) { return mOffscreenRTViewport; };
	Camera GpuCollision::GetCascadeCamera(UINT cascadeIdx) { return mCascadeCamera[cascadeIdx]; };
	bool GpuCollision::Contains(MeshInstanceIndex meshInstanceIdx) { return mMeshesBodiesIndices.find(meshInstanceIdx) != mMeshesBodiesIndices.end(); };
};

#endif