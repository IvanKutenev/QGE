#ifndef _MESH_MANAGER
#define _MESH_MANAGER

#include "../Common/GeometryGenerator.h"
#include "FBXLoader.h"
#include "Materials.h"

#define MAX_TEXTURE_COUNT 10
#define MAX_INSTANCES_COUNT 200

#define MESH_CUBE_MAP_SZ 2048
#define MESH_PLANAR_REFRACTION_MAP_SZ 2048

static XMMATRIX I = XMMatrixIdentity();

namespace HelperEnumTypes {
	static const enum DrawType { INDEXED_DRAW, NOT_INDEXED_DRAW };

	static const enum ReflectionType { REFLECTION_TYPE_NONE, REFLECTION_TYPE_STATIC_CUBE_MAP, REFLECTION_TYPE_DYNAMIC_CUBE_MAP,
									REFLECTION_TYPE_STATIC_CUBE_MAP_AND_SSLR, REFLECTION_TYPE_DYNAMIC_CUBE_MAP_AND_SSLR,
									REFLECTION_TYPE_SSLR, REFLECTION_TYPE_SVO_CONE_TRACING };

	static const enum RefractionType { REFRACTION_TYPE_NONE, REFRACTION_TYPE_STATIC_CUBE_MAP,
						REFRACTION_TYPE_DYNAMIC_CUBE_MAP, REFRACTION_TYPE_PLANAR,
						REFRACTION_TYPE_SVO_CONE_TRACING };
	
	static const enum MeshType { PROCEDURAL_MESH, FBX_MESH, CUSTOM_FORMAT_MESH };

	static const enum TexArrayType { EQUAL_FORMAT, DIFFERENT_FORMAT };

	static const enum MaterialType { MTL_TYPE_DEFAULT, MTL_TYPE_REFLECT_MAP };
};

using namespace HelperEnumTypes;
using namespace Materials;

struct MeshStates
{
	bool mUseTextures;
	bool mUseTransparency;
	bool mUseAdditiveBlend;
	bool mUseCulling;
	bool mUseNormalMapping;
	bool mUseDisplacementMapping;
};

struct Object {
	ID3D11Buffer* mVB;
	ID3D11Buffer* mIB;

	BoundingOrientedBox OBB;
	BoundingBox AABB;
	BoundingSphere BS;

	XMFLOAT4X4 mWorld[MAX_INSTANCES_COUNT];
	XMFLOAT4X4 mLocal;
	XMFLOAT4X4 mTexTransform[MAX_TEXTURE_COUNT];
	XMFLOAT3 mObjectDynamicCubeMapWorld[MAX_INSTANCES_COUNT];

	ID3DX11EffectTechnique* mObjectDrawTech;
	ID3D11ShaderResourceView* mDiffuseMapArraySRV;
	ID3D11ShaderResourceView* mExtDiffuseMapArraySRV[MAX_TEXTURE_COUNT];
	ID3D11ShaderResourceView* mReflectMapSRV;
	ID3D11ShaderResourceView* mNormalMapSRV;
	ID3D11ShaderResourceView* mObjectDynamicCubeMapSRV;
	ID3D11DepthStencilView* mObjectDynamicCubeMapDSV;
	ID3D11RenderTargetView* mObjectDynamicCubeMapRTV[6];

	ID3D11ShaderResourceView* mObjectPlanarRefractionMapSRV;
	ID3D11DepthStencilView* mObjectPlanarRefractionMapDSV;
	ID3D11RenderTargetView* mObjectPlanarRefractionMapRTV;

	Material mMaterial;
	MaterialType mMaterialType;

	int mTexCount;
	int mIndexCount;
	int mVertexCount;

	bool mUseTextures;
	bool mUseTransparency;
	bool mUseAdditiveBlend;
	bool mUseCulling;
	bool mUseNormalMapping;
	bool mUseDisplacementMapping;
	
	UINT mInstancesCount;

	ReflectionType mReflectionType;
	RefractionType mRefractionType;
	DrawType drawType;

	MeshType mMeshType;
	TexArrayType mTexArrayType;
	TextureType mTexType[MAX_TEXTURE_COUNT];

	D3D_PRIMITIVE_TOPOLOGY primitiveTopology;
	D3D11_VIEWPORT mCubeMapViewport;
	D3D11_VIEWPORT mPlanarRefractionMapViewport;

	float ApproxSceneSz;
	float HeightScale;
};

