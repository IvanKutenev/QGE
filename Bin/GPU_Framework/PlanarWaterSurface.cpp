#include "PlanarWaterSurface.h"

PlanarWaterSurface::PlanarWaterSurface(ID3D11Device* device, ID3DX11Effect* drawFX, MeshManager* meshManager, FLOAT waterHeight, UINT waterRenderTargetResolution,
	XMFLOAT3 waterNormal, XMFLOAT2 waterSurfaceSize) :
	md3dDevice(device), mDrawFX(drawFX), mWaterHeight(waterHeight), mWaterRenderTargetResolution(waterRenderTargetResolution), mWaterNormalL(waterNormal), pMeshManager(meshManager)
{
	BuildFX();
	BuildWaterReflectionViews();
	BuildWaterRefractionViews();

	mWaterWorldMatrixXM = XMMatrixTranslation(0, mWaterHeight, 0);
	XMStoreFloat4x4(&mWaterWorldMatrix, mWaterWorldMatrixXM);
	mWaterNormalLXM = XMLoadFloat3(&mWaterNormalL);
	mWaterNormalWXM = XMVector3TransformCoord(mWaterNormalLXM, mWaterWorldMatrixXM);
	XMStoreFloat3(&mWaterNormalW, mWaterNormalWXM);

	std::vector<std::wstring>TexFilenames(0);
	std::vector<std::wstring>NormalMapFilenames(0);
	XMMATRIX WaterGridTexScale = XMMatrixScaling(20.0f, 20.0f, 1.0f);
	
	NormalMapFilenames.push_back(L"Textures\\water_surface_norm.dds");
	TexFilenames.push_back(L"Textures\\water_surface.dds");
	pMeshManager->CreateGrid(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, DrawType::INDEXED_DRAW, &WaterGridTexScale, &mWaterWorldMatrixXM, mWavesTech, TexFilenames, NormalMapFilenames,
		1, true, false, false, false, true, false, 1, ReflectionType::REFLECTION_TYPE_NONE, RefractionType::REFRACTION_TYPE_PLANAR, Materials::Liquid_Water, waterSurfaceSize.x, waterSurfaceSize.y, 2, 2);
	TexFilenames.clear();
	NormalMapFilenames.clear();

	mWaterMeshIndex = pMeshManager->GetMeshCount() - 1;
}

VOID PlanarWaterSurface::Update(Camera &cam)
{
	WaterComputeLookPos(cam);
	BuildWaterReflectionCamera(cam);
}

VOID PlanarWaterSurface::WaterComputeLookPos(Camera &cam)
{
	mWaterNormalW = XMFLOAT3(0, 1, 0);
	XMVECTOR look = XMVector3Normalize(cam.GetLookXM());
	mWaterNormalWXM = XMVector3Normalize(mWaterNormalWXM);
	XMVECTOR LOOKdotNORMAL = XMVector3Dot(look, mWaterNormalWXM);
	XMFLOAT3 dot;
	XMStoreFloat3(&dot, LOOKdotNORMAL);
	float cosAngle = dot.x*(-1);
	float dist = abs(-mWaterNormalW.x*cam.GetPosition().x - mWaterNormalW.y*cam.GetPosition().y - mWaterNormalW.z
		*cam.GetPosition().z + mWaterHeight) / sqrt(mWaterNormalW.x*mWaterNormalW.x + mWaterNormalW.y*mWaterNormalW.y
			+ mWaterNormalW.z*mWaterNormalW.z);

	mWaterDist = dist;

	mLookPositionWXM = cam.GetLookXM()*(dist / cosAngle) + cam.GetPositionXM();
	XMStoreFloat3(&mLookPositionW, mLookPositionWXM);
}

