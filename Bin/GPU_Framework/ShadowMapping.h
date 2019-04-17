#ifndef _CSM_H_
#define _CSM_H_

#include "../Common/d3dUtil.h"
#include "Camera.h"

#define SHADOW_MAP_SIZE 4096
#define CASCADE_COUNT 4

#define MAX_DIR_LIGHTS_NUM 3
#define MAX_POINT_LIGHTS_NUM 3
#define MAX_SPOT_LIGHTS_NUM 3

static const float SplitLamda = 0.5f;
static const XMMATRIX NDCSpacetoTexSpaceTransform(
	0.5f, 0.0f, 0.0f, 0.0f,
	0.0f, -0.5f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.5f, 0.5f, 0.0f, 1.0f);

class ShadowMapping {
public: 
	ShadowMapping(ID3D11Device* device, float sz);
	~ShadowMapping();

	//For directional lights
	ID3D11ShaderResourceView* GetDirLightDistanceMapSRV() { return mDirLightDistanceMapSRV; };
	ID3D11DepthStencilView* GetDirLightDistanceMapDSV(int DirLightIndex) { return mDirLightDistanceMapDSV[DirLightIndex]; };
	ID3D11RenderTargetView* GetDirLightDistanceMapRTV(int CascadeIndex, int DirLightIndex) { return mDirLightDistanceMapRTV[CascadeIndex + DirLightIndex * CASCADE_COUNT]; };

	void DirLightShadowMapSetup(ID3D11DeviceContext*, int CascadeIndex, int DirLightIndex);

	void UpdateDirLightMatrices(DirectionalLight&, Camera&, int DirLightIndex);
	Camera CreateDirLightCameraFromPlayerCam(Camera&, int CascadeIndex, int DirLightIndex);
	XMFLOAT4X4 mDirLightShadowTransform[CASCADE_COUNT * MAX_DIR_LIGHTS_NUM];

	void CalculateSplitDistances(Camera&);
	XMFLOAT4 mDirLightSplitDistances[CASCADE_COUNT];

	//For spot lights
	ID3D11ShaderResourceView* GetSpotLightDistanceMapSRV() { return mSpotLightDistanceMapSRV; };
	ID3D11DepthStencilView* GetSpotLightDistanceMapDSV(int SpotLightIndex) { return mSpotLightDistanceMapDSV[SpotLightIndex]; };
	ID3D11RenderTargetView* GetSpotLightDistanceMapRTV(int SpotLightIndex) { return mSpotLightDistanceMapRTV[SpotLightIndex]; };
	
	void SpotLightShadowMapSetup(ID3D11DeviceContext*, int SpotLightIndex);

	void UpdateSpotLightMatrices(SpotLight&, Camera&,
		float Fov_Angle_Y, float AspectRatio, float Near_Z, int SpotLightIndex);
	Camera CreateSpotLightCameraFromPlayerCam(Camera&, int SpotLightIndex);
	XMFLOAT4X4 mSpotLightShadowTransform[MAX_SPOT_LIGHTS_NUM];

	//For point lights
	ID3D11ShaderResourceView* GetPointLightDistanceMapSRV() { return mPointLightDistanceMapSRV; };
	ID3D11DepthStencilView* GetPointLightDistanceMapDSV(int PointLightIndex) { return mPointLightDistanceMapDSV[PointLightIndex]; };
	ID3D11RenderTargetView* GetPointLightDistanceMapRTV(int CubeMapFaceIndex, int PointLightIndex) { return mPointLightDistanceMapRTV[CubeMapFaceIndex + PointLightIndex * 6]; }

	void PointLightShadowMapSetup(ID3D11DeviceContext*, int CubeMapFaceIndex, int PointLightIndex);

