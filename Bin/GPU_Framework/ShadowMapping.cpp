#include "ShadowMapping.h"

void ShadowMapping::CreateDirLightDistanceMapViews(ID3D11Device* device, ID3D11ShaderResourceView** SRV, ID3D11DepthStencilView** DSV,
	ID3D11RenderTargetView** RTV, D3D11_TEXTURE2D_DESC* RTVtexDesc, D3D11_RENDER_TARGET_VIEW_DESC* rtvDesc,
	D3D11_TEXTURE2D_DESC* DSVtexDesc, D3D11_DEPTH_STENCIL_VIEW_DESC* dsvDesc,
	D3D11_SHADER_RESOURCE_VIEW_DESC* srvDesc)
{
	ID3D11Texture2D* Tex = 0;
	if (HR(device->CreateTexture2D(RTVtexDesc, 0, &Tex)) == S_OK)
		;
	else
		MessageBoxA(NULL, "CreateTexture2D error", "Error", MB_OK);

	for (int i = 0; i < CASCADE_COUNT * MAX_DIR_LIGHTS_NUM; ++i)
	{
		rtvDesc->Texture2DArray.FirstArraySlice = i;
		if (HR(device->CreateRenderTargetView(Tex, rtvDesc, &RTV[i])) == S_OK);
		else
			MessageBoxA(NULL, "CreateRenderTargetView error", "Error", MB_OK);
	}
	if (HR(device->CreateShaderResourceView(Tex, srvDesc, SRV)) == S_OK);
	else
		MessageBoxA(NULL, "CreateShaderResourceView error", "Error", MB_OK);
	ReleaseCOM(Tex);

	ID3D11Texture2D* depthTex = 0;
	if (HR(device->CreateTexture2D(DSVtexDesc, 0, &depthTex)) == S_OK);
	else
		MessageBoxA(NULL, "CreateTexture2D error", "Error", MB_OK);

	for (int i = 0; i < MAX_DIR_LIGHTS_NUM; ++i) {
		if (HR(device->CreateDepthStencilView(depthTex, dsvDesc, &DSV[i])) == S_OK)
			;
		else
			MessageBoxA(NULL, "CreateDepthStencilView error", "Error", MB_OK);
	}
	ReleaseCOM(depthTex);
}

void ShadowMapping::CreatePointLightDistanceMapViews(ID3D11Device* device, ID3D11ShaderResourceView** SRV, ID3D11DepthStencilView** DSV,
	ID3D11RenderTargetView** RTV, D3D11_TEXTURE2D_DESC* RTVtexDesc, D3D11_RENDER_TARGET_VIEW_DESC* rtvDesc,
	D3D11_TEXTURE2D_DESC* DSVtexDesc, D3D11_DEPTH_STENCIL_VIEW_DESC* dsvDesc,
	D3D11_SHADER_RESOURCE_VIEW_DESC* srvDesc)
{
	ID3D11Texture2D* cubeTex = 0;
	if (HR(device->CreateTexture2D(RTVtexDesc, 0, &cubeTex)) == S_OK)
		;
	else
		MessageBoxA(NULL, "CreateTexture2D error", "Error", MB_OK);
	for (int i = 0; i < 6 * MAX_POINT_LIGHTS_NUM; ++i)
	{
		rtvDesc->Texture2DArray.FirstArraySlice = i;
		if (HR(device->CreateRenderTargetView(cubeTex, rtvDesc, &RTV[i])) == S_OK);
		else
			MessageBoxA(NULL, "CreateRenderTargetView error", "Error", MB_OK);
	}
	if (HR(device->CreateShaderResourceView(cubeTex, srvDesc, SRV)) == S_OK)
		;
	else
		MessageBoxA(NULL, "CreateShaderResourceView error", "Error", MB_OK);
	ReleaseCOM(cubeTex);

	ID3D11Texture2D* depthTex = 0;
	if (HR(device->CreateTexture2D(DSVtexDesc, 0, &depthTex)) == S_OK)
		;
	else
		MessageBoxA(NULL, "CreateTexture2D error", "Error", MB_OK);

	for (int i = 0; i < MAX_POINT_LIGHTS_NUM; ++i) {
		if (HR(device->CreateDepthStencilView(depthTex, dsvDesc, &DSV[i])) == S_OK)
			;
		else
			MessageBoxA(NULL, "CreateDepthStencilView error", "Error", MB_OK);
	}
	ReleaseCOM(depthTex);
}