class MeshManager
{
private:
	MeshManager::~MeshManager();

	std::vector<FBXLoader*> pFbxLoader;

	Object *Meshes;
	int MeshCount;

	ID3D11Device* md3dDevice;
	ID3D11DeviceContext* m3dDeviceContext;
	ID3D11InputLayout* mInputLayout;

	int CubeMapSize;
	int PlanarRefractionTexSize;

	HWND mhMainWnd;

	void MeshManager::BuildDynamicCubeMapViews(int MeshIndex);
	void MeshManager::BuildPlanarRefractionMapViews(int MeshIndex);

	void MeshManager::BuildVertexInputLayout(ID3DX11EffectTechnique* tech);

public:
	MeshManager::MeshManager(ID3D11Device* device, ID3D11DeviceContext* dc, ID3DX11EffectTechnique* tech, int CubeMapSz, int planarRefrSz);

	void MeshManager::CreateBox(D3D_PRIMITIVE_TOPOLOGY primitiveTopology, DrawType drawType, XMMATRIX *TexTransform, XMMATRIX *world, ID3DX11EffectTechnique* ObjectTech,
		std::vector<std::wstring>&TexFilenames, std::vector<std::wstring>&NormalMapFilenames, int TexCount, bool UseTextures, bool UseTransparecy, bool UseAdditiveBlend, bool UseCulling, bool mUseNormalMapping, bool UseDisplacementMapping, UINT InstancesCount, ReflectionType reflectionType,
		RefractionType refractionType, Material material, float width, float height, float depth);

	void MeshManager::CreateSphere(D3D_PRIMITIVE_TOPOLOGY primitiveTopology, DrawType drawType, XMMATRIX *TexTransform, XMMATRIX *world, ID3DX11EffectTechnique* ObjectTech,
		std::vector<std::wstring>&TexFilenames, std::vector<std::wstring>&NormalMapFilenames, int TexCount, bool UseTextures, bool UseTransparecy, bool UseAdditiveBlend, bool UseCulling, bool mUseNormalMapping, bool UseDisplacementMapping, UINT InstancesCount, ReflectionType reflectionType,
		RefractionType refractionType, Material material, float radius, int sliceCount, int stackCount);

	void MeshManager::CreateGrid(D3D_PRIMITIVE_TOPOLOGY primitiveTopology, DrawType drawType, XMMATRIX *TexTransform, XMMATRIX *world, ID3DX11EffectTechnique* ObjectTech,
		std::vector<std::wstring>&TexFilenames, std::vector<std::wstring>&NormalMapFilenames, int TexCount, bool UseTextures, bool UseTransparecy, bool UseAdditiveBlend, bool UseCulling, bool mUseNormalMapping, bool UseDisplacementMapping, UINT InstancesCount, ReflectionType reflectionType,
		RefractionType refractionType, Material material, float width, float depth, int m, int n);

	void MeshManager::CreateCylinder(D3D_PRIMITIVE_TOPOLOGY primitiveTopology, DrawType drawType, XMMATRIX *TexTransform, XMMATRIX *world, ID3DX11EffectTechnique* ObjectTech,
		std::vector<std::wstring>&TexFilenames, std::vector<std::wstring>&NormalMapFilenames, int TexCount, bool UseTextures, bool UseTransparecy, bool UseAdditiveBlend, bool UseCulling, bool mUseNormalMapping, bool UseDisplacementMapping, UINT InstancesCount, ReflectionType reflectionType,
		RefractionType refractionType, Material material, float bottomRadius, float topRadius, float height,
		int sliceCount, int stackCount, bool BuildCaps);

	void MeshManager::CreateBrokenLineFromFile(D3D_PRIMITIVE_TOPOLOGY primitiveTopology, DrawType drawType, XMMATRIX *world, ID3DX11EffectTechnique* ObjectTech, bool UseTransparecy, bool UseAdditiveBlend, bool UseCulling, UINT InstancesCount,
		Material material);

	void MeshManager::CreateMeshFromFile(D3D_PRIMITIVE_TOPOLOGY primitiveTopology, DrawType drawType, XMMATRIX *world, ID3DX11EffectTechnique* ObjectTech, bool UseTransparecy, bool UseAdditiveBlend,
		bool UseCulling, UINT InstancesCount, ReflectionType reflectionType, RefractionType refractionType, Material material, std::wstring MeshFilename);

