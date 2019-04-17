#ifndef _TERRAIN_H_
#define _TERRAIN_H_

#include "../Common/d3dUtil.h"
#include "Materials.h"
#include "Camera.h"

using namespace Materials;

struct TerrainVertex
{
	XMFLOAT3 Pos;
	XMFLOAT2 Tex;
	XMFLOAT2 BoundsY;
};

struct InitInfo
{
	std::wstring HeightMapFilename;
	std::wstring LayerMapFilename0;
	std::wstring LayerMapFilename1;
	std::wstring LayerMapFilename2;
	std::wstring LayerMapFilename3;
	std::wstring LayerMapFilename4;
	std::wstring LayerNormalMapFilename0;
	std::wstring LayerNormalMapFilename1;
	std::wstring LayerNormalMapFilename2;
	std::wstring LayerNormalMapFilename3;
	std::wstring LayerNormalMapFilename4;
	std::wstring BlendMapFilename;
	float HeightScale;
	int HeightmapWidth;
	int HeightmapHeight;
	float CellSpacing;
};

class Terrain
{
private:
	void BuildVertexInputLayout(ID3D11Device* device);
	void BuildFX();
	void LoadHeightmap();
	void Smooth();
	bool InBounds(int i, int j);
	float Average(int i, int j);
	void CalcAllPatchBoundsY();
	void CalcPatchBoundsY(int i, int j);
	void BuildVB(ID3D11Device* device);
	void BuildIB(ID3D11Device* device);
	void BuildHeightmapSRV(ID3D11Device* device);
public:
	Terrain(ID3D11Device* device, ID3D11DeviceContext* dc, const InitInfo& initInfo, ID3DX11Effect* mFX);

	void Draw(
		ID3D11DeviceContext* dc,
		const Camera& camera,
		ID3D11RenderTargetView** renderTargets,
		ID3D11DepthStencilView* depthStencilView,
		DrawShadowType shadowType,
		int LightIndex,
		DrawPassType dst
		);

	float GetWidth()const;
	float GetDepth()const;
	float GetHeight(float x, float z)const;

	XMMATRIX GetWorld()const;

	void SetWorld(CXMMATRIX M);
private:
	ID3DX11Effect* mTerrainFX;
	ID3DX11EffectMatrixVariable* mfxViewProj;
	ID3DX11EffectMatrixVariable* mfxWorld;
	ID3DX11EffectMatrixVariable* mfxInvWorld;
	ID3DX11EffectMatrixVariable* mfxWorldInvTranspose;
	ID3DX11EffectMatrixVariable* mfxView;
	ID3DX11EffectMatrixVariable* mfxProj;
	ID3DX11EffectVariable* mfxMaterial;
	ID3DX11EffectScalarVariable* mfxHeightScale;

	ID3D11InputLayout* mTerrainInputLayout;

	ID3DX11EffectTechnique* mTerrainTech;
	ID3DX11EffectTechnique* mBuildShadowMapTerrainTech;
	ID3DX11EffectTechnique* mBuildNormalDepthMapTerrainTech;
	ID3DX11EffectTechnique* mBuildAmbientMapTerrainTech;
	ID3DX11EffectTechnique* mBuildDiffuseMapTerrainTech;
	ID3DX11EffectTechnique* mBuildDepthMapTerrainTech;
	ID3DX11EffectTechnique* mBuildWSPositionMapTerrainTech;
	ID3DX11EffectTechnique* mGenerateGridTerrainTech;

	ID3DX11EffectScalarVariable* mfxMinDist;
	ID3DX11EffectScalarVariable* mfxMaxDist;

	float mMinDist, mMaxDist;

	ID3DX11EffectScalarVariable* mfxMinTess;
	ID3DX11EffectScalarVariable* mfxMaxTess;

	float mMinTess, mMaxTess;

	ID3DX11EffectScalarVariable* mfxTexelCellSpaceU;
	ID3DX11EffectScalarVariable* mfxTexelCellSpaceV;

	float mTexelCellSpaceU, mTexelCellSpaceV;

	ID3DX11EffectScalarVariable* mfxWorldCellSpace;
	ID3DX11EffectVectorVariable* mfxWorldFrustrumPlanes;

	float mWorldCellSpace;

	ID3DX11EffectShaderResourceVariable* mfxLayerMapArray;
	ID3DX11EffectShaderResourceVariable* mfxLayerNormalMapArray;
	ID3DX11EffectShaderResourceVariable* mfxBlendMap;
	ID3DX11EffectShaderResourceVariable* mfxHeightMap;

	ID3D11Buffer* mTerrainVB;
	ID3D11Buffer* mTerrainIB;

	ID3D11ShaderResourceView* mLayerMapArraySRV;
	ID3D11ShaderResourceView* mLayerNormalMapArraySRV;
	ID3D11ShaderResourceView* mBlendMapSRV;
	ID3D11ShaderResourceView* mHeightMapSRV;

	InitInfo mInfo;
	int mNumPatchVertices;
	int mNumPatchQuadFaces;

	int mNumPatchVertRows;
	int mNumPatchVertCols;

	XMFLOAT4X4 mTerrainWorld;

	Material mTerrainMaterial;

	std::vector<XMFLOAT2> mPatchBoundsY;
	std::vector<float> mHeightmap;

	int CellsPerPatch;

	float mTerrainMinLODdist;
	float mTerrainMaxLODdist;
	float mTerrainMaxTessFactor;
	float mTerrainMinTessFactor;
	float mTerrainHeightScale;
};

#endif