void ShadowMapping::CreateSpotLightDistanceMapViews(ID3D11Device* device, ID3D11ShaderResourceView** SRV, ID3D11DepthStencilView** DSV,
	ID3D11RenderTargetView** RTV, D3D11_TEXTURE2D_DESC* RTVtexDesc, D3D11_RENDER_TARGET_VIEW_DESC* rtvDesc,
	D3D11_TEXTURE2D_DESC* DSVtexDesc, D3D11_DEPTH_STENCIL_VIEW_DESC* dsvDesc,
	D3D11_SHADER_RESOURCE_VIEW_DESC* srvDesc)
{
	ID3D11Texture2D* tex = 0;
	if (HR(device->CreateTexture2D(RTVtexDesc, 0, &tex)) == S_OK)
		;
	else
		MessageBoxA(NULL, "CreateTexture2D error", "Error", MB_OK);

	for (int i = 0; i < MAX_SPOT_LIGHTS_NUM; ++i)
	{
		rtvDesc->Texture2DArray.FirstArraySlice = i;
		if (HR(device->CreateRenderTargetView(tex, rtvDesc, &RTV[i])) == S_OK)
			;
		else
			MessageBoxA(NULL, "CreateRenderTargetView error", "Error", MB_OK);
	}

	if (HR(device->CreateShaderResourceView(tex, srvDesc, SRV)) == S_OK)
		;
	else
		MessageBoxA(NULL, "CreateShaderResourceView error", "Error", MB_OK);
	ReleaseCOM(tex);

	ID3D11Texture2D* depthTex = 0;
	if (HR(device->CreateTexture2D(DSVtexDesc, 0, &depthTex)) == S_OK)
		;
	else
		MessageBoxA(NULL, "CreateTexture2D error", "Error", MB_OK);

	for (int i = 0; i < MAX_SPOT_LIGHTS_NUM; ++i) {
		if (HR(device->CreateDepthStencilView(depthTex, dsvDesc, &DSV[i])) == S_OK)
			;
		else
			MessageBoxA(NULL, "CreateDepthStencilView error", "Error", MB_OK);
	}
	ReleaseCOM(depthTex);
}