	void MeshManager::CreateFbxScene(D3D_PRIMITIVE_TOPOLOGY primitiveTopology, BuffersOptimization bo, DrawType drawType, XMMATRIX *world, ID3DX11EffectTechnique* ObjectTech, std::string SceneFilename, std::string SceneName,
		bool UseCulling, bool UseTransparency, UINT InstancesCount, ReflectionType reflectionType, RefractionType refractionType);

	///Get/Set methods

	void MeshManager::SetMeshVertexBuffer(int MeshIndex, MeshVertex *DestBuffer);
	void MeshManager::GetMeshVertexBuffer(int MeshIndex, MeshVertex *DestBuffer) const;
	void MeshManager::SetMeshIndexBuffer(int MeshIndex, int *FromBuffer);
	void MeshManager::GetMeshIndexBuffer(int MeshIndex, int *DestBuffer) const;

	int MeshManager::GetMeshVertexCount(int MeshIndex) const { return Meshes[MeshIndex].mVertexCount; };
	int MeshManager::GetMeshIndexCount(int MeshIndex) const { return Meshes[MeshIndex].mIndexCount; };
	int MeshManager::GetMeshCount() const { return MeshCount; };
	int MeshManager::GetMeshTextureCount(int MeshIndex) const { return Meshes[MeshIndex].mTexCount; };
	int MeshManager::GetMeshInstancesCount(int MeshIndex) const { return Meshes[MeshIndex].mInstancesCount; };

	D3D11_PRIMITIVE_TOPOLOGY MeshManager::GetMeshPrimitiveTopology(int MeshIndex) const { return  Meshes[MeshIndex].primitiveTopology; }

	void MeshManager::SetMeshDrawTech(int MeshIndex, ID3DX11EffectTechnique* tech) { Meshes[MeshIndex].mObjectDrawTech = tech; };
	ID3DX11EffectTechnique* MeshManager::GetMeshDrawTech(int MeshIndex) const { return Meshes[MeshIndex].mObjectDrawTech; };
	ID3D11Buffer* const* MeshManager::GetMeshVertexBuffer(int MeshIndex) const { return &Meshes[MeshIndex].mVB; };
	ID3D11Buffer* MeshManager::GetMeshIndexBuffer(int MeshIndex) const { return Meshes[MeshIndex].mIB; };
	ID3D11InputLayout* MeshManager::GetInputLayout() const { return mInputLayout; };

	MeshStates MeshManager::GetMeshStates(int MeshIndex) const {
		MeshStates retval;
		retval.mUseTextures = Meshes[MeshIndex].mUseTextures;
		retval.mUseTransparency = Meshes[MeshIndex].mUseTransparency;
		retval.mUseAdditiveBlend = Meshes[MeshIndex].mUseAdditiveBlend;
		retval.mUseCulling = Meshes[MeshIndex].mUseCulling;
		retval.mUseNormalMapping = Meshes[MeshIndex].mUseNormalMapping;
		retval.mUseDisplacementMapping = Meshes[MeshIndex].mUseDisplacementMapping;
		return retval;
	};

	ReflectionType MeshManager::GetMeshReflectionType(int MeshIndex) const { return Meshes[MeshIndex].mReflectionType; };
	RefractionType MeshManager::GetMeshRefractionType(int MeshIndex) const { return Meshes[MeshIndex].mRefractionType; };
	MaterialType MeshManager::GetMeshMaterialType(int MeshIndex) const { return Meshes[MeshIndex].mMaterialType; };

	DrawType MeshManager::GetMeshDrawType(int MeshIndex) const { return Meshes[MeshIndex].drawType; };

	void MeshManager::SetMeshWorldMatrixXM(int MeshIndex, int InstanceIndex, XMMATRIX *world);
	XMMATRIX MeshManager::GetMeshWorldMatrixXM(int MeshIndex, int InstanceIndex) const;
	void MeshManager::SetMeshLocalMatrixXM(int MeshIndex, XMMATRIX *loc);
	XMMATRIX MeshManager::GetMeshLocalMatrixXM(int MeshIndex) const;
	XMFLOAT4X4 MeshManager::GetMeshWorldMatrixF4X4(int MeshIndex, int InstanceIndex) const { return Meshes[MeshIndex].mWorld[InstanceIndex]; };
	XMFLOAT4X4 MeshManager::GetMeshLocalMatrixF4X4(int MeshIndex) const { return Meshes[MeshIndex].mLocal; };
	XMFLOAT4X4* MeshManager::GetMeshTextureTransformsF4X4(int MeshIndex) const { return Meshes[MeshIndex].mTexTransform; };
	XMFLOAT3 MeshManager::GetMeshDynamicCubeMapWorldF3(int MeshIndex, int InstanceIndex) const { return Meshes[MeshIndex].mObjectDynamicCubeMapWorld[InstanceIndex]; };
	void MeshManager::SetMeshDynamicCubeMapWorldF3(int MeshIndex, int InstanceIndex, XMFLOAT3 pos) { Meshes[MeshIndex].mObjectDynamicCubeMapWorld[InstanceIndex] = pos; };
	void MeshManager::SetMeshDynamicCubeMapWorldV(int MeshIndex, int InstanceIndex, XMVECTOR pos) { XMStoreFloat3(&Meshes[MeshIndex].mObjectDynamicCubeMapWorld[InstanceIndex], pos); };

