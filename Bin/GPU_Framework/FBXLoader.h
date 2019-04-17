#ifndef _FBXLOADER_H_
#define _FBXLOADER_H_

#include "../Common/d3dUtil.h"
#include "Materials.h"
#include <fbxsdk.h>
#include <map>

#define NON_MAPPED_TEXC -100500.0f

static const enum TextureType { TEX_TYPE_DIFFUSE_MAP, TEX_TYPE_SHININESS_MAP, TEX_TYPE_NORMAL_MAP, TEX_TYPE_TRANSPARENT_MAP, TEX_TYPE_UNKNOWN};
static const enum BuffersOptimization { BUFFERS_OPTIMIZATION_DISABLE, BUFFERS_OPTIMIZATION_ENABLE };

static const Material DefaultMaterial = Materials::Plastic_Optorez1330;
static const std::pair<BOOL, BOOL>RewriteNormals(TRUE, TRUE);

struct MeshVertex
{
	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
	XMFLOAT2 TexCoord;
	XMFLOAT3 Tangent;
};

struct TexInfo {
	std::wstring TexFilename;
	std::wstring TexUVChannelName;
	std::wstring TexTypeName;
	FbxAMatrix TexTransform;
	TextureType TexType;
};

struct Mesh
{
	FbxMesh* mMesh;
	Material mMeshMtl;
	std::vector<TexInfo>mTextures;
	std::vector<MeshVertex>mVertices;
	std::vector<int>mIndicies;
};

struct Node
{
	FbxNode* mNode;
	FbxAMatrix mGlobalTransform;
	std::vector<Mesh>mMeshes;
};

class FBXLoader {
private:
	~FBXLoader();

	void Init();

	void GetNodesListByRootNode(FbxNode* pRootNode);
	void GetMeshesListByRootNode(FbxNode* pRootNode);
	void GetNodesBuffers();
	void GetNodeVertices(Node* pNode);
	void GetMeshVertices(Node* pNode, int MeshIndex);
	void GetNodeIndicies(Node* pNode);
	void GetMeshIndicies(Node* pNode, int MeshIndex);
	void GetNodeTextures(Node* pNode);
	void GetNodeMaterials(Node* pNode);
	std::vector<std::pair<std::pair<std::wstring, std::wstring>, FbxAMatrix>> GetTextureFilename(FbxProperty Property, int &TexIndex);
	FbxAMatrix GetNodeGlobalTransform(FbxNode* pNode);
	BuffersOptimization mBuffersOptimizization;
private:
	FbxManager* mSdkManager;
	FbxIOSettings* mIOSettings;

	FbxScene* mScene;
	FbxNode* mRootNode;
public:
	FBXLoader(const char* Filename, const char* SceneName, BuffersOptimization bo);
	std::vector<Node>mNodeList;
};
#endif