ShadowMapping::ShadowMapping(ID3D11Device* device, float sz) : mWidth((int)sz), mHeight((int)sz),
mDirLightDistanceMapSRV(0), mSpotLightDistanceMapSRV(0), mPointLightDistanceMapSRV(0)
{
	InitViewport(&mDirLightDepthMapViewport, 0.0f, 0.0f, mWidth, mHeight, 0.0f, 1.0f);
	InitViewport(&mSpotLightDepthMapViewport, 0.0f, 0.0f, mWidth, mHeight, 0.0f, 1.0f);
	InitViewport(&mPointLightDepthMapViewport, 0.0f, 0.0f, mWidth, mHeight, 0.0f, 1.0f);

	//Cascaded distance maps for directional lights
	D3D11_TEXTURE2D_DESC DirLightTexDesc;
	DirLightTexDesc.Width = mWidth;
	DirLightTexDesc.Height = mHeight;
	DirLightTexDesc.MipLevels = 1;
	DirLightTexDesc.ArraySize = CASCADE_COUNT * MAX_DIR_LIGHTS_NUM;
	DirLightTexDesc.SampleDesc.Count = 1;
	DirLightTexDesc.SampleDesc.Quality = 0;
	DirLightTexDesc.Format = DXGI_FORMAT_R32_FLOAT;
	DirLightTexDesc.Usage = D3D11_USAGE_DEFAULT;
	DirLightTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	DirLightTexDesc.CPUAccessFlags = 0;
	DirLightTexDesc.MiscFlags = 0;

	D3D11_RENDER_TARGET_VIEW_DESC DirLightRTVDesc;
	DirLightRTVDesc.Format = DirLightTexDesc.Format;
	DirLightRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	DirLightRTVDesc.Texture2DArray.ArraySize = 1;
	DirLightRTVDesc.Texture2DArray.MipSlice = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC DirLightSRVDesc;
	DirLightSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	DirLightSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	DirLightSRVDesc.Texture2DArray.MostDetailedMip = 0;
	DirLightSRVDesc.Texture2DArray.MipLevels = DirLightTexDesc.MipLevels;
	DirLightSRVDesc.Texture2DArray.FirstArraySlice = 0;
	DirLightSRVDesc.Texture2DArray.ArraySize = CASCADE_COUNT * MAX_DIR_LIGHTS_NUM;

	D3D11_TEXTURE2D_DESC DirLightDepthTexDesc;
	DirLightDepthTexDesc.Width = mWidth;
	DirLightDepthTexDesc.Height = mHeight;
	DirLightDepthTexDesc.MipLevels = 1;
	DirLightDepthTexDesc.ArraySize = 1;
	DirLightDepthTexDesc.SampleDesc.Count = 1;
	DirLightDepthTexDesc.SampleDesc.Quality = 0;
	DirLightDepthTexDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DirLightDepthTexDesc.Usage = D3D11_USAGE_DEFAULT;
	DirLightDepthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	DirLightDepthTexDesc.CPUAccessFlags = 0;
	DirLightDepthTexDesc.MiscFlags = 0;

	D3D11_DEPTH_STENCIL_VIEW_DESC DirLightDSVDesc;
	DirLightDSVDesc.Format = DirLightDepthTexDesc.Format;
	DirLightDSVDesc.Flags = 0;
	DirLightDSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DirLightDSVDesc.Texture2D.MipSlice = 0;

	CreateDirLightDistanceMapViews(device, &mDirLightDistanceMapSRV, mDirLightDistanceMapDSV, mDirLightDistanceMapRTV,
		&DirLightTexDesc, &DirLightRTVDesc, &DirLightDepthTexDesc, &DirLightDSVDesc, &DirLightSRVDesc);

	//Distance maps for spot lights
	D3D11_TEXTURE2D_DESC SpotLightTexDesc;
	SpotLightTexDesc.Width = mWidth;
	SpotLightTexDesc.Height = mHeight;
	SpotLightTexDesc.MipLevels = 1;
	SpotLightTexDesc.ArraySize = MAX_SPOT_LIGHTS_NUM;
	SpotLightTexDesc.SampleDesc.Count = 1;
	SpotLightTexDesc.SampleDesc.Quality = 0;
	SpotLightTexDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SpotLightTexDesc.Usage = D3D11_USAGE_DEFAULT;
	SpotLightTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	SpotLightTexDesc.CPUAccessFlags = 0;
	SpotLightTexDesc.MiscFlags = 0;

	D3D11_RENDER_TARGET_VIEW_DESC SpotLightRTVDesc;
	SpotLightRTVDesc.Format = SpotLightTexDesc.Format;
	SpotLightRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	SpotLightRTVDesc.Texture2DArray.ArraySize = 1;
	SpotLightRTVDesc.Texture2DArray.MipSlice = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC SpotLightSRVDesc;
	SpotLightSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SpotLightSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	SpotLightSRVDesc.Texture2DArray.MostDetailedMip = 0;
	SpotLightSRVDesc.Texture2DArray.MipLevels = SpotLightTexDesc.MipLevels;
	SpotLightSRVDesc.Texture2DArray.FirstArraySlice = 0;
	SpotLightSRVDesc.Texture2DArray.ArraySize = MAX_SPOT_LIGHTS_NUM;

	D3D11_TEXTURE2D_DESC SpotLightDepthTexDesc;
	SpotLightDepthTexDesc.Width = mWidth;
	SpotLightDepthTexDesc.Height = mHeight;
	SpotLightDepthTexDesc.MipLevels = 1;
	SpotLightDepthTexDesc.ArraySize = 1;
	SpotLightDepthTexDesc.SampleDesc.Count = 1;
	SpotLightDepthTexDesc.SampleDesc.Quality = 0;
	SpotLightDepthTexDesc.Format = DXGI_FORMAT_D32_FLOAT;
	SpotLightDepthTexDesc.Usage = D3D11_USAGE_DEFAULT;
	SpotLightDepthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	SpotLightDepthTexDesc.CPUAccessFlags = 0;
	SpotLightDepthTexDesc.MiscFlags = 0;

	D3D11_DEPTH_STENCIL_VIEW_DESC SpotLightDSVDesc;
	SpotLightDSVDesc.Format = SpotLightDepthTexDesc.Format;
	SpotLightDSVDesc.Flags = 0;
	SpotLightDSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	SpotLightDSVDesc.Texture2D.MipSlice = 0;

	CreateSpotLightDistanceMapViews(device, &mSpotLightDistanceMapSRV, mSpotLightDistanceMapDSV, mSpotLightDistanceMapRTV,
		&SpotLightTexDesc, &SpotLightRTVDesc, &SpotLightDepthTexDesc, &SpotLightDSVDesc, &SpotLightSRVDesc);

	//Cubic distance maps for point lights
	D3D11_TEXTURE2D_DESC PointLightTexDesc;
	PointLightTexDesc.Width = mWidth;
	PointLightTexDesc.Height = mHeight;
	PointLightTexDesc.MipLevels = 1;
	PointLightTexDesc.ArraySize = 6 * MAX_POINT_LIGHTS_NUM;
	PointLightTexDesc.SampleDesc.Count = 1;
	PointLightTexDesc.SampleDesc.Quality = 0;
	PointLightTexDesc.Format = DXGI_FORMAT_R32_FLOAT;
	PointLightTexDesc.Usage = D3D11_USAGE_DEFAULT;
	PointLightTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	PointLightTexDesc.CPUAccessFlags = 0;
	PointLightTexDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	D3D11_RENDER_TARGET_VIEW_DESC PointLightRTVDesc;
	PointLightRTVDesc.Format = PointLightTexDesc.Format;
	PointLightRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	PointLightRTVDesc.Texture2DArray.ArraySize = 1;
	PointLightRTVDesc.Texture2DArray.MipSlice = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC PointLightSRVDesc;
	PointLightSRVDesc.Format = PointLightTexDesc.Format;
	PointLightSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
	PointLightSRVDesc.TextureCubeArray.MostDetailedMip = 0;
	PointLightSRVDesc.TextureCubeArray.MipLevels = -1;
	PointLightSRVDesc.TextureCubeArray.First2DArrayFace = 0;
	PointLightSRVDesc.TextureCubeArray.NumCubes = MAX_POINT_LIGHTS_NUM;

	D3D11_TEXTURE2D_DESC PointLightDepthTexDesc;
	PointLightDepthTexDesc.Width = mWidth;
	PointLightDepthTexDesc.Height = mHeight;
	PointLightDepthTexDesc.MipLevels = 1;
	PointLightDepthTexDesc.ArraySize = 1;
	PointLightDepthTexDesc.SampleDesc.Count = 1;
	PointLightDepthTexDesc.SampleDesc.Quality = 0;
	PointLightDepthTexDesc.Format = DXGI_FORMAT_D32_FLOAT;
	PointLightDepthTexDesc.Usage = D3D11_USAGE_DEFAULT;
	PointLightDepthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	PointLightDepthTexDesc.CPUAccessFlags = 0;
	PointLightDepthTexDesc.MiscFlags = 0;

	D3D11_DEPTH_STENCIL_VIEW_DESC PointLightDSVDesc;
	PointLightDSVDesc.Format = PointLightDepthTexDesc.Format;
	PointLightDSVDesc.Flags = 0;
	PointLightDSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	PointLightDSVDesc.Texture2D.MipSlice = 0;

	CreatePointLightDistanceMapViews(device, &mPointLightDistanceMapSRV, mPointLightDistanceMapDSV, mPointLightDistanceMapRTV,
		&PointLightTexDesc, &PointLightRTVDesc, &PointLightDepthTexDesc, &PointLightDSVDesc, &PointLightSRVDesc);
	////////////////////////////////////////////////////////////////////////////////////
}