VOID PlanarWaterSurface::BuildWaterReflectionCamera(Camera &cam)
{
	mWaterReflectionCamera = cam;
	XMVECTOR Pos = cam.GetPositionXM();
	Pos = Pos - 2 * mWaterDist*mWaterNormalWXM;
	XMFLOAT3 nPos;
	XMStoreFloat3(&nPos, Pos);
	mWaterReflectionCamera.SetPosition(nPos);
	XMVECTOR Look = cam.GetLookXM();
	Look = XMVector3Reflect(Look, mWaterNormalWXM);
	XMFLOAT3 nLook;
	XMStoreFloat3(&nLook, Look);
	mWaterReflectionCamera.SetLook(nLook);
	mWaterReflectionCamera.UpdateViewMatrix();
}

VOID PlanarWaterSurface::BuildWaterReflectionViews(VOID)
{
	mWaterReflectionRTV = 0;

	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = mWaterRenderTargetResolution;
	texDesc.Height = mWaterRenderTargetResolution;
	texDesc.MipLevels = 0;
	texDesc.ArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	ID3D11Texture2D* Tex = 0;
	HR(md3dDevice->CreateTexture2D(&texDesc, 0, &Tex));

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = texDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2DArray.ArraySize = 1;
	rtvDesc.Texture2DArray.MipSlice = 0;

	HR(md3dDevice->CreateRenderTargetView(Tex, &rtvDesc, &mWaterReflectionRTV));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = -1;

	HR(md3dDevice->CreateShaderResourceView(Tex, &srvDesc, &mWaterReflectionSRV));

	ReleaseCOM(Tex);

	D3D11_TEXTURE2D_DESC depthTexDesc;
	depthTexDesc.Width = mWaterRenderTargetResolution;
	depthTexDesc.Height = mWaterRenderTargetResolution;
	depthTexDesc.MipLevels = 1;
	depthTexDesc.ArraySize = 1;
	depthTexDesc.SampleDesc.Count = 1;
	depthTexDesc.SampleDesc.Quality = 0;
	depthTexDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
	depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthTexDesc.CPUAccessFlags = 0;
	depthTexDesc.MiscFlags = 0;

	ID3D11Texture2D* depthTex = 0;
	HR(md3dDevice->CreateTexture2D(&depthTexDesc, 0, &depthTex));

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Format = depthTexDesc.Format;
	dsvDesc.Flags = 0;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	HR(md3dDevice->CreateDepthStencilView(depthTex, &dsvDesc, &mWaterReflectionDSV));

	ReleaseCOM(depthTex);

	InitViewport(&mWaterReflectionViewport, 0.0f, 0.0f, mWaterRenderTargetResolution, mWaterRenderTargetResolution, 0.0f, 1.0f);
}

VOID PlanarWaterSurface::BuildWaterRefractionViews(VOID)
{
	mWaterRefractionRTV = 0;

	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = mWaterRenderTargetResolution;
	texDesc.Height = mWaterRenderTargetResolution;
	texDesc.MipLevels = 0;
	texDesc.ArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	ID3D11Texture2D* Tex = 0;
	HR(md3dDevice->CreateTexture2D(&texDesc, 0, &Tex));

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = texDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2DArray.ArraySize = 1;
	rtvDesc.Texture2DArray.MipSlice = 0;

	HR(md3dDevice->CreateRenderTargetView(Tex, &rtvDesc, &mWaterRefractionRTV));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = -1;

	HR(md3dDevice->CreateShaderResourceView(Tex, &srvDesc, &mWaterRefractionSRV));

	ReleaseCOM(Tex);

	D3D11_TEXTURE2D_DESC depthTexDesc;
	depthTexDesc.Width = mWaterRenderTargetResolution;
	depthTexDesc.Height = mWaterRenderTargetResolution;
	depthTexDesc.MipLevels = 1;
	depthTexDesc.ArraySize = 1;
	depthTexDesc.SampleDesc.Count = 1;
	depthTexDesc.SampleDesc.Quality = 0;
	depthTexDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
	depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthTexDesc.CPUAccessFlags = 0;
	depthTexDesc.MiscFlags = 0;

	ID3D11Texture2D* depthTex = 0;
	HR(md3dDevice->CreateTexture2D(&depthTexDesc, 0, &depthTex));

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Format = depthTexDesc.Format;
	dsvDesc.Flags = 0;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	HR(md3dDevice->CreateDepthStencilView(depthTex, &dsvDesc, &mWaterRefractionDSV));

	ReleaseCOM(depthTex);

	InitViewport(&mWaterRefractionViewport, 0.0f, 0.0f, mWaterRenderTargetResolution, mWaterRenderTargetResolution, 0.0f, 1.0f);
}