	ID3D11ShaderResourceView* MeshManager::GetMeshDiffuseMapArraySRV(int MeshIndex) const { return Meshes[MeshIndex].mDiffuseMapArraySRV; };
	ID3D11ShaderResourceView** MeshManager::GetMeshExtDiffuseMapArraySRVs(int MeshIndex) const { return Meshes[MeshIndex].mExtDiffuseMapArraySRV; };
	ID3D11ShaderResourceView* MeshManager::GetMeshNormalMapSRV(int MeshIndex) const { return Meshes[MeshIndex].mNormalMapSRV; };
	ID3D11ShaderResourceView* MeshManager::GetMeshReflectMapSRV(int MeshIndex) const { return Meshes[MeshIndex].mReflectMapSRV; };
	ID3D11ShaderResourceView* MeshManager::GetMeshDynamicCubeMapSRV(int MeshIndex) const { return Meshes[MeshIndex].mObjectDynamicCubeMapSRV; };
	ID3D11DepthStencilView* MeshManager::GetMeshDynamicCubeMapDSV(int MeshIndex) const { return Meshes[MeshIndex].mObjectDynamicCubeMapDSV; };
	ID3D11RenderTargetView** MeshManager::GetMeshDynamicCubeMapRTVs(int MeshIndex) const { return Meshes[MeshIndex].mObjectDynamicCubeMapRTV; };
	ID3D11RenderTargetView* MeshManager::GetMeshPlanarRefractionMapRTV(int MeshIndex) const { return Meshes[MeshIndex].mObjectPlanarRefractionMapRTV; };
	ID3D11DepthStencilView* MeshManager::GetMeshPlanarRefractionMapDSV(int MeshIndex) const { return Meshes[MeshIndex].mObjectPlanarRefractionMapDSV; };
	ID3D11ShaderResourceView* MeshManager::GetMeshPlanarRefractionMapSRV(int MeshIndex) const { return Meshes[MeshIndex].mObjectPlanarRefractionMapSRV; };

	Material MeshManager::GetMeshMaterial(int MeshIndex) const { return Meshes[MeshIndex].mMaterial; };

	D3D11_VIEWPORT MeshManager::GetMeshCubeMapViewport(int MeshIndex) const { return Meshes[MeshIndex].mCubeMapViewport; };
	D3D11_VIEWPORT MeshManager::GetMeshPlanarRefractionMapViewport(int MeshIndex) const { return Meshes[MeshIndex].mPlanarRefractionMapViewport; };

	FLOAT MeshManager::GetMeshApproxSceneSz(int MeshIndex) const { return Meshes[MeshIndex].ApproxSceneSz; };
	FLOAT MeshManager::GetMeshHeightScale(int MeshIndex) const { return Meshes[MeshIndex].HeightScale; };

	TexArrayType MeshManager::GetMeshTexArrayType(int MeshIndex) const { return Meshes[MeshIndex].mTexArrayType; };
	TextureType* MeshManager::GetMeshTextureType(int MeshIndex) const { return Meshes[MeshIndex].mTexType; };

	MeshType MeshManager::GetMeshType(int MeshIndex) const { return Meshes[MeshIndex].mMeshType; };

	BoundingOrientedBox MeshManager::GetMeshOBB(int MeshIndex) const { return Meshes[MeshIndex].OBB; };
	BoundingBox MeshManager::GetMeshAABB(int MeshIndex) const { return Meshes[MeshIndex].AABB; };
	BoundingSphere MeshManager::GetMeshBS(int MeshIndex) const { return Meshes[MeshIndex].BS; };
	///End of Get/Set methods
};

#endif