ShadowMapping::~ShadowMapping()
{
	ReleaseCOM(mDirLightDistanceMapSRV);
	for (int i = 0; i < MAX_DIR_LIGHTS_NUM; ++i)
		ReleaseCOM(mDirLightDistanceMapDSV[i]);
	for (int i = 0; i < CASCADE_COUNT * MAX_DIR_LIGHTS_NUM; ++i)
		ReleaseCOM(mDirLightDistanceMapRTV[i]);

	ReleaseCOM(mSpotLightDistanceMapSRV);
	for (int i = 0; i < MAX_SPOT_LIGHTS_NUM; ++i)
		ReleaseCOM(mSpotLightDistanceMapDSV[i]);
	for (int i = 0; i < MAX_SPOT_LIGHTS_NUM; ++i)
		ReleaseCOM(mSpotLightDistanceMapRTV[i]);

	ReleaseCOM(mPointLightDistanceMapSRV);
	for (int i = 0; i < MAX_POINT_LIGHTS_NUM; ++i)
		ReleaseCOM(mPointLightDistanceMapDSV[i]);
	for (int i = 0; i < 6 * MAX_POINT_LIGHTS_NUM; ++i)
		ReleaseCOM(mPointLightDistanceMapRTV[i]);
}

void ShadowMapping::DirLightShadowMapSetup(ID3D11DeviceContext* dc, int CascadeIndex, int DirLightIndex)
{
	float RGBA[] = { Z_FAR, Z_FAR, Z_FAR, Z_FAR };

	dc->RSSetViewports(1, &mDirLightDepthMapViewport);
	dc->ClearDepthStencilView(mDirLightDistanceMapDSV[DirLightIndex], D3D11_CLEAR_DEPTH, 1.0f, 0);
	dc->ClearRenderTargetView(mDirLightDistanceMapRTV[CascadeIndex + DirLightIndex * CASCADE_COUNT], RGBA);
}