VOID PlanarWaterSurface::SetResources(VOID)
{
	mfxWaterReflectionTex->SetResource(mWaterReflectionSRV);
	mfxDrawWaterReflection->SetBool(mDrawWaterReflection);
	mfxWaterRefractionTex->SetResource(mWaterRefractionSRV);
	mfxDrawWaterRefraction->SetBool(mDrawWaterRefraction);
	mfxWaterHeight->SetFloat(mWaterHeight);
}

VOID PlanarWaterSurface::SelectDrawTech(DrawPassType dpt, DrawShadowType dst)
{
	if (dpt == DrawPassType::DRAW_PASS_TYPE_SHADOW_MAP_BUILD) {
		if (pMeshManager->GetMeshDrawTech(mWaterMeshIndex) == mWavesTech)
			pMeshManager->SetMeshDrawTech(mWaterMeshIndex, mBuildShadowMapWavesTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_NORMAL_DEPTH)
	{
		if (pMeshManager->GetMeshDrawTech(mWaterMeshIndex) == mWavesTech)
			pMeshManager->SetMeshDrawTech(mWaterMeshIndex, mBuildNormalDepthMapWavesTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_DEPTH)
	{
		if (pMeshManager->GetMeshDrawTech(mWaterMeshIndex) == mWavesTech)
			pMeshManager->SetMeshDrawTech(mWaterMeshIndex, mBuildDepthMapWavesTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_POSITION_WS)
	{
		if (pMeshManager->GetMeshDrawTech(mWaterMeshIndex) == mWavesTech)
			pMeshManager->SetMeshDrawTech(mWaterMeshIndex, mBuildWSPositionMapWavesTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_AMBIENT)
	{
		if (pMeshManager->GetMeshDrawTech(mWaterMeshIndex) == mWavesTech)
			pMeshManager->SetMeshDrawTech(mWaterMeshIndex, mBuildAmbientMapWavesTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_DIFFUSE)
	{
		if (pMeshManager->GetMeshDrawTech(mWaterMeshIndex) == mWavesTech)
			pMeshManager->SetMeshDrawTech(mWaterMeshIndex, mBuildDiffuseMapWavesTech);
	}
}

VOID PlanarWaterSurface::BuildFX(VOID)
{
	mWavesTech = mDrawFX->GetTechniqueByName("WavesTech");
	mBuildShadowMapWavesTech = mDrawFX->GetTechniqueByName("BuildShadowMapWavesTech");
	mBuildNormalDepthMapWavesTech = mDrawFX->GetTechniqueByName("BuildNormalDepthMapWavesTech");
	mBuildAmbientMapWavesTech = mDrawFX->GetTechniqueByName("BuildAmbientMapWavesTech");
	mBuildDiffuseMapWavesTech = mDrawFX->GetTechniqueByName("BuildDiffuseMapWavesTech");
	mBuildDepthMapWavesTech = mDrawFX->GetTechniqueByName("BuildDepthMapWavesTech");
	mBuildWSPositionMapWavesTech = mDrawFX->GetTechniqueByName("BuildWSPositionMapWavesTech");

	mfxWaterReflectionTex = mDrawFX->GetVariableByName("gWaterReflectionTex")->AsShaderResource();
	mfxWaterRefractionTex = mDrawFX->GetVariableByName("gWaterRefractionTex")->AsShaderResource();
	mfxDrawWaterReflection = mDrawFX->GetVariableByName("gDrawWaterReflection")->AsScalar();
	mfxDrawWaterRefraction = mDrawFX->GetVariableByName("gDrawWaterRefraction")->AsScalar();
	mfxWaterHeight = mDrawFX->GetVariableByName("gWaterHeight")->AsScalar();
}