	void UpdatePointLightMatrices(PointLight&, Camera&,
		float Near_Z, int PointLightIndex);
	Camera CreatePointLightCameraFromPlayerCam(Camera&, int CubeMapFaceIndex, int PointLightIndex);
private:
	XMFLOAT3X4X4 BuildDirLightMatricesCSF(DirectionalLight&, Camera&, int CascadeIndex);
	XMFLOAT3X4X4 BuildSpotLightMatrices(SpotLight&, Camera&,
		float Fov_Angle_Y, float AspectRatio, float Near_Z);
	XMFLOAT2X4X4 BuildPointLightMatrices(PointLight&, Camera&,
		float Near_Z, int CubeMapFaceIndex);

	void CreateDirLightDistanceMapViews(ID3D11Device*, ID3D11ShaderResourceView**, ID3D11DepthStencilView**,
		ID3D11RenderTargetView**, D3D11_TEXTURE2D_DESC*, D3D11_RENDER_TARGET_VIEW_DESC*,
		D3D11_TEXTURE2D_DESC*, D3D11_DEPTH_STENCIL_VIEW_DESC*,
		D3D11_SHADER_RESOURCE_VIEW_DESC*);

	void CreateSpotLightDistanceMapViews(ID3D11Device*, ID3D11ShaderResourceView**, ID3D11DepthStencilView**,
		ID3D11RenderTargetView**, D3D11_TEXTURE2D_DESC*, D3D11_RENDER_TARGET_VIEW_DESC*,
		D3D11_TEXTURE2D_DESC*, D3D11_DEPTH_STENCIL_VIEW_DESC*,
		D3D11_SHADER_RESOURCE_VIEW_DESC*);

	void CreatePointLightDistanceMapViews(ID3D11Device*, ID3D11ShaderResourceView**, ID3D11DepthStencilView**,
		ID3D11RenderTargetView**, D3D11_TEXTURE2D_DESC*, D3D11_RENDER_TARGET_VIEW_DESC*,
		D3D11_TEXTURE2D_DESC*, D3D11_DEPTH_STENCIL_VIEW_DESC*,
		D3D11_SHADER_RESOURCE_VIEW_DESC*);

	int mWidth;
	int mHeight;

	//For directional lights
	ID3D11ShaderResourceView* mDirLightDistanceMapSRV;
	ID3D11DepthStencilView* mDirLightDistanceMapDSV[MAX_DIR_LIGHTS_NUM];
	ID3D11RenderTargetView* mDirLightDistanceMapRTV[CASCADE_COUNT * MAX_DIR_LIGHTS_NUM];

	D3D11_VIEWPORT mDirLightDepthMapViewport;

	XMFLOAT4X4 mDirLightView[CASCADE_COUNT * MAX_DIR_LIGHTS_NUM];
	XMFLOAT4X4 mDirLightProj[CASCADE_COUNT * MAX_DIR_LIGHTS_NUM];

	//For spot lights
	ID3D11ShaderResourceView* mSpotLightDistanceMapSRV;
	ID3D11RenderTargetView* mSpotLightDistanceMapRTV[MAX_SPOT_LIGHTS_NUM];
	ID3D11DepthStencilView* mSpotLightDistanceMapDSV[MAX_SPOT_LIGHTS_NUM];

	D3D11_VIEWPORT mSpotLightDepthMapViewport;

	XMFLOAT4X4 mSpotLightView[MAX_SPOT_LIGHTS_NUM];
	XMFLOAT4X4 mSpotLightProj[MAX_SPOT_LIGHTS_NUM];

	//For point lights
	ID3D11ShaderResourceView* mPointLightDistanceMapSRV;
	ID3D11RenderTargetView* mPointLightDistanceMapRTV[6 * MAX_POINT_LIGHTS_NUM];
	ID3D11DepthStencilView* mPointLightDistanceMapDSV[MAX_POINT_LIGHTS_NUM];

	D3D11_VIEWPORT mPointLightDepthMapViewport;

	XMFLOAT4X4 mPointLightView[6 * MAX_POINT_LIGHTS_NUM];
	XMFLOAT4X4 mPointLightProj[6 * MAX_POINT_LIGHTS_NUM];

	/////////////////////////////////////////
};

#endif