void ShadowMapping::SpotLightShadowMapSetup(ID3D11DeviceContext* dc, int SpotLightIndex)
{
	float RGBA[] = { Z_FAR, Z_FAR, Z_FAR, Z_FAR };

	dc->RSSetViewports(1, &mSpotLightDepthMapViewport);
	dc->ClearDepthStencilView(mSpotLightDistanceMapDSV[SpotLightIndex], D3D11_CLEAR_DEPTH, 1.0f, 0);
	dc->ClearRenderTargetView(mSpotLightDistanceMapRTV[SpotLightIndex], RGBA);
}

void ShadowMapping::PointLightShadowMapSetup(ID3D11DeviceContext* dc, int CubeMapFaceIndex, int PointLightIndex)
{
	float RGBA[] = { Z_FAR, Z_FAR, Z_FAR, Z_FAR };

	dc->RSSetViewports(1, &mPointLightDepthMapViewport);
	dc->ClearDepthStencilView(mPointLightDistanceMapDSV[PointLightIndex], D3D11_CLEAR_DEPTH, 1.0f, 0);
	dc->ClearRenderTargetView(mPointLightDistanceMapRTV[CubeMapFaceIndex + PointLightIndex * 6], RGBA);
}


void ShadowMapping::UpdateDirLightMatrices(DirectionalLight &DirLight, Camera &PlayerCam, int DirLightIndex)
{
	for (int i = 0; i < CASCADE_COUNT; ++i)
	{
		XMFLOAT3X4X4 VPS;
		VPS = BuildDirLightMatricesCSF(DirLight, PlayerCam, i);
		mDirLightView[i + DirLightIndex * CASCADE_COUNT] = VPS.Matrix_0;
		mDirLightProj[i + DirLightIndex * CASCADE_COUNT] = VPS.Matrix_1;
		mDirLightShadowTransform[i + DirLightIndex * CASCADE_COUNT] = VPS.Matrix_2;
	}
}

void ShadowMapping::UpdateSpotLightMatrices(SpotLight &SLight, Camera &PlayerCam,
	float Fov_Angle_Y, float AspectRatio, float Near_Z, int SpotLightIndex)
{
	XMFLOAT3X4X4 VPS = BuildSpotLightMatrices(SLight, PlayerCam, Fov_Angle_Y, AspectRatio, Near_Z);
	mSpotLightView[SpotLightIndex] = VPS.Matrix_0;
	mSpotLightProj[SpotLightIndex] = VPS.Matrix_1;
	mSpotLightShadowTransform[SpotLightIndex] = VPS.Matrix_2;
}

void ShadowMapping::UpdatePointLightMatrices(PointLight &PLight, Camera &PlayerCam,
	float Near_Z, int PointLightIndex)
{
	for (int i = 0; i < 6; ++i)
	{
		XMFLOAT2X4X4 VP = BuildPointLightMatrices(PLight, PlayerCam, Near_Z, i);
		mPointLightView[i + PointLightIndex * 6] = VP.Matrix_0;
		mPointLightProj[i + PointLightIndex * 6] = VP.Matrix_1;
	}
}

XMFLOAT3X4X4 ShadowMapping::BuildDirLightMatricesCSF(DirectionalLight &DirLight, Camera &PlayerCam, int CascadeIndex) { // Coverage Sub-Frustums

	float FarPlane = mDirLightSplitDistances[CascadeIndex].x;

	XMVECTOR lookXright = XMVector3Cross(PlayerCam.GetLookXM(), PlayerCam.GetRightXM());

	XMVECTOR NearPlaneCenter = PlayerCam.GetNearZ()*XMVector3Normalize(PlayerCam.GetLookXM()) + PlayerCam.GetPositionXM(),
		FarPlaneCenter = FarPlane*XMVector3Normalize(PlayerCam.GetLookXM()) + PlayerCam.GetPositionXM();
	XMVECTOR p;
	p = FarPlaneCenter - XMVector3Normalize(PlayerCam.GetRightXM()) *
		PlayerCam.GetFarWindowWidth()*(FarPlane / PlayerCam.GetFarZ()) + XMVector3Normalize(lookXright) *
		PlayerCam.GetFarWindowHeight()*(FarPlane / PlayerCam.GetFarZ());
	XMVECTOR FrustumBBOXcenter = (NearPlaneCenter + FarPlaneCenter) / 2;
	XMVECTOR r = XMVector3Length(FrustumBBOXcenter - p);

	XMFLOAT3 Center, Radius3;
	XMStoreFloat3(&Radius3, r);
	float Radius = Radius3.x;
	XMStoreFloat3(&Center, FrustumBBOXcenter);

	XMVECTOR lightDir = XMLoadFloat3(&DirLight.Direction);
	XMVECTOR lightPos = -2.0f*Radius*lightDir + FrustumBBOXcenter;
	XMVECTOR targetPos = XMLoadFloat3(&Center);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX V = XMMatrixLookAtLH(lightPos, targetPos, up);

	// Transform bounding sphere to light space.
	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, V));

	// Ortho frustum in light space encloses scene.
	float leftPt = sphereCenterLS.x - Radius;
	float bottomPt = sphereCenterLS.y - Radius;
	float nearPt = sphereCenterLS.z - Radius;
	float rightPt = sphereCenterLS.x + Radius;
	float topPt = sphereCenterLS.y + Radius;
	float farPt = sphereCenterLS.z + Radius;
	XMMATRIX P = XMMatrixOrthographicOffCenterLH(leftPt, rightPt, bottomPt,
		topPt, nearPt, farPt);

	XMMATRIX S = V*P*NDCSpacetoTexSpaceTransform;

	XMFLOAT3X4X4 returnValue;

	XMStoreFloat4x4(&returnValue.Matrix_0, V);
	XMStoreFloat4x4(&returnValue.Matrix_1, P);
	XMStoreFloat4x4(&returnValue.Matrix_2, S);

	return returnValue;
}
XMFLOAT3X4X4 ShadowMapping::BuildSpotLightMatrices(SpotLight &SLight, Camera &PlayerCam,
	float Fov_Angle_Y, float AspectRatio, float Near_Z) {
	XMVECTOR lightDir = XMLoadFloat3(&SLight.Direction);
	XMVECTOR lightPos = XMLoadFloat3(&SLight.Position);
	XMVECTOR targetPos = lightPos + lightDir;
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX V = XMMatrixLookAtLH(lightPos, targetPos, up);

	XMMATRIX P = XMMatrixPerspectiveFovLH(Fov_Angle_Y, AspectRatio, Near_Z, SLight.Range);

	XMMATRIX S = V*P*NDCSpacetoTexSpaceTransform;

	XMFLOAT3X4X4 returnValue;

	XMStoreFloat4x4(&returnValue.Matrix_0, V);
	XMStoreFloat4x4(&returnValue.Matrix_1, P);
	XMStoreFloat4x4(&returnValue.Matrix_2, S);

	return returnValue;
}

XMFLOAT2X4X4 ShadowMapping::BuildPointLightMatrices(PointLight &PLight, Camera &PlayerCam,
	float Near_Z, int CubeMapFaceIndex) {

	float x = PLight.Position.x;
	float y = PLight.Position.y;
	float z = PLight.Position.z;

	XMFLOAT3 targets[6] =
	{
		XMFLOAT3(x + 1.0f, y, z), // +X
		XMFLOAT3(x - 1.0f, y, z), // -X
		XMFLOAT3(x, y + 1.0f, z), // +Y
		XMFLOAT3(x, y - 1.0f, z), // -Y
		XMFLOAT3(x, y, z + 1.0f), // +Z
		XMFLOAT3(x, y, z - 1.0f)  // -Z
	};

	XMFLOAT3 ups[6] =
	{
		XMFLOAT3(0.0f, 1.0f, 0.0f),  // +X
		XMFLOAT3(0.0f, 1.0f, 0.0f),  // -X
		XMFLOAT3(0.0f, 0.0f, -1.0f), // +Y
		XMFLOAT3(0.0f, 0.0f, +1.0f), // -Y
		XMFLOAT3(0.0f, 1.0f, 0.0f),	 // +Z
		XMFLOAT3(0.0f, 1.0f, 0.0f)	 // -Z
	};

	XMVECTOR lightPos = XMLoadFloat3(&PLight.Position);
	XMVECTOR targetPos = XMLoadFloat3(&targets[CubeMapFaceIndex]);
	XMVECTOR up = XMLoadFloat3(&ups[CubeMapFaceIndex]);

	XMMATRIX V = XMMatrixLookAtLH(lightPos, targetPos, up);

	XMMATRIX P = XMMatrixPerspectiveFovLH(XM_PI / 2, 1.0f, Near_Z, Z_FAR);

	XMFLOAT2X4X4 returnValue;

	XMStoreFloat4x4(&returnValue.Matrix_0, V);
	XMStoreFloat4x4(&returnValue.Matrix_1, P);

	return returnValue;
}

Camera ShadowMapping::CreateDirLightCameraFromPlayerCam(Camera &PlayerCam, int CascadeIndex, int DirLightIndex)
{
	Camera lightCam;
	lightCam.SetView(mDirLightView[CascadeIndex + DirLightIndex * CASCADE_COUNT]);
	lightCam.SetProj(mDirLightProj[CascadeIndex + DirLightIndex * CASCADE_COUNT]);
	lightCam.SetPosition(PlayerCam.GetPosition());
	return lightCam;
}

Camera ShadowMapping::CreateSpotLightCameraFromPlayerCam(Camera &PlayerCam, int SpotLightIndex)
{
	Camera lightCam;
	lightCam.SetView(mSpotLightView[SpotLightIndex]);
	lightCam.SetProj(mSpotLightProj[SpotLightIndex]);
	lightCam.SetPosition(PlayerCam.GetPosition());
	return lightCam;
}

Camera ShadowMapping::CreatePointLightCameraFromPlayerCam(Camera &PlayerCam, int CubeMapFaceIndex, int PointLightIndex)
{
	Camera lightCam;
	lightCam.SetView(mPointLightView[CubeMapFaceIndex + PointLightIndex * 6]);
	lightCam.SetProj(mPointLightProj[CubeMapFaceIndex + PointLightIndex * 6]);
	lightCam.SetPosition(PlayerCam.GetPosition());
	return lightCam;
}

void ShadowMapping::CalculateSplitDistances(Camera &PlayerCam)
{
	for (int i = 1; i <= CASCADE_COUNT; ++i)
	{
		float f = (float)i / (float)CASCADE_COUNT;
		float l = PlayerCam.GetNearZ() * pow(PlayerCam.GetFarZ() / PlayerCam.GetNearZ(), f);
		float u = PlayerCam.GetNearZ() + (PlayerCam.GetFarZ() - PlayerCam.GetNearZ()) * f;
		mDirLightSplitDistances[i - 1] = XMFLOAT4(l * SplitLamda + u * (1.0f - SplitLamda),
			l * SplitLamda + u * (1.0f - SplitLamda),
			l * SplitLamda + u * (1.0f - SplitLamda),
			l * SplitLamda + u * (1.0f - SplitLamda));
	}
}