#include "QGE_GPU_Framework.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	QGE_GPU_Framework theApp(hInstance);

	if (!theApp.Init())
		return 0;

	return theApp.Run();
}

void QGE_GPU_Framework::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}


void QGE_GPU_Framework::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void QGE_GPU_Framework::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

		if (!mCamera.IsMouseCameraControlLocked()) {
			mCamera.Pitch(dy);
			mCamera.RotateY(dx);
		}
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void QGE_GPU_Framework::OnResize()
{
	QGE_Core::OnResize();

	mCamera.SetLens(0.25f*MathHelper::Pi, AspectRatio(), Z_NEAR, Z_FAR);

	BuildTextureDebuggerBuffers();
	BuildOffscreenViews();

	if (pSsao)
		pSsao->OnSize(mClientWidth, mClientHeight, mCamera.GetFovY(), mCamera.GetFarZ());
	if (pSSLR && pSsao)
		pSSLR->OnSize(mClientWidth, mClientHeight, mOffscreenSRV, pSsao->GetNormalDepthSRV(), mWSPositionMapSRV, mSpecularMapSRV);
	if (pSPH)
		pSPH->OnSize(mClientHeight, mClientWidth);
	if (pOIT_PPLL)
		pOIT_PPLL->OnSize(mClientHeight, mClientWidth);
	if (pOIT && pOIT_PPLL)
		pOIT->OnSize(pOIT_PPLL->GetHeadIndexBufferSRV(), pOIT_PPLL->GetFragmentsListBufferSRV(), mClientHeight, mClientWidth);
	if (pVolFX && pGI)
		pVolFX->OnSize(mClientHeight, mClientWidth);
	if (pSVO && pSsao)
		pSVO->OnSize(pSsao->GetNormalDepthSRV(), mClientWidth, mClientHeight);
}

////////////////////////////////Particle system/////////////////////////////////////////////////////

void QGE_GPU_Framework::ParticleSystemDrawRefraction(Camera &camera)
{
	ID3D11RenderTargetView* curRenderTargets[1];
	ID3D11UnorderedAccessView* oitUnorderedAccessViews[2];
	oitUnorderedAccessViews[0] = (pOIT_PPLL ? pOIT_PPLL->GetHeadIndexBufferUAV() : NULL);
	oitUnorderedAccessViews[1] = (pOIT_PPLL ? pOIT_PPLL->GetFragmentsListBufferUAV() : NULL);

	md3dImmediateContext->RSSetViewports(1, &ParticleSystem::GetRefractionViewport());

	md3dImmediateContext->ClearRenderTargetView(ParticleSystem::GetRefractionRTV(), reinterpret_cast<const float*>(&Colors::Black));
	md3dImmediateContext->ClearDepthStencilView(ParticleSystem::GetRefractionDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	curRenderTargets[0] = ParticleSystem::GetRefractionRTV();

	mDrawParticleSystemRefraction = true;
	Draw(camera, -1, curRenderTargets, 1, oitUnorderedAccessViews, 2, 0, ParticleSystem::GetRefractionDSV(), DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_COLOR);
	mDrawParticleSystemRefraction = false;

	md3dImmediateContext->GenerateMips(ParticleSystem::GetRefractionSRV());
}
/////////////Planar water surface/////////////////////////////////////////////////////////////////////////////////////
void QGE_GPU_Framework::WaterDrawReflection(Camera &camera)
{
	if (pPlanarWaterSurface)
	{
		ID3D11RenderTargetView* curRenderTargets[1];
		ID3D11UnorderedAccessView* oitUnorderedAccessViews[2];
		oitUnorderedAccessViews[0] = (pOIT_PPLL ? pOIT_PPLL->GetHeadIndexBufferUAV() : NULL);
		oitUnorderedAccessViews[1] = (pOIT_PPLL ? pOIT_PPLL->GetFragmentsListBufferUAV() : NULL);

		pPlanarWaterSurface->Update(camera);

		md3dImmediateContext->RSSetViewports(1, &pPlanarWaterSurface->GetReflectionViewport());

		md3dImmediateContext->ClearRenderTargetView(pPlanarWaterSurface->GetWaterReflectionRTV(), reinterpret_cast<const float*>(&Colors::Silver));
		md3dImmediateContext->ClearDepthStencilView(pPlanarWaterSurface->GetWaterReflectionDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		curRenderTargets[0] = pPlanarWaterSurface->GetWaterReflectionRTV();

		pPlanarWaterSurface->SetDrawReflectionFlag(true);
		Draw(pPlanarWaterSurface->GetReflectionCamera(), pPlanarWaterSurface->GetWaterMeshIndex(), curRenderTargets, 1, oitUnorderedAccessViews, 2, 0, pPlanarWaterSurface->GetWaterReflectionDSV(),
			DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_COLOR);
		pPlanarWaterSurface->SetDrawReflectionFlag(false);

		md3dImmediateContext->GenerateMips(pPlanarWaterSurface->GetWaterReflectionSRV());
	}
}

void QGE_GPU_Framework::WaterDrawRefraction(Camera &camera)
{
	if (pPlanarWaterSurface)
	{
		ID3D11RenderTargetView* curRenderTargets[1];
		ID3D11UnorderedAccessView* oitUnorderedAccessViews[2];
		oitUnorderedAccessViews[0] = (pOIT_PPLL ? pOIT_PPLL->GetHeadIndexBufferUAV() : NULL);
		oitUnorderedAccessViews[1] = (pOIT_PPLL ? pOIT_PPLL->GetFragmentsListBufferUAV() : NULL);

		md3dImmediateContext->RSSetViewports(1, &pPlanarWaterSurface->GetRefractionViewport());

		md3dImmediateContext->ClearRenderTargetView(pPlanarWaterSurface->GetWaterRefractionRTV(), reinterpret_cast<const float*>(&Colors::Silver));
		md3dImmediateContext->ClearDepthStencilView(pPlanarWaterSurface->GetWaterRefractionDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		curRenderTargets[0] = pPlanarWaterSurface->GetWaterRefractionRTV();

		pPlanarWaterSurface->SetDrawRefractionFlag(true);
		Draw(camera, pPlanarWaterSurface->GetWaterMeshIndex(), curRenderTargets, 1, oitUnorderedAccessViews, 2, 0, pPlanarWaterSurface->GetWaterRefractionDSV(),
			DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_COLOR);
		pPlanarWaterSurface->SetDrawRefractionFlag(false);

		md3dImmediateContext->GenerateMips(pPlanarWaterSurface->GetWaterRefractionSRV());
	}
}

void QGE_GPU_Framework::WaterDrawReflectionAndRefraction(Camera &cam)
{
	WaterDrawRefraction(cam);
	WaterDrawReflection(cam);
}

/////////////GUI/////////////////////////////////////////////////////////////////////////////////////
void QGE_GPU_Framework::DrawGUI() {
	if (pGUI)
		pGUI->DrawGUI(D2DRenderTarget);
}

void QGE_GPU_Framework::RenderD2DGUIdataToTexture()
{
	if (pGUI)
	{
		//Release the D3D 11 Device
		keyedMutex11->ReleaseSync(0);

		//Use D3D10.1 device
		keyedMutex10->AcquireSync(0, 5);

		//Draw D2D content		
		D2DRenderTarget->BeginDraw();

		DrawGUI();

		D2DRenderTarget->EndDraw();

		//Release the D3D10.1 Device
		keyedMutex10->ReleaseSync(1);

		//Use the D3D11 Device
		keyedMutex11->AcquireSync(1, 5);
	}
}

void QGE_GPU_Framework::D2DGUIDraw(ID3D11RenderTargetView** renderTargets,
	ID3D11DepthStencilView* depthStencilView)
{
	if (pGUI)
	{
		md3dImmediateContext->RSSetViewports(1, &mScreenViewport);
		md3dImmediateContext->OMSetRenderTargets(1, renderTargets, depthStencilView);

		md3dImmediateContext->IASetInputLayout(pMeshManager->GetInputLayout());
		md3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		const UINT stride = sizeof(MeshVertex);
		const UINT offset = 0;

		XMFLOAT4X4 Identity;
		XMStoreFloat4x4(&Identity, I);

		D3DX11_TECHNIQUE_DESC techDesc;

		mDrawScreenQuadTech->GetDesc(&techDesc);
		for (int p = 0; p < techDesc.Passes; ++p)
		{
			//Draw to 2D texture
			RenderD2DGUIdataToTexture();

			//Tile texture over a rectangle
			md3dImmediateContext->IASetVertexBuffers(0, 1, &(QGE_Core::d2dVertBuffer), &stride, &offset);
			md3dImmediateContext->IASetIndexBuffer(QGE_Core::d2dIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
			md3dImmediateContext->RSSetState(pRenderStates->NoCullRS);
			md3dImmediateContext->OMSetBlendState(pRenderStates->TransparentBS, NULL, 0xffffff);

			mfxWorld->SetMatrix(reinterpret_cast<float*>(&Identity));
			mfxView->SetMatrix(reinterpret_cast<const float*>(&Identity));
			mfxProj->SetMatrix(reinterpret_cast<float*>(&Identity));
			mfxWorldInvTranspose->SetMatrix(reinterpret_cast<float*>(&Identity));
			mfxWorldViewProj->SetMatrix(reinterpret_cast<float*>(&Identity));
			mfxViewProj->SetMatrix(reinterpret_cast<float*>(&Identity));
			TexTransform->SetRawValue(&Identity, 0, sizeof(I));
			TexDiffuseMapArray->SetResource(QGE_Core::d2dTexture);

			mDrawScreenQuadTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
			md3dImmediateContext->DrawIndexed(6, 0, 0);
		}
		//Reset all
		md3dImmediateContext->RSSetState(0);
		md3dImmediateContext->OMSetBlendState(0, 0, 0xffffffff);
		md3dImmediateContext->OMSetDepthStencilState(0, 0);
	}
}

void QGE_GPU_Framework::D2DGUIBuildBuffers()
{
	GeometryGenerator::MeshData quad;

	GeometryGenerator geoGen;
	geoGen.CreateFullscreenQuad(quad);

	std::vector<MeshVertex> vertices(quad.Vertices.size());

	for (int i = 0; i < quad.Vertices.size(); ++i)
	{
		vertices[i].Pos = quad.Vertices[i].Position;
		vertices[i].Normal = quad.Vertices[i].Normal;
		vertices[i].TexCoord = quad.Vertices[i].TexC;
	}

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(MeshVertex)* quad.Vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &vertices[0];
	HR(md3dDevice->CreateBuffer(&vbd, &vinitData, &(QGE_Core::d2dVertBuffer)));

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(int)* quad.Indices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &quad.Indices[0];
	HR(md3dDevice->CreateBuffer(&ibd, &iinitData, &(QGE_Core::d2dIndexBuffer)));
}

////////SPH/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void QGE_GPU_Framework::SPHFluidDraw(Camera &cam, DrawPassType dpt, ID3D11RenderTargetView** renderTargets, ID3D11DepthStencilView* depthStencilView, UINT meshNotDrawIdx)
{
	if (pSPH && meshNotDrawIdx != -2)
	{
		if (dpt == DrawPassType::DRAW_PASS_TYPE_SHADOW_MAP_BUILD)
		{
			if (mEnableShadows)
				;
			else
				return;
		}
		mfxEnvMap->SetResource(pEnvMap->GetCubeMapSRV());
		mfxReflectionType->SetInt(ReflectionType::REFLECTION_TYPE_NONE);
		mfxRefractionType->SetInt(RefractionType::REFRACTION_TYPE_PLANAR);
		mfxPlanarRefractionMap->SetResource(pSPH->GetPlanarRefractionMapSRV());
		Material WaterMtl = Materials::Liquid_Water * Materials::Color_lightgrey;
		mfxMaterial->SetRawValue(&WaterMtl, 0, sizeof(WaterMtl));

		pSPH->Draw(cam.ViewProj(), XMMatrixTranslation(0.0f, 0.0f, 0.0f), cam.View(), cam.Proj(), depthStencilView, renderTargets, dpt);

		md3dImmediateContext->RSSetState(0);
		md3dImmediateContext->OMSetBlendState(0, 0, 0xffffffff);
		md3dImmediateContext->OMSetDepthStencilState(0, 0);
	}
}

void QGE_GPU_Framework::SPHFluidDrawRefraction(Camera &cam)
{
	if (pSPH)
	{
		ID3D11RenderTargetView* curRenderTargets[1];
		ID3D11UnorderedAccessView* oitUnorderedAccessViews[2];
		oitUnorderedAccessViews[0] = (pOIT_PPLL ? pOIT_PPLL->GetHeadIndexBufferUAV() : NULL);
		oitUnorderedAccessViews[1] = (pOIT_PPLL ? pOIT_PPLL->GetFragmentsListBufferUAV() : NULL);

		md3dImmediateContext->RSSetViewports(1, &pSPH->GetPlanarRefractionMapVP());
		md3dImmediateContext->ClearRenderTargetView(pSPH->GetPlanarRefractionMapRTV(), reinterpret_cast<const float*>(&Colors::Silver));
		md3dImmediateContext->ClearDepthStencilView(pSPH->GetPlanarRefractionMapDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		curRenderTargets[0] = pSPH->GetPlanarRefractionMapRTV();
		Draw(cam, -2, curRenderTargets, 1, oitUnorderedAccessViews, 2, 0, pSPH->GetPlanarRefractionMapDSV(), DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_COLOR);
		md3dImmediateContext->GenerateMips(pSPH->GetPlanarRefractionMapSRV());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QGE_GPU_Framework::QGE_GPU_Framework(HINSTANCE hInstance) : QGE_Core(hInstance),
/*Camera settings*/
mfxEyePosW(0), mWalkCamMode(false),
/*Lighting*/
mDirLightsNum(1), mSpotLightsNum(0), mPointLightsNum(0),
/*Meshes initialization*/
pMeshManager(0),
/*Techniques*/
mBasicTech(0), mTessTech(0),
/*Time*/
mTimeSpeed(0.1f), mfxTime(0), mTime(0),
/*Offscreen RTs*/
mOffscreenUAV(0), mOffscreenSRV(0), mOffscreenRTV(0),
/*Debug texture mode*/
mEnableDebugTextureMode(false), mScreenQuadIB(0), mScreenQuadVB(0),
/*Blending and clipping*/
mUseBlackClippingAtNonBlend(false), mUseAlphaClippingAtNonBlend(false), mUseBlackClippingAtAddBlend(true),
mUseAlphaClippingAtAddBlend(true), mUseBlackClippingAtBlend(false), mUseAlphaClippingAtBlend(false),
/*Teselation*/
mMaxTessFactor(10.0f), mMaxLODdist(1.0f), mMinLODdist(1000.0f), mConstantLODTessFactor(4.0f), mEnableDynamicLOD(false),
/*Enviroment mapping*/
mEnvMapDraw(true), pEnvMap(0),
/*Reflection/refraction*/
mfxReflectionType(0), mfxRefractionType(0), mApproxSceneSz(10),
/*Matrices*/
mfxInvWorld(0),
/*Particle system*/
mDrawParticleSystemRefraction(false),
/*Terrain*/
pTerrain(0), mTerrainDraw(false),
/*Water*/
pPlanarWaterSurface(0),
/*Bump mapping*/
mHeightScale(0.07),
/*Shadow mapping*/
mfxDirLightSplitDistances(0), mfxDirLightShadowMapArray(0), mfxDirLightShadowTransformArray(0), mfxDirLightIndex(0),
mfxSpotLightShadowMapArray(0), mfxSpotLightShadowTransformArray(0), mfxSpotLightIndex(0),
mfxPointLightCubicShadowMapArray(0), mfxPointLightIndex(0),
mfxShadowType(0), pSM(0), mEnableShadows(false),
/*Ssao*/
pSsao(0), mEnableSsao(false),
/*SSLR*/
pSSLR(0), mWSPositionMapRTV(0), mWSPositionMapSRV(0), mEnableSSLR(false),
/*SPH*/
pSPH(0),
/*OIT*/
pOIT(0), pOIT_PPLL(0),
/*Other*/
I(XMMatrixIdentity()),
/*Pure offscreen RTs and SRVs*/
mSpecularMapRTV(0), mSpecularMapSRV(0), mAmbientMapRTV(0), mAmbientMapSRV(0), mDiffuseMapRTV(0), mDiffuseMapSRV(0),
/*Physics*/
pGameLogic(0),
/*Global illumination*/
pGI(0), mEnableGI(false),
/*Volumetric effects*/
pVolFX(0), mEnableVolFX(false),
/*Frustum culling*/
mEnableFrustumCulling(false),
/*GPU collision*/
pGpuCollision(0), mEnablePhysics(false),
/*SVO*/
pSVO(0), mEnableRT(false)
{
	 mLastMousePos.x = 0;
	 mLastMousePos.y = 0;

	XMStoreFloat4x4(&mView, I);
	XMStoreFloat4x4(&mProj, I);
	//TODO: Add here lights initialization code////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Example
	mDirLight[0].Ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	mDirLight[0].Diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
	mDirLight[0].Specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mDirLight[0].Direction = XMFLOAT3(-0.2f, -1.5f, 0.5f);

	mDirLight[1].Ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	mDirLight[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mDirLight[1].Specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mDirLight[1].Direction = XMFLOAT3(0.57735f, -0.57735f, -0.57735f);

	mDirLight[2].Ambient = XMFLOAT4(0.05f, 0.0555f, 0.125f, 1.0f);
	mDirLight[2].Diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mDirLight[2].Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mDirLight[2].Direction = XMFLOAT3(0.0f, -0.7f, 0.3f);

	mSpotLight[0].Ambient = XMFLOAT4(0.025f, 0.025f, 0.025f, 1.0f);
	mSpotLight[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	mSpotLight[0].Specular = XMFLOAT4(0.01f, 0.01f, 0.01f, 1.0f);
	mSpotLight[0].Att = XMFLOAT3(0.0f, 0.01f, 0.0f);
	mSpotLight[0].Spot = 20.0f;
	mSpotLight[0].Range = 2000.0f;
	mSpotLight[0].Direction = XMFLOAT3(1.0f, 0.0f, -2.0f);
	mSpotLight[0].Position = XMFLOAT3(149, 66, 651);

	mSpotLight[1].Ambient = XMFLOAT4(0.05f, 0.05f, 0.05f, 1.0f);
	mSpotLight[1].Diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mSpotLight[1].Specular = XMFLOAT4(0.01f, 0.01f, 0.01f, 1.0f);
	mSpotLight[1].Att = XMFLOAT3(0.0f, 0.9f, 0.0f);
	mSpotLight[1].Spot = 10.0f;
	mSpotLight[1].Range = 2000.0f;
	mSpotLight[1].Direction = XMFLOAT3(-1.0f, -1.0f, 0.0f);
	mSpotLight[1].Position = XMFLOAT3(-28.0f, 14.9f, 139.0f);
	mSpotLight[1].Range = 300;

	mSpotLight[2].Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mSpotLight[2].Diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mSpotLight[2].Specular = XMFLOAT4(0.01f, 0.01f, 0.01f, 1.0f);
	mSpotLight[2].Att = XMFLOAT3(0.0f, 0.9f, 0.0f);
	mSpotLight[2].Spot = 10.0f;
	mSpotLight[2].Range = 2000.0f;
	mSpotLight[2].Direction = XMFLOAT3(0.0f, -1.0f, -1.0f);
	mSpotLight[2].Position = XMFLOAT3(-38.0f, 14.9f, 159.0f);
	mSpotLight[2].Range = 300;

	mPointLight[0].Ambient = XMFLOAT4(0.075f, 0.05f, 0.025f, 1.0f);
	mPointLight[0].Diffuse = XMFLOAT4(0.8863f, 0.345f, 0.133f, 1.0f);
	mPointLight[0].Specular = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
	mPointLight[0].Att = XMFLOAT3(0.0f, 0.0f, 0.09f);
	mPointLight[0].Range = 30.0f;
	mPointLight[0].Position = XMFLOAT3(40.0f, 20.0f, 40.0f);

	mPointLight[1].Ambient = XMFLOAT4(0.075f, 0.05f, 0.025f, 1.0f);
	mPointLight[1].Diffuse = XMFLOAT4(0.8863f, 0.345f, 0.133f, 1.0f);
	mPointLight[1].Specular = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
	mPointLight[1].Att = XMFLOAT3(0.0f, 0.09f, 0.0f);
	mPointLight[1].Range = 1000.0f;
	mPointLight[1].Position = XMFLOAT3(-50.0f, 14.9f, 139.0f);

	mPointLight[2].Ambient = XMFLOAT4(0.075f, 0.05f, 0.025f, 1.0f);
	mPointLight[2].Diffuse = XMFLOAT4(0.8863f, 0.345f, 0.133f, 1.0f);
	mPointLight[2].Specular = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
	mPointLight[2].Att = XMFLOAT3(0.0f, 0.09f, 0.0f);
	mPointLight[2].Range = 1000.0f;
	mPointLight[2].Position = XMFLOAT3(-50.0f, 14.9f, 119.0f);
}

QGE_GPU_Framework::~QGE_GPU_Framework()
{

	md3dImmediateContext->ClearState();

	ReleaseCOM(mOffscreenRTV);
	ReleaseCOM(mOffscreenSRV);
	ReleaseCOM(mOffscreenUAV);
	ReleaseCOM(mScreenQuadIB);
	ReleaseCOM(mScreenQuadVB);

	ReleaseCOM(mFX);
}

bool QGE_GPU_Framework::Init()
{
	if (!QGE_Core::Init())
		return false;

	static const std::vector<std::wstring> nullvec(0);

	BuildFX();
	BuildTextureDebuggerBuffers();
	D2DGUIBuildBuffers();
	ParticleSystem::BuildRefractionViews(md3dDevice);
	BuildOffscreenViews();
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//TODO: add or tweak graphics engine modules here//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///Shadow mapping - handles direct, point and spot lights shadows
	pSM = new ShadowMapping(md3dDevice, SHADOW_MAP_SIZE);
	if (pSM) {
		for (int i = 0; i < mDirLightsNum; ++i)
			pSM->UpdateDirLightMatrices(mDirLight[i], mCamera, i);
		for (int i = 0; i < mSpotLightsNum; ++i)
			pSM->UpdateSpotLightMatrices(mSpotLight[i], mCamera, XM_PI / 2, 1.0f, 1.0f, i);
		for (int i = 0; i < mPointLightsNum; ++i)
			pSM->UpdatePointLightMatrices(mPointLight[i], mCamera, 1.0f, i);
	}
	///SSAO (screen-space ambient occlusion)
	pSsao = new Ssao(md3dDevice, md3dImmediateContext, mClientWidth, mClientHeight, mCamera.GetFovY(), mCamera.GetFarZ());
	///SSLR (screen-space local reflections) - screen-space raytracer able to produce correct reflections
	//pSSLR = new SSLR(md3dDevice, md3dImmediateContext, mClientWidth, mClientHeight, mOffscreenSRV, pSsao->GetNormalDepthSRV(),
	//	mWSPositionMapSRV, mSpecularMapSRV, true, 4, WEIGHTING_MODE_GLOSS, true);
	///Rendering states
	pRenderStates = new RenderStates(md3dDevice);
	///Meshes manager - does all work with meshes handling
	pMeshManager = new MeshManager(md3dDevice, md3dImmediateContext, mBasicTech, MESH_CUBE_MAP_SZ, MESH_PLANAR_REFRACTION_MAP_SZ);
	///SPH - simulates incompressible liquid
	//std::map<DrawSceneType, XMFLOAT2>RTsSizes;
	//RTsSizes.insert(make_pair(DrawSceneType::DRAW_SCENE_TO_SPH_REFRACTION_RT, XMFLOAT2(SPH_REFRACTION_MAP_SZ, SPH_REFRACTION_MAP_SZ)));
	//RTsSizes.insert(make_pair(DrawSceneType::DRAW_SCENE_TO_SHADOWMAP_CASCADE_RT, XMFLOAT2(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE)));
	//pSPH = new SPH(md3dDevice, md3dImmediateContext, mFX, mfxWorldViewProj, mfxWorld, mfxView, mfxProj, RTsSizes, mClientHeight, mClientWidth);
	
	///OIT (order-independent transparency) - allows drawing transparent meshes without any API overhead during meshes creation;
	///BOTH "PPLL" class and "OIT" class instances ARE REQUIRED! 
	//pOIT_PPLL = new PPLL(md3dDevice, md3dImmediateContext, mFX, mClientHeight, mClientWidth);
	//pOIT = new OIT(md3dDevice, md3dImmediateContext, pOIT_PPLL->GetHeadIndexBufferSRV(), pOIT_PPLL->GetFragmentsListBufferSRV(), mClientHeight, mClientWidth);
	///Global illumination - simulates indirect illumination using cascaded voxel grids and virtual point lights (VPL) propagation technique
	pGI = new GlobalIllumination(md3dDevice, md3dImmediateContext, mFX, mCamera);
	///Volumetric effects - simulates effects connected with light transport through medium with specified density distribution;
	///uses cascaded voxel grids for compuattion speed imporvement, DOES NOT WORK WITHOUT instance of "GlobalIllumination" class!
	pVolFX = new VolumetricEffect(md3dDevice, md3dImmediateContext, mClientHeight, mClientWidth, pGI->GetGridCenterPosW(), pGI->GetInvertedGridCellSizes(), pGI->GetGridCellSizes(), mCamera);
	///GPU collision - simulates collision on GPU
	//pGpuCollision = new GpuCollision(md3dDevice, md3dImmediateContext, mFX, pMeshManager);
	///Environment map
	pEnvMap = new EnvironmentMap(md3dDevice, md3dImmediateContext, L"Textures\\sky.dds", 5000.0f);
	///SVO pipeline
	pSVO = new SvoPipeline(md3dDevice, md3dImmediateContext, mFX, pSsao->GetNormalDepthSRV(), pEnvMap->GetCubeMapSRV(), mClientWidth, mClientHeight);
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//TODO: Add 2D GUI elements connected with graphic settings here///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Example:
	std::wstring caption = L"";
	Alignment align;
	align.FontTextAlignment = DWRITE_TEXT_ALIGNMENT_LEADING;
	align.FontTextParagraphAligment = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
	Alignment align1;
	align1.FontTextAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
	align1.FontTextParagraphAligment = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
	BrushInit bi;
	bi.BrushColor = D2D1::ColorF(0.0f, 0.7f, 0.0f, 1.0f);
	bi.BrushStyle = BrushType::SOLID_COLOR;
	BrushInit rbi1;
	rbi1.BrushColor = D2D1::ColorF(0.0f, 0.7f, 0.0f, 1.0f);
	rbi1.BrushStyle = BrushType::SOLID_COLOR;
	BrushInit rbi2;
	rbi2.BrushColor = D2D1::ColorF(0.0f, 0.0f, 1.0f, 1.0f);
	rbi2.BrushStyle = BrushType::SOLID_COLOR;
	FontInit fi;
	fi.FontCollection = NULL;
	fi.FontFamilyName = L"Times New Roman";
	fi.FontLocaleName = L"en-us";
	fi.fontSize = 15.0f;
	fi.FontStretch = DWRITE_FONT_STRETCH_NORMAL;
	fi.FontStyle = DWRITE_FONT_STYLE_NORMAL;
	fi.FontWeight = DWRITE_FONT_WEIGHT_LIGHT;

	pGUI->CreateTextBox(D2DRenderTarget, L"                                                           ", -1, -1, XMFLOAT2(820, 520.0f), align, bi, fi, false, L"Help");
	pGUI->CreateTextBox(D2DRenderTarget, L"                                                           ", -1, -1, XMFLOAT2(10.0f, 20.0f), align, bi, fi, false, L"Info");

	pGUI->CreateTextBox(D2DRenderTarget, L"Directional lights count:", -1, -1, XMFLOAT2(10.0f, 60.0f), align, bi, fi, false, L"dl");
	pGUI->CreateButton(D2DRenderTarget, L"-", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(170.0f, 60.0f), align1, bi, fi, false, true, L"-dl");
	pGUI->CreateTextBox(D2DRenderTarget, L"0", -1, -1, XMFLOAT2(190.0f, 60.0f), align1, bi, fi, false, L"Directional lights num");
	pGUI->CreateButton(D2DRenderTarget, L"+", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(210.0f, 60.0f), align1, bi, fi, false, true, L"+dl");

	pGUI->CreateTextBox(D2DRenderTarget, L"Point lights count:      ", -1, -1, XMFLOAT2(10.0f, 100.0f), align, bi, fi, false, L"pl");
	pGUI->CreateButton(D2DRenderTarget, L"-", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(170.0f, 100.0f), align1, bi, fi, false, true, L"-pl");
	pGUI->CreateTextBox(D2DRenderTarget, L"0", -1, -1, XMFLOAT2(190.0f, 100.0f), align1, bi, fi, false, L"Point lights num");
	pGUI->CreateButton(D2DRenderTarget, L"+", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(210.0f, 100.0f), align1, bi, fi, false, true, L"+pl");

	pGUI->CreateRadioButton(D2DRenderTarget, L"Frustum culling", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(40.0f, 140.0f), align, rbi1, fi, rbi2, fi, false, true, L"Frustum culling");
	pGUI->CreateRadioButton(D2DRenderTarget, L"Volumetric FX", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(40.0f, 180.0f), align, rbi1, fi, rbi2, fi, false, true, L"Volumetric FX");
	pGUI->CreateRadioButton(D2DRenderTarget, L"Global illumination", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(40.0f, 220.0f), align, rbi1, fi, rbi2, fi, false, true, L"Global illumination");
	pGUI->CreateRadioButton(D2DRenderTarget, L"Debug texture", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(40.0f, 260.0f), align, rbi1, fi, rbi2, fi, false, true, L"Debug texture");
	pGUI->CreateRadioButton(D2DRenderTarget, L"SSAO", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(40.0f, 300.0f), align, rbi1, fi, rbi2, fi, false, true, L"SSAO");
	pGUI->CreateRadioButton(D2DRenderTarget, L"SSLR", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(40.0f, 340.0f), align, rbi1, fi, rbi2, fi, false, true, L"SSLR");
	pGUI->CreateRadioButton(D2DRenderTarget, L"SVO ray tracing", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(40.0f, 380.0f), align, rbi1, fi, rbi2, fi, false, true, L"SVORT");
	pGUI->CreateRadioButton(D2DRenderTarget, L"Shadows", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(40.0f, 420.0f), align, rbi1, fi, rbi2, fi, false, true, L"Shadows");
	pGUI->CreateRadioButton(D2DRenderTarget, L"Terrain", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(40.0f, 460.0f), align, rbi1, fi, rbi2, fi, false, true, L"Terrain");
	pGUI->CreateRadioButton(D2DRenderTarget, L"Physics", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(40.0f, 500.0f), align, rbi1, fi, rbi2, fi, false, true, L"Physics");
	pGUI->CreateRadioButton(D2DRenderTarget, L"Camera walk mode", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(40.0f, 540.0f), align, rbi1, fi, rbi2, fi, false, true, L"Camera walk mode");
	pGUI->CreateRadioButton(D2DRenderTarget, L"Camera mouse control", -1, -1, ButtonType::ELLIPSOID, XMFLOAT2(40.0f, 580.0f), align, rbi1, fi, rbi2, fi, false, true, L"Camera mouse control");
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//TODO: Add terrain init code here/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Example:
	InitInfo tii;
	tii.HeightMapFilename = L"Textures/terrain.raw";
	tii.LayerMapFilename0 = L"Textures/grass.dds";
	tii.LayerNormalMapFilename0 = L"Textures/grassnormal.dds";
	tii.LayerMapFilename1 = L"Textures/darkdirt.dds";
	tii.LayerNormalMapFilename1 = L"Textures/dirtnormal.dds";
	tii.LayerMapFilename2 = L"Textures/stone.dds";
	tii.LayerNormalMapFilename2 = L"Textures/stonenormal.dds";
	tii.LayerMapFilename3 = L"Textures/lightdirt.dds";
	tii.LayerNormalMapFilename3 = L"Textures/dirtnormal.dds";
	tii.LayerMapFilename4 = L"Textures/snow.dds";
	tii.LayerNormalMapFilename4 = L"Textures/snownormal.dds";
	tii.BlendMapFilename = L"Textures/blend.dds";
	tii.HeightScale = 400.0f;
	tii.HeightmapWidth = 2049;
	tii.HeightmapHeight = 2049;
	tii.CellSpacing = 2.0f;
	///Terrain - oblidged with heightmap-based terrain rendering and handling
	//pTerrain = new Terrain(md3dDevice, md3dImmediateContext, tii, mFX);
	//TODO: planar water surface init code here////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Example:
	///Planar refractive/reflective water;
	///REQUIRES "MeshManager" class instance!
	//pPlanarWaterSurface = new PlanarWaterSurface(md3dDevice, mFX, pMeshManager, 6.0f, 256, XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(3200.0f, 3200.0f));
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//TODO: Set initial camera position here///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Example
	XMVECTOR CamPos = XMVectorSet(20.0f, 40.0f, -80.0f, 1.0f);
	mCamera.SetPosition(XMVectorGetX(CamPos), XMVectorGetY(CamPos), XMVectorGetZ(CamPos));
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//TODO: Add particle system initialization code here///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Example:
	/*std::vector<std::wstring> raindrops(0);
	std::vector<std::wstring> raindropsnormal(0);
	raindrops.push_back(L"Textures\\raindrops.jpg");
	raindropsnormal.push_back(L"Textures\\waterdropnormal.dds");
	ParticleSystemInitData RAIN[] = {
		{ true, mTimer.TotalTime(), mTimer.DeltaTime(), 0, 1000000, XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMFLOAT3(0.0f, 0.0f, 0.0f), true, true, true, L"FX\\Rain.cso", raindropsnormal, raindrops }
	};
	ParticleSystemInitNew(md3dDevice, RAIN);*/
	/*std::vector<std::wstring> raindrops(0);
	std::vector<std::wstring> raindropsnormal(0);
	raindrops.push_back(L"Textures\\raindrops.jpg");
	raindropsnormal.push_back(L"Textures\\waterdropnormal.dds");
	ParticleSystemInitData RAIN[] = {
		{ true, mTimer.TotalTime(), mTimer.DeltaTime(), 0, 1000000, XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMFLOAT3(0.0f, 0.0f, 0.0f), true, true, true, L"FX\\Rain.cso", raindropsnormal, raindrops }
	};
	ParticleSystems.insert({ "RAIN", new ParticleSystem(md3dDevice, md3dImmediateContext, RAIN) });*/
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//TODO: Add mesh creation code here////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Example:
	XMMATRIX carWorld = XMMatrixTranslation(-800, 0, -200) * XMMatrixScaling(0.09f, 0.09f, 0.09f) * XMMatrixRotationY(-XM_PI / 2.0f);
	pMeshManager->CreateFbxScene(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, BuffersOptimization::BUFFERS_OPTIMIZATION_DISABLE, DrawType::INDEXED_DRAW, &carWorld, mBasicTech, "Models\\sponza.fbx",
		"Models\\sponza.fbx", false, false, 1, ReflectionType::REFLECTION_TYPE_SVO_CONE_TRACING, RefractionType::REFRACTION_TYPE_NONE);
	
	/*XMMATRIX gridWorld = I;
	XMMATRIX gridTexTransform = XMMatrixScaling(5.0f, 5.0f, 1.0f);
	std::vector<std::wstring>GridTexFilenames = {L"Textures\\floor.jpg"};
	pMeshManager->CreateGrid(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, DrawType::INDEXED_DRAW, &gridTexTransform, &gridWorld, mBasicTech,
		GridTexFilenames, static_cast<std::vector<std::wstring>>(nullvec), 1, true, false, false, false, false, false, 1,
		ReflectionType::REFLECTION_TYPE_SVO_CONE_TRACING, RefractionType::REFRACTION_TYPE_NONE,	Materials::Plastic_Cellulose, 200.0f, 200.0f, 100, 100);*/
	
	/*XMMATRIX sphereWorld = XMMatrixTranslation(-50, 15, -50);
	XMMATRIX sphereTexTransform = I;
	pMeshManager->CreateSphere(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, DrawType::INDEXED_DRAW, &sphereTexTransform, &sphereWorld, mBasicTech,
		static_cast<std::vector<std::wstring>>(nullvec), static_cast<std::vector<std::wstring>>(nullvec), 0, false, false, false, false, false, false, 1,
		ReflectionType::REFLECTION_TYPE_SVO_CONE_TRACING, RefractionType::REFRACTION_TYPE_NONE, Materials::Metal_Silver * Materials::Color_grey, 15.0f, 15, 15);*/

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//TODO: Add here buffers mapping code( map created buffers to get data stored in it):////////////////////////////////////////////////////////////////////////////////////////////////
	//Example:
	//Get vertices( described by Meshes[0])
	//if (pMeshManager->GetMeshCount() > 0)
	//{
	//	MeshVertex *verts = new MeshVertex[pMeshManager->GetMeshVertexCount(0)];
	//	pMeshManager->GetMeshVertexBuffer(0, &verts[0]);
	//	//Get indices( described by Meshes[0])
	//	int *inds = new int[pMeshManager->GetMeshIndexCount(0)];
	//	pMeshManager->GetMeshIndexBuffer(0, &inds[0]);
	//	//Set vertices( described by Meshes[0])
	//	pMeshManager->SetMeshVertexBuffer(0, &verts[0]);
	//	//Set indices( described by Meshes[0])
	//	pMeshManager->SetMeshIndexBuffer(0, &inds[0]);
	//	delete[] inds;
	//	delete[] verts;
	//}
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	return true;
}

void QGE_GPU_Framework::UpdateScene(float dt)
{
	//TODO: Add here buttons processing code///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//
	// Control the camera.
	//
	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(mCamera.GetCamSpeed()*dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-mCamera.GetCamSpeed()*dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-mCamera.GetCamSpeed()*dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(mCamera.GetCamSpeed()*dt);

	if (GetAsyncKeyState('0') & 0x8000)
		mCamera.SetCamSpeed(mCamera.GetCamSpeed() * 1.1f);

	if (GetAsyncKeyState('9') & 0x8000)
		mCamera.SetCamSpeed(mCamera.GetCamSpeed() / 1.1f);

	if (GetAsyncKeyState('4') & 0x8000)
		mSpotLight[0].Position.x += 2.0f;
	if (GetAsyncKeyState('5') & 0x8000)
		mSpotLight[0].Position.x -= 2.0f;
	if (GetAsyncKeyState('6') & 0x8000)
		mSpotLight[0].Position.z += 2.0f;
	if (GetAsyncKeyState('7') & 0x8000)
		mSpotLight[0].Position.z -= 2.0f;
	if (GetAsyncKeyState('8') & 0x8000)
		mSpotLight[0].Spot += 2.0f;
	if (GetAsyncKeyState('I') & 0x8000)
		mSpotLight[0].Spot -= 2.0f;

	if ((GetAsyncKeyState('E') & 0x8000) && pSPH)
		pSPH->Drop();

	if (mWalkCamMode && pTerrain)
	{
		XMFLOAT3 camPos = mCamera.GetPosition();
		float y = pTerrain->GetHeight(camPos.x, camPos.z);
		mCamera.SetPosition(camPos.x, y + 3.0f, camPos.z);
	}

	if (pGameLogic) {
		pGameLogic->OnKeyPress();
		if(pGameLogic->GetCameraCarChaseFlag())
			mCamera = pGameLogic->GetBumberChaseCamera();
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	static bool showHelp = false;
	for (; !pGUI->ActionQueue.empty();) {
		//TODO: Add 2D GUI actions proceissing code here///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"Frustum culling")
			InterlockedExchange(reinterpret_cast<LONG*>(&mEnableFrustumCulling), pGUI->GetRadioButtonState(pGUI->ActionQueue[pGUI->ActionQueue.size() - 1]));
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"Volumetric FX")
			InterlockedExchange(reinterpret_cast<LONG*>(&mEnableVolFX), pGUI->GetRadioButtonState(pGUI->ActionQueue[pGUI->ActionQueue.size() - 1]));
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"Debug texture")
			InterlockedExchange(reinterpret_cast<LONG*>(&mEnableDebugTextureMode), pGUI->GetRadioButtonState(pGUI->ActionQueue[pGUI->ActionQueue.size() - 1]));
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"Camera walk mode")
			InterlockedExchange(reinterpret_cast<LONG*>(&mWalkCamMode), pGUI->GetRadioButtonState(pGUI->ActionQueue[pGUI->ActionQueue.size() - 1]));
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"SSAO")
			InterlockedExchange(reinterpret_cast<LONG*>(&mEnableSsao), pGUI->GetRadioButtonState(pGUI->ActionQueue[pGUI->ActionQueue.size() - 1]));
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"SSLR")
			InterlockedExchange(reinterpret_cast<LONG*>(&mEnableSSLR), pGUI->GetRadioButtonState(pGUI->ActionQueue[pGUI->ActionQueue.size() - 1]));
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"SVORT")
			InterlockedExchange(reinterpret_cast<LONG*>(&mEnableRT), pGUI->GetRadioButtonState(pGUI->ActionQueue[pGUI->ActionQueue.size() - 1]));
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"Shadows")
			InterlockedExchange(reinterpret_cast<LONG*>(&mEnableShadows), pGUI->GetRadioButtonState(pGUI->ActionQueue[pGUI->ActionQueue.size() - 1]));
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"Terrain")
			InterlockedExchange(reinterpret_cast<LONG*>(&mTerrainDraw), pGUI->GetRadioButtonState(pGUI->ActionQueue[pGUI->ActionQueue.size() - 1]));
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"Physics")
			InterlockedExchange(reinterpret_cast<LONG*>(&mEnablePhysics), pGUI->GetRadioButtonState(pGUI->ActionQueue[pGUI->ActionQueue.size() - 1]));
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"Global illumination")
			InterlockedExchange(reinterpret_cast<LONG*>(&mEnableGI), pGUI->GetRadioButtonState(pGUI->ActionQueue[pGUI->ActionQueue.size() - 1]));
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"Camera mouse control") {
			if (pGUI->GetRadioButtonState(pGUI->ActionQueue[pGUI->ActionQueue.size() - 1]))
				mCamera.UnlockMouseCameraControl();
			else
				mCamera.LockMouseCameraControl();
		}
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"-dl") {
			if (mDirLightsNum > 0)
				mDirLightsNum--;
		}
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"+dl") {
			if (mDirLightsNum < MAX_DIR_LIGHTS_NUM)
				mDirLightsNum++;
		}
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"-pl") {
			if (mPointLightsNum > 0)
				mPointLightsNum--;
		}
		if (pGUI->ActionQueue[pGUI->ActionQueue.size() - 1] == L"+pl") {
			if (mPointLightsNum < MAX_POINT_LIGHTS_NUM)
				mPointLightsNum++;
		}
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		pGUI->ActionQueue.pop_back();
	}
	//TODO: Add 2D GUI update code here/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	static const  std::wstring WalkControls = L"Use W,A,S,D to walk;\n";
	static const  std::wstring CameraControls = L"Use 9,0 to control camera speed;";

	TextInfo = WalkControls + CameraControls;

	wchar_t str[10];
	wsprintf(str, L"%d", mDirLightsNum);
	pGUI->UpdateTextBox(str, L"Directional lights num");

	wsprintf(str, L"%d", mPointLightsNum);
	pGUI->UpdateTextBox(str, L"Point lights num");

	pGUI->UpdateTextBox(AllInfo, L"Info");
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//TODO: Add particle system reset and update code here//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Example
	if (ParticleSystems.find("RAIN") != ParticleSystems.end()) {
		if (GetAsyncKeyState('R') & 0x8000)
		{
			std::vector<std::wstring>nullvec(0);

			ParticleSystemInitData RAIN[] = {
				{ true, mTimer.TotalTime(), mTimer.DeltaTime(), 0, 1000000, XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f), true, true, true, L"FX\\Rain.cso", nullvec, nullvec }
			};
			ParticleSystems["RAIN"]->Reset(RAIN);
		}
		
		//Update emitter position for rain effect
		ParticleSystems["RAIN"]->SetEmitPos(mCamera.GetPosition());

		//Update all particle systems
		ParticleSystems["RAIN"]->Update(dt, mTimer.TotalTime());
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//TODO: Add physics processing function here//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (pSPH)
		pSPH->Compute();
	if (pGameLogic)
		pGameLogic->Update(dt, AspectRatio());
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	mTime += dt;
	mCamera.UpdateViewMatrix();

	if (pSM) {
		pSM->CalculateSplitDistances(mCamera);
		for (int i = 0; i < mDirLightsNum; ++i)
			pSM->UpdateDirLightMatrices(mDirLight[i], mCamera, i);
		for (int i = 0; i < mSpotLightsNum; ++i)
			pSM->UpdateSpotLightMatrices(mSpotLight[i], mCamera, XM_PI / 2, 1.0f, 0.1f, i);
		for (int i = 0; i < mPointLightsNum; ++i)
			pSM->UpdatePointLightMatrices(mPointLight[i], mCamera, 0.1f, i);

		for (int j = 0; j < mDirLightsNum; ++j)
			for (int i = 0; i < CASCADE_COUNT; ++i)
				dirLightCams[i + j * CASCADE_COUNT] = pSM->CreateDirLightCameraFromPlayerCam(mCamera, i, j);
		for (int i = 0; i < mSpotLightsNum; ++i)
			spotLightCams[i] = pSM->CreateSpotLightCameraFromPlayerCam(mCamera, i);
		for (int j = 0; j < mPointLightsNum; ++j)
			for (int i = 0; i < 6; ++i)
				pointLightCams[i + j * 6] = pSM->CreatePointLightCameraFromPlayerCam(mCamera, i, j);
	}
	if(pGI)
		pGI->Update(mCamera);
	if (pVolFX && pGI)
		pVolFX->Update(mCamera, pGI->GetGridCenterPosW(), mTime*mTimeSpeed);
	if (pGpuCollision)
		pGpuCollision->Update(mCamera);
	if (pSVO)
		pSVO->Update(mCamera);
}
void QGE_GPU_Framework::DrawScene()
{
	ID3D11RenderTargetView* renderTargets[1];
	//GPU collision///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (pGpuCollision && mEnablePhysics)
	{
		ID3D11UnorderedAccessView* gpuCollisionUnorderedAccessViews[2];
		pGpuCollision->Setup();
		renderTargets[0] = *(pGpuCollision->GetOffscreenRTV());
		for (int i = 0; i < PARTICLES_GRID_CASCADES_COUNT; ++i)
		{
			gpuCollisionUnorderedAccessViews[0] = (pGpuCollision ? pGpuCollision->GetHeadIndexBuffersUAVs()[i] : NULL);
			gpuCollisionUnorderedAccessViews[1] = (pGpuCollision ? pGpuCollision->GetBodiesPPLLsUAVs()[i] : NULL);
			pGpuCollision->SetCascadeIndex(i);
			md3dImmediateContext->RSSetViewports(1, &pGpuCollision->GetOffscreenRTViewport());
			Draw(pGpuCollision->GetCascadeCamera(i), -1, renderTargets, 1, gpuCollisionUnorderedAccessViews, 2, 0, NULL, DrawShadowType::DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_PARTICLES_GRID);
			md3dImmediateContext->RSSetViewports(1, &mScreenViewport);
		}
		pGpuCollision->Compute(mTimer.DeltaTime() * mTimeSpeed * 10);
	}
	//Shadows/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (pSM) {
		for (int j = 0; j < mDirLightsNum; ++j) {
			for (int i = 0; i < CASCADE_COUNT; ++i) {
				pSM->DirLightShadowMapSetup(md3dImmediateContext, i, j);
				ID3D11RenderTargetView* dirRenderTargets[1] = { pSM->GetDirLightDistanceMapRTV(i, j) };
				if (mEnableShadows)
					Draw(dirLightCams[i + j * CASCADE_COUNT], pPlanarWaterSurface ? pPlanarWaterSurface->GetWaterMeshIndex() : -1, dirRenderTargets, 1, pNullUAV,
						1, 0, pSM->GetDirLightDistanceMapDSV(j),
						DrawShadowType::DRAW_SHADOW_TYPE_DIRECTIONAL_LIGHT, j, DrawPassType::DRAW_PASS_TYPE_SHADOW_MAP_BUILD);
			}
		}
		for (int j = 0; j < mSpotLightsNum; ++j) {
			pSM->SpotLightShadowMapSetup(md3dImmediateContext, j);
			ID3D11RenderTargetView* spotRenderTargets[1] = { pSM->GetSpotLightDistanceMapRTV(j) };
			if (mEnableShadows)
				Draw(spotLightCams[j], pPlanarWaterSurface ? pPlanarWaterSurface->GetWaterMeshIndex() : -1, spotRenderTargets, 1, pNullUAV,
					1, 0, pSM->GetSpotLightDistanceMapDSV(j),
					DrawShadowType::DRAW_SHADOW_TYPE_SPOT_LIGHT, j, DrawPassType::DRAW_PASS_TYPE_SHADOW_MAP_BUILD);
		}
		for (int j = 0; j < mPointLightsNum; ++j) {
			for (int i = 0; i < 6; ++i)
			{
				pSM->PointLightShadowMapSetup(md3dImmediateContext, i, j);
				ID3D11RenderTargetView* pointRenderTargets[1] = { pSM->GetPointLightDistanceMapRTV(i, j) };
				if (mEnableShadows)
					Draw(pointLightCams[i + j * 6], pPlanarWaterSurface ? pPlanarWaterSurface->GetWaterMeshIndex() : -1, pointRenderTargets, 1, pNullUAV,
						1, 0, pSM->GetPointLightDistanceMapDSV(j),
						DrawShadowType::DRAW_SHADOW_TYPE_POINT_LIGHT, j, DrawPassType::DRAW_PASS_TYPE_SHADOW_MAP_BUILD);
			}
		}
	}
	//Ssao////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (mEnableSsao && pSsao || pSVO && pSsao && mEnableRT)
	{
		renderTargets[0] = pSsao->NormalDepthRTV();
		static const float clearColor[] = { 0.0f, 0.0f, -1.0f, 1e5f };
		md3dImmediateContext->ClearRenderTargetView(renderTargets[0], clearColor);
		md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		md3dImmediateContext->RSSetViewports(1, &mScreenViewport);
		Draw(mCamera, -1, renderTargets, 1, pNullUAV, 1, 0, mDepthStencilView, DrawShadowType::DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_NORMAL_DEPTH);

		pSsao->ComputeSsao(mCamera);
		pSsao->BlurAmbientMap(4);
	}
	//SSLR////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (mEnableSSLR && pSSLR || mEnableVolFX && pVolFX)
	{
		renderTargets[0] = mWSPositionMapRTV;
		md3dImmediateContext->ClearRenderTargetView(mWSPositionMapRTV, reinterpret_cast<const float*>(&Colors::Inf));
		md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		md3dImmediateContext->RSSetViewports(1, &mScreenViewport);
		Draw(mCamera, -1, renderTargets, 1, pNullUAV, 1, 0, mDepthStencilView, DrawShadowType::DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_POSITION_WS);
	}

	if (mEnableSSLR && pSSLR)
	{
		ID3D11UnorderedAccessView* oitUnorderedAccessViews[2];
		oitUnorderedAccessViews[0] = (pOIT_PPLL ? pOIT_PPLL->GetHeadIndexBufferUAV() : NULL);
		oitUnorderedAccessViews[1] = (pOIT_PPLL ? pOIT_PPLL->GetFragmentsListBufferUAV() : NULL);
		renderTargets[0] = pSSLR->GetPreConvolutionBufferRTV(0);
		md3dImmediateContext->ClearRenderTargetView(pSSLR->GetPreConvolutionBufferRTV(0), reinterpret_cast<const float*>(&Colors::Black));
		md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		md3dImmediateContext->RSSetViewports(1, &mScreenViewport);
		Draw(mCamera, -1, renderTargets, 1, oitUnorderedAccessViews, 2, 0, mDepthStencilView, DrawShadowType::DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_COLOR_NO_REFLECTION);

		renderTargets[0] = pSSLR->GetHiZBufferRTV(0);
		md3dImmediateContext->ClearRenderTargetView(pSSLR->GetHiZBufferRTV(0), reinterpret_cast<const float*>(&Colors::White));
		md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		md3dImmediateContext->RSSetViewports(1, &mScreenViewport);
		Draw(mCamera, -1, renderTargets, 1, pNullUAV, 1, 0, mDepthStencilView, DrawShadowType::DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_DEPTH);

		if (!mEnableSsao)
		{
			renderTargets[0] = pSsao->NormalDepthRTV();
			static const float clearColor[] = { 0.0f, 0.0f, -1.0f, 1e5f };
			md3dImmediateContext->ClearRenderTargetView(renderTargets[0], clearColor);
			md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
			md3dImmediateContext->RSSetViewports(1, &mScreenViewport);
			Draw(mCamera, -1, renderTargets, 1, pNullUAV, 1, 0, mDepthStencilView, DrawShadowType::DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_NORMAL_DEPTH);
		}

		renderTargets[0] = mSpecularMapRTV;
		md3dImmediateContext->ClearRenderTargetView(mSpecularMapRTV, reinterpret_cast<const float*>(&Colors::Black));
		md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		md3dImmediateContext->RSSetViewports(1, &mScreenViewport);
		Draw(mCamera, -1, renderTargets, 1, pNullUAV, 1, 0, mDepthStencilView, DrawShadowType::DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_MATERIAL_SPECULAR);

		pSSLR->RenderReflections(mCamera.View(), mCamera.Proj(), mCamera.GetNearZ(), mCamera.GetFarZ());
	}
	//Global illumination/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if(pGI && mEnableGI)
	{
		renderTargets[0] = pGI->Get64x64RT();
		md3dImmediateContext->ClearRenderTargetView(renderTargets[0], reinterpret_cast<const float*>(&Colors::Black));
		md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		md3dImmediateContext->RSSetViewports(1, &pGI->Get64x64RTViewport());
		Draw(mCamera, -1, renderTargets, 1, pNullUAV, 1, 0, NULL, DrawShadowType::DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_VOXEL_GRID);
		
		pGI->Compute(pSM, mDirLightsNum, mDirLight, mSpotLightsNum, mSpotLight, mPointLightsNum, mPointLight);
	}
	//Volumetric effects//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (pVolFX && mEnableVolFX && pGI)
	{
		pVolFX->ComputeFog(pSM, mDirLightsNum, mDirLight, mSpotLightsNum, mSpotLight, mPointLightsNum, mPointLight, mWSPositionMapSRV, mBackbuffer);
	}
	//SVO build and raycasting////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (pSVO && mEnableRT)
	{
		static bool builtTree = false;
		if (!builtTree)
		{
			pSVO->Reset();

			ID3D11UnorderedAccessView* svoUnorderedAccessViews[1] = { pSVO->GetNodesPoolUAV() };
			renderTargets[0] = pSVO->Get64x64RT();
			md3dImmediateContext->ClearRenderTargetView(renderTargets[0], reinterpret_cast<const float*>(&Colors::Black));
			md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
			md3dImmediateContext->RSSetViewports(1, &pSVO->Get64x64RTViewport());
			Draw(mCamera, -1, renderTargets, 1, svoUnorderedAccessViews, 1, 1, NULL, DrawShadowType::DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_SVO);
			builtTree = true;
		}

		pSVO->SvoCastReflectionRays();  //generate precise mirror - like reflection position map
		pSVO->SvoTraceReflectionCones(); //generate blurry reflections
		//pSVO->SvoCastRays(); 
	}
	//Draw objects////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	renderTargets[0] = mRenderTargetView;
	md3dImmediateContext->ClearRenderTargetView(mRenderTargetView, reinterpret_cast<const float*>(&Colors::LightSteelBlue));
	md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	Draw(renderTargets, mDepthStencilView);
	//SVO visualization///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/*if (pSVO && mEnableRT)
		pSVO->ViewTree(mRenderTargetView, mDepthStencilView, mScreenViewport);*/
	//Texture debugger////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (mEnableDebugTextureMode) {
		DrawDebuggingTexture(&mRenderTargetView, mDepthStencilView, pSVO->GetRayCasterOutput());
	}
	//Volumetric FX///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (pVolFX && mEnableVolFX && pGI)
		pVolFX->Draw(mRenderTargetView);
	//GUI/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	md3dImmediateContext->OMSetDepthStencilState(pRenderStates->NoDepthWritesDSS, 0);
	D2DGUIDraw(&mRenderTargetView, mDepthStencilView);
	md3dImmediateContext->OMSetDepthStencilState(0, 0);
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	mSwapChain->Present(0, 0);
}

void QGE_GPU_Framework::DrawMesh(Camera& camera, DrawPassType dpt, int MeshIndex)
{
	const UINT stride = sizeof(MeshVertex);
	const UINT offset = 0;

	XMFLOAT4 MeshTexTypes[MAX_TEXTURE_COUNT];

	XMMATRIX view = camera.View();
	XMMATRIX proj = camera.Proj();
	XMMATRIX viewProj = camera.ViewProj();

	D3DX11_TECHNIQUE_DESC TechDesc;
	pMeshManager->GetMeshDrawTech(MeshIndex)->GetDesc(&TechDesc);

	md3dImmediateContext->IASetPrimitiveTopology(pMeshManager->GetMeshPrimitiveTopology(MeshIndex));
	md3dImmediateContext->IASetVertexBuffers(0, 1, pMeshManager->GetMeshVertexBuffer(MeshIndex), &stride, &offset);
	md3dImmediateContext->IASetInputLayout(pMeshManager->GetInputLayout());

	if (pMeshManager->GetMeshDrawType(MeshIndex) == DrawType::INDEXED_DRAW)
		md3dImmediateContext->IASetIndexBuffer(pMeshManager->GetMeshIndexBuffer(MeshIndex), DXGI_FORMAT_R32_UINT, 0);

	if (pMeshManager->GetMeshStates(MeshIndex).mUseCulling == TRUE);
	else
		md3dImmediateContext->RSSetState(pRenderStates->NoCullRS);

	XMMATRIX world[MAX_INSTANCES_COUNT];
	XMMATRIX worldInvTranspose[MAX_INSTANCES_COUNT];
	XMMATRIX worldInv[MAX_INSTANCES_COUNT];
	XMMATRIX worldViewProj[MAX_INSTANCES_COUNT];
	XMFLOAT3 objDynamicCubeMapW[MAX_INSTANCES_COUNT];
	UINT visibleInstancesCount = 0;
	for (int i = 0; i < pMeshManager->GetMeshInstancesCount(MeshIndex); ++i)
	{
		if (((PerformFrustumCulling(camera, MeshIndex, i) || !mEnableFrustumCulling) && dpt != DrawPassType::DRAW_PASS_TYPE_PARTICLES_GRID) || (pGpuCollision && dpt == DrawPassType::DRAW_PASS_TYPE_PARTICLES_GRID && pGpuCollision->Contains(make_pair(MeshIndex, i))))
			visibleInstancesCount++;
		else
			continue;

		world[visibleInstancesCount - 1] = pMeshManager->GetMeshWorldMatrixXM(MeshIndex, i);
		worldInvTranspose[visibleInstancesCount - 1] = MathHelper::InverseTranspose(world[visibleInstancesCount - 1]);
		worldInv[visibleInstancesCount - 1] = XMMatrixTranspose(worldInvTranspose[visibleInstancesCount - 1]);
		worldViewProj[visibleInstancesCount - 1] = world[visibleInstancesCount - 1] * view*proj;
		objDynamicCubeMapW[visibleInstancesCount - 1] = pMeshManager->GetMeshDynamicCubeMapWorldF3(MeshIndex, i);
	}

	if (visibleInstancesCount == 0)
		return;

	for (int p = 0; p < TechDesc.Passes; ++p)
	{
		if (dpt != DrawPassType::DRAW_PASS_TYPE_PARTICLES_GRID && pGpuCollision)
			pGpuCollision->SetTestParams();
		if (dpt == DrawPassType::DRAW_PASS_TYPE_PARTICLES_GRID && pGpuCollision)
			pGpuCollision->SetPerMeshData(MeshIndex);

		mfxMeshType->SetInt(pMeshManager->GetMeshType(MeshIndex));

		if (pMeshManager->GetMeshDynamicCubeMapSRV(MeshIndex) != 0)
			mfxEnvMap->SetResource(pMeshManager->GetMeshDynamicCubeMapSRV(MeshIndex));
		else
			mfxEnvMap->SetResource(pEnvMap->GetCubeMapSRV());

		if (pMeshManager->GetMeshPlanarRefractionMapSRV(MeshIndex) != 0)
			mfxPlanarRefractionMap->SetResource(pMeshManager->GetMeshPlanarRefractionMapSRV(MeshIndex));

		if (pMeshManager->GetMeshStates(MeshIndex).mUseNormalMapping || pMeshManager->GetMeshStates(MeshIndex).mUseDisplacementMapping)
			NormalMap->SetResource(pMeshManager->GetMeshNormalMapSRV(MeshIndex));

		if (pMeshManager->GetMeshMaterialType(MeshIndex) == MaterialType::MTL_TYPE_REFLECT_MAP)
			ReflectMap->SetResource(pMeshManager->GetMeshReflectMapSRV(MeshIndex));

		mfxMaterialType->SetInt(pMeshManager->GetMeshMaterialType(MeshIndex));

		TexCount->SetInt(pMeshManager->GetMeshTextureCount(MeshIndex));
		TexTransform->SetRawValue(pMeshManager->GetMeshTextureTransformsF4X4(MeshIndex), 0, sizeof(XMFLOAT4X4)*(pMeshManager->GetMeshTextureCount(MeshIndex)));
		UseTexture->SetBool(pMeshManager->GetMeshStates(MeshIndex).mUseTextures);
		if (pMeshManager->GetMeshTexArrayType(MeshIndex) == TexArrayType::EQUAL_FORMAT)
		{
			if (pMeshManager->GetMeshStates(MeshIndex).mUseTextures)
			{

				for (int f = 0; f < pMeshManager->GetMeshTextureCount(MeshIndex); ++f)
				{
					MeshTexTypes[f] = XMFLOAT4(pMeshManager->GetMeshTextureType(MeshIndex)[f], pMeshManager->GetMeshTextureType(MeshIndex)[f],
						pMeshManager->GetMeshTextureType(MeshIndex)[f], pMeshManager->GetMeshTextureType(MeshIndex)[f]);
				}
				TexType->SetRawValue(MeshTexTypes, 0, pMeshManager->GetMeshTextureCount(MeshIndex) * sizeof(XMFLOAT4));
				TexDiffuseMapArray->SetResource(pMeshManager->GetMeshDiffuseMapArraySRV(MeshIndex));
			}
			UseExtTextureArray->SetBool(false);
		}
		else
		{
			if (pMeshManager->GetMeshStates(MeshIndex).mUseTextures)
			{
				for (int f = 0; f < pMeshManager->GetMeshTextureCount(MeshIndex); ++f)
				{
					MeshTexTypes[f] = XMFLOAT4(pMeshManager->GetMeshTextureType(MeshIndex)[f], pMeshManager->GetMeshTextureType(MeshIndex)[f],
						pMeshManager->GetMeshTextureType(MeshIndex)[f], pMeshManager->GetMeshTextureType(MeshIndex)[f]);
				}
				TexType->SetRawValue(MeshTexTypes, 0, pMeshManager->GetMeshTextureCount(MeshIndex) * sizeof(XMFLOAT4));
				ExtTexDiffuseMapArray->SetResourceArray(pMeshManager->GetMeshExtDiffuseMapArraySRVs(MeshIndex), 0, pMeshManager->GetMeshTextureCount(MeshIndex));
			}
			UseExtTextureArray->SetBool(true);
		}
		UseBlackClipping->SetBool(mUseBlackClippingAtNonBlend);
		UseAlphaClipping->SetBool(mUseAlphaClippingAtNonBlend);
		UseNormalMapping->SetBool(pMeshManager->GetMeshStates(MeshIndex).mUseNormalMapping);
		UseDisplacementMapping->SetBool(pMeshManager->GetMeshStates(MeshIndex).mUseDisplacementMapping);

		mfxObjectDynamicCubeMapWorld->SetFloatVectorArray(reinterpret_cast<float*>(objDynamicCubeMapW), 0, visibleInstancesCount);
		if (dpt == DrawPassType::DRAW_PASS_TYPE_COLOR_NO_REFLECTION)
		{
			mfxReflectionType->SetInt(ReflectionType::REFLECTION_TYPE_NONE);
			mfxRefractionType->SetInt(RefractionType::REFRACTION_TYPE_NONE);
		}
		else
		{
			mfxReflectionType->SetInt(pMeshManager->GetMeshReflectionType(MeshIndex));
			mfxRefractionType->SetInt(pMeshManager->GetMeshRefractionType(MeshIndex));
		}
		mfxApproxSceneSz->SetFloat(pMeshManager->GetMeshBS(MeshIndex).Radius * mApproxSceneSz);

		mfxHeightScale->SetFloat(mHeightScale);

		mfxWorld->SetMatrixArray(reinterpret_cast<float*>(world), 0, visibleInstancesCount);
		mfxView->SetMatrix(reinterpret_cast<float*>(&view));
		mfxProj->SetMatrix(reinterpret_cast<float*>(&proj));
		mfxInvWorld->SetMatrixArray(reinterpret_cast<float*>(worldInv), 0, visibleInstancesCount);
		mfxWorldInvTranspose->SetMatrixArray(reinterpret_cast<float*>(worldInvTranspose), 0, visibleInstancesCount);
		mfxWorldViewProj->SetMatrixArray(reinterpret_cast<float*>(worldViewProj), 0, visibleInstancesCount);
		mfxViewProj->SetMatrix(reinterpret_cast<float*>(&viewProj));
		mfxMaterial->SetRawValue(&pMeshManager->GetMeshMaterial(MeshIndex), 0, sizeof(pMeshManager->GetMeshMaterial(MeshIndex)));
		mfxBoundingSphereRadius->SetFloat(pMeshManager->GetMeshBS(MeshIndex).Radius);

		pMeshManager->GetMeshDrawTech(MeshIndex)->GetPassByIndex(p)->Apply(0, md3dImmediateContext);

		if (pMeshManager->GetMeshDrawType(MeshIndex) == DrawType::INDEXED_DRAW)
			md3dImmediateContext->DrawIndexedInstanced(pMeshManager->GetMeshIndexCount(MeshIndex), visibleInstancesCount, 0, 0, 0);
		else
			md3dImmediateContext->DrawInstanced(pMeshManager->GetMeshVertexCount(MeshIndex), visibleInstancesCount, 0, 0);
	}
	md3dImmediateContext->RSSetState(0);
}

bool QGE_GPU_Framework::PerformFrustumCulling(Camera& camera,	int MeshIndex,	int MeshInstanceIndex)
{
	XMMATRIX World = pMeshManager->GetMeshWorldMatrixXM(MeshIndex, MeshInstanceIndex);
	XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(World), World);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(camera.View()), camera.View());
	XMMATRIX toLocal = XMMatrixMultiply(invView, invWorld);
	XMVECTOR scale;
	XMVECTOR rotQuat;
	XMVECTOR translation;
	XMMatrixDecompose(&scale, &rotQuat, &translation, toLocal);
	BoundingFrustum localSpaceCamFrustum;
	BoundingFrustum::CreateFromMatrix(localSpaceCamFrustum, camera.Proj());
	localSpaceCamFrustum.Transform(localSpaceCamFrustum, XMVectorGetX(scale), rotQuat, translation);
	return localSpaceCamFrustum.Intersects(pMeshManager->GetMeshAABB(MeshIndex));
}

void QGE_GPU_Framework::SelectMeshDrawTech(DrawShadowType shadowType, DrawPassType dpt, int MeshIndex)
{
	if (pPlanarWaterSurface && MeshIndex == pPlanarWaterSurface->GetWaterMeshIndex())
		pPlanarWaterSurface->SelectDrawTech(dpt, shadowType);

	if (dpt == DrawPassType::DRAW_PASS_TYPE_SHADOW_MAP_BUILD) {
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildShadowMapBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildShadowMapTessTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_NORMAL_DEPTH)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildNormalDepthMapBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildNormalDepthMapTessTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_DEPTH)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildDepthMapBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildDepthMapTessTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_POSITION_WS)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildWSPositionMapBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildWSPositionMapTessTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_AMBIENT)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildAmbientMapBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildAmbientMapTessTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_DIFFUSE)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildDiffuseMapBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildDiffuseMapTessTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_MATERIAL_SPECULAR)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildSpecularMapBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildSpecularMapTessTech);
	}
	else if(dpt == DrawPassType::DRAW_PASS_TYPE_VOXEL_GRID)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mGenerateGridBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mGenerateGridTessTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_PARTICLES_GRID)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildBodiesPPLLBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildBodiesPPLLTessTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_SVO)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildSvoBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildSvoTessTech);
	}
}

bool QGE_GPU_Framework::SelectTransparentMeshDrawTech(DrawShadowType shadowType, DrawPassType dpt, int MeshIndex)
{
	if (dpt == DrawPassType::DRAW_PASS_TYPE_COLOR ||
		dpt == DrawPassType::DRAW_PASS_TYPE_AMBIENT ||
		dpt == DrawPassType::DRAW_PASS_TYPE_DIFFUSE ||
		dpt == DrawPassType::DRAW_PASS_TYPE_COLOR_NO_REFLECTION)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildPPLLBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildPPLLTessTech);
		return true;
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_SHADOW_MAP_BUILD) {
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildShadowMapBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildShadowMapTessTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_NORMAL_DEPTH)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildNormalDepthMapBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildNormalDepthMapTessTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_DEPTH)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildDepthMapBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildDepthMapTessTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_POSITION_WS)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildWSPositionMapBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildWSPositionMapTessTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_MATERIAL_SPECULAR)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildSpecularMapBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildSpecularMapTessTech);
	}
	else if(dpt == DrawPassType::DRAW_PASS_TYPE_VOXEL_GRID)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mGenerateGridBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mGenerateGridTessTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_PARTICLES_GRID)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildBodiesPPLLBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildBodiesPPLLTessTech);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_SVO)
	{
		if (pMeshManager->GetMeshDrawTech(MeshIndex) == mBasicTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildSvoBasicTech);
		else if (pMeshManager->GetMeshDrawTech(MeshIndex) == mTessTech)
			pMeshManager->SetMeshDrawTech(MeshIndex, mBuildSvoTessTech);
	}
	return false;
}

void QGE_GPU_Framework::SetPerFrameConstants(Camera& camera, DrawShadowType shadowType, int LightIndex, DrawPassType dpt)
{
	if (pPlanarWaterSurface)
		pPlanarWaterSurface->SetResources();

	if (pSM) {
		mfxDirLightShadowMapArray->SetResource(pSM->GetDirLightDistanceMapSRV());
		mfxDirLightShadowTransformArray->SetRawValue(&pSM->mDirLightShadowTransform, 0, sizeof(pSM->mDirLightShadowTransform));
		mfxDirLightSplitDistances->SetRawValue(&pSM->mDirLightSplitDistances, 0, sizeof(pSM->mDirLightSplitDistances));
		mfxDirLightIndex->SetFloat(LightIndex);

		mfxSpotLightShadowMapArray->SetResource(pSM->GetSpotLightDistanceMapSRV());
		mfxSpotLightShadowTransformArray->SetRawValue(&pSM->mSpotLightShadowTransform, 0, sizeof(pSM->mSpotLightShadowTransform));
		mfxSpotLightIndex->SetFloat(LightIndex);

		mfxPointLightCubicShadowMapArray->SetResource(pSM->GetPointLightDistanceMapSRV());
	}

	if (pGpuCollision && dpt == DrawPassType::DRAW_PASS_TYPE_PARTICLES_GRID)
	{
		pGpuCollision->SetPerIterationData();
	}
	else if (pSVO && dpt == DrawPassType::DRAW_PASS_TYPE_SVO)
	{
		pSVO->Setup();
	}
	else if (pOIT_PPLL && (dpt == DrawPassType::DRAW_PASS_TYPE_COLOR || dpt == DrawPassType::DRAW_PASS_TYPE_AMBIENT || dpt == DrawPassType::DRAW_PASS_TYPE_DIFFUSE || dpt == DrawPassType::DRAW_PASS_TYPE_COLOR_NO_REFLECTION))
	{
		pOIT_PPLL->Setup();
		pOIT_PPLL->SetResources();
	}
	else if (pGI && dpt == DrawPassType::DRAW_PASS_TYPE_VOXEL_GRID)
	{
		pGI->Setup();
	}

	/*if (pSVO && dpt == DrawPassType::DRAW_PASS_TYPE_COLOR)
		pSVO->SetTestParams();*/

	mfxPointLightIndex->SetFloat(LightIndex);

	mfxShadowType->SetFloat(static_cast<const float>(shadowType));

	mfxEyePosW->SetFloatVector(reinterpret_cast<float*>(&(camera.GetPosition())));

	mfxDirLightsNum->SetInt(static_cast<int>(mDirLightsNum));
	mfxDirLight->SetRawValue(&mDirLight, 0, sizeof(mDirLight));

	mfxSpotLightsNum->SetInt(static_cast<int>(mSpotLightsNum));
	mfxSpotLight->SetRawValue(&mSpotLight, 0, sizeof(mSpotLight));

	mfxPointLightsNum->SetInt(static_cast<int>(mPointLightsNum));
	mfxPointLight->SetRawValue(&mPointLight, 0, sizeof(mPointLight));

	mfxTime->SetFloat(mTime*mTimeSpeed);

	if(pSsao)
		SsaoMap->SetResource(pSsao->AmbientSRV());
	mfxEnableSsao->SetBool(mEnableSsao);
	if(pSSLR)
		SSLRMap->SetResource(pSSLR->GetSSLROutputSRV());
	mfxEnableSSLR->SetBool(mEnableSSLR);
	if (pSVO)
		SVORTMap->SetResource(pSVO->GetRayCasterOutput());
	mfxEnableRT->SetBool(mEnableRT);
	if (pGI && dpt != DrawPassType::DRAW_PASS_TYPE_VOXEL_GRID)
		pGI->SetFinalSHBuffer();
	mfxEnableGI->SetBool(mEnableGI);

	EnableDynamicLOD->SetBool(mEnableDynamicLOD);
	MaxTessFactor->SetFloat(mMaxTessFactor);
	MaxLODdist->SetFloat(mMaxLODdist);
	MinLODdist->SetFloat(mMinLODdist);
	ConstantLODTessFactor->SetFloat(mConstantLODTessFactor);
}

void QGE_GPU_Framework::Draw(Camera& camera, int MeshNotDrawIndex,
	ID3D11RenderTargetView** RTVs, UINT RTVsCount,
	ID3D11UnorderedAccessView** UAVs, UINT UAVsCount, UINT initialCounts,
	ID3D11DepthStencilView* depthStencilView, DrawShadowType shadowType,
	int LightIndex, DrawPassType dpt)
{
	mfxDrawPassType->SetInt(dpt);

	md3dImmediateContext->PSSetShaderResources(0, 1, pNullSRV);

	UINT InitialCounts = initialCounts;
	md3dImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, RTVsCount, UAVsCount, UAVs, &InitialCounts);      /// these two lines binds uavs with reset counter and unbinds them,
	md3dImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, RTVsCount, UAVsCount, pNullUAV, &InitialCounts);  /// because Effects11 seems to have bug when setting UAVs and RTs at the same time!
	md3dImmediateContext->OMSetRenderTargets(RTVsCount, RTVs, depthStencilView);

	SetPerFrameConstants(camera, shadowType, LightIndex, dpt);

	if (pTerrain && mTerrainDraw && dpt != DrawPassType::DRAW_PASS_TYPE_MATERIAL_SPECULAR && dpt != DrawPassType::DRAW_PASS_TYPE_PARTICLES_GRID && dpt != DrawPassType::DRAW_PASS_TYPE_SVO)
		pTerrain->Draw(md3dImmediateContext, camera, RTVs, depthStencilView, shadowType, LightIndex, dpt);
	if (mEnvMapDraw && (dpt == DrawPassType::DRAW_PASS_TYPE_COLOR) && dpt != DrawPassType::DRAW_PASS_TYPE_PARTICLES_GRID && dpt != DrawPassType::DRAW_PASS_TYPE_SVO)
		pEnvMap->Draw(camera, RTVs, depthStencilView);

	md3dImmediateContext->RSSetState(0);
	md3dImmediateContext->OMSetBlendState(0, 0, 0xffffffff);
	md3dImmediateContext->OMSetDepthStencilState(0, 0);

	if (dpt == DrawPassType::DRAW_PASS_TYPE_VOXEL_GRID)
	{
		md3dImmediateContext->OMSetDepthStencilState(pRenderStates->NoDepthStencilTestDSS, 0);
	}
	else if (dpt == DrawPassType::DRAW_PASS_TYPE_SVO)
	{
		md3dImmediateContext->OMSetDepthStencilState(pRenderStates->NoDepthStencilTestDSS, 0);
	}
	else if(dpt == DrawPassType::DRAW_PASS_TYPE_PARTICLES_GRID)
	{
		md3dImmediateContext->RSSetState(pRenderStates->NoCullRS);
		md3dImmediateContext->OMSetDepthStencilState(pRenderStates->NoDepthDSS, 0);
	}

	for (int i = 0; i < pMeshManager->GetMeshCount(); ++i)
	{
		ID3DX11EffectTechnique* tempTech;
		tempTech = pMeshManager->GetMeshDrawTech(i);

		SelectMeshDrawTech(shadowType, dpt, i);
		if (i != MeshNotDrawIndex)
		{
			if (pMeshManager->GetMeshStates(i).mUseTransparency == FALSE && pMeshManager->GetMeshStates(i).mUseAdditiveBlend == FALSE)
			{
				DrawMesh(camera, dpt, i);
			}
		}
		pMeshManager->SetMeshDrawTech(i, tempTech);
	}

	UINT TransparentMeshesCount = 0;
	for (int i = 0; i < pMeshManager->GetMeshCount(); ++i)
	{
		ID3DX11EffectTechnique* tempTech;
		tempTech = pMeshManager->GetMeshDrawTech(i);

		bool IsDrawNeedTransparency = SelectTransparentMeshDrawTech(shadowType, dpt, i);
		
		if (i != MeshNotDrawIndex)
		{
			if (pMeshManager->GetMeshStates(i).mUseTransparency == TRUE)
			{
				if (IsDrawNeedTransparency)
				{
					++TransparentMeshesCount;
					md3dImmediateContext->RSSetState(pRenderStates->NoCullRS);
					md3dImmediateContext->OMSetDepthStencilState(pRenderStates->NoDepthDSS, 0);			
				}

				DrawMesh(camera, dpt, i);

				if (IsDrawNeedTransparency)
				{
					md3dImmediateContext->RSSetState(0);
					md3dImmediateContext->OMSetDepthStencilState(0, 0);
				}
			}
		}
		pMeshManager->SetMeshDrawTech(i, tempTech);
	}

	for (int i = 0; i < pMeshManager->GetMeshCount(); ++i)
	{
		ID3DX11EffectTechnique* tempTech;
		tempTech = pMeshManager->GetMeshDrawTech(i);

		SelectMeshDrawTech(shadowType, dpt, i);
		if (i != MeshNotDrawIndex)
		{
			if (pMeshManager->GetMeshStates(i).mUseAdditiveBlend == TRUE)
			{
				if(dpt != DrawPassType::DRAW_PASS_TYPE_VOXEL_GRID && dpt != DrawPassType::DRAW_PASS_TYPE_SVO)
				{
					md3dImmediateContext->OMSetBlendState(pRenderStates->AdditiveBlendBS, 0, 0xffffffff);
					md3dImmediateContext->OMSetDepthStencilState(pRenderStates->NoDepthDSS, 0);
					md3dImmediateContext->RSSetState(pRenderStates->NoCullRS);
				}
	
				DrawMesh(camera, dpt, i);

				if(dpt != DrawPassType::DRAW_PASS_TYPE_VOXEL_GRID && dpt != DrawPassType::DRAW_PASS_TYPE_SVO)
				{
					md3dImmediateContext->RSSetState(0);
					md3dImmediateContext->OMSetBlendState(0, 0, 0xffffffff);
					md3dImmediateContext->OMSetDepthStencilState(0, 0);
				}
			}
		}

		pMeshManager->SetMeshDrawTech(i, tempTech);
	}

	if (shadowType == DrawShadowType::DRAW_SHADOW_TYPE_NO_SHADOW && dpt == DrawPassType::DRAW_PASS_TYPE_COLOR)
	{
		if (ParticleSystems.find("RAIN") != ParticleSystems.end() && MeshNotDrawIndex != -1)
		{
			ParticleSystems["RAIN"]->SetEmitPos(mCamera.GetPosition());
		}
		for (auto ps : ParticleSystems)
		{
			if (!mDrawParticleSystemRefraction || ps.first != "RAIN")
			{
				if (!pPlanarWaterSurface || !pPlanarWaterSurface->GetDrawRefractionFlag())
					ps.second->Draw(camera, RTVs, depthStencilView, mClientHeight, mClientWidth, pEnvMap->GetCubeMapSRV(), mDirLight, mDirLightsNum, mPointLight, mPointLightsNum, mSpotLight, mSpotLightsNum);
			}
		}
		md3dImmediateContext->RSSetState(0);
		md3dImmediateContext->OMSetBlendState(0, 0, 0xffffffff);
		md3dImmediateContext->OMSetDepthStencilState(0, 0);
	}

	SPHFluidDraw(camera, dpt, RTVs, depthStencilView, MeshNotDrawIndex);

	md3dImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, pNullRTV, nullptr, 1, 1, pNullUAV, &InitialCounts);

	if (pOIT && TransparentMeshesCount > 0 && (dpt == DrawPassType::DRAW_PASS_TYPE_COLOR || dpt == DrawPassType::DRAW_PASS_TYPE_AMBIENT || dpt == DrawPassType::DRAW_PASS_TYPE_DIFFUSE || dpt == DrawPassType::DRAW_PASS_TYPE_COLOR_NO_REFLECTION))
	{
		pOIT->Compute(RTVs);
	}
}

void QGE_GPU_Framework::Draw(ID3D11RenderTargetView** renderTargets, ID3D11DepthStencilView* depthStencilView)
{
	ID3D11RenderTargetView* curRenderTargets[1];
	ID3D11UnorderedAccessView* oitUnorderedAccessViews[2];
	oitUnorderedAccessViews[0] = (pOIT_PPLL ? pOIT_PPLL->GetHeadIndexBufferUAV() : NULL);
	oitUnorderedAccessViews[1] = (pOIT_PPLL ? pOIT_PPLL->GetFragmentsListBufferUAV() : NULL);

	for (int j = 0; j < pMeshManager->GetMeshCount(); ++j)
	{
		XMVECTOR vCenter = XMVectorSet(pMeshManager->GetMeshBS(j).Center.x, pMeshManager->GetMeshBS(j).Center.y, pMeshManager->GetMeshBS(j).Center.z, 1.0f);
		for (int i = 0; i < pMeshManager->GetMeshInstancesCount(j); ++i)
		{
			vCenter = XMVector3TransformCoord(vCenter, pMeshManager->GetMeshWorldMatrixXM(j, i));
			pMeshManager->SetMeshDynamicCubeMapWorldV(j, i, vCenter);
			BuildCubeFaceCamera(pMeshManager->GetMeshDynamicCubeMapWorldF3(j, i).x, pMeshManager->GetMeshDynamicCubeMapWorldF3(j, i).y, pMeshManager->GetMeshDynamicCubeMapWorldF3(j, i).z);
		}
		if (pMeshManager->GetMeshRefractionType(j) == RefractionType::REFRACTION_TYPE_DYNAMIC_CUBE_MAP ||
			pMeshManager->GetMeshReflectionType(j) == ReflectionType::REFLECTION_TYPE_DYNAMIC_CUBE_MAP ||
			pMeshManager->GetMeshReflectionType(j) == ReflectionType::REFLECTION_TYPE_DYNAMIC_CUBE_MAP_AND_SSLR) {
			for (int i = 0; i < 6; ++i)
			{
				if (ParticleSystems.size() > 0)
					ParticleSystemDrawRefraction(mCubeMapCamera[i]);
				if (pPlanarWaterSurface)
					WaterDrawReflectionAndRefraction(mCubeMapCamera[i]);

				md3dImmediateContext->RSSetViewports(1, &pMeshManager->GetMeshCubeMapViewport(j));
				md3dImmediateContext->ClearRenderTargetView(pMeshManager->GetMeshDynamicCubeMapRTVs(j)[i], reinterpret_cast<const float*>(&Colors::Black));
				md3dImmediateContext->ClearDepthStencilView(pMeshManager->GetMeshDynamicCubeMapDSV(j), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
				curRenderTargets[0] = pMeshManager->GetMeshDynamicCubeMapRTVs(j)[i];

				Draw(mCubeMapCamera[i], j, curRenderTargets, 1, oitUnorderedAccessViews, 2, 0, pMeshManager->GetMeshDynamicCubeMapDSV(j), DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_COLOR);
			}
			md3dImmediateContext->GenerateMips(pMeshManager->GetMeshDynamicCubeMapSRV(j));
		}
		else if (pMeshManager->GetMeshRefractionType(j) == RefractionType::REFRACTION_TYPE_PLANAR)
		{
			if (ParticleSystems.size() > 0)
				ParticleSystemDrawRefraction(mCamera);
			if (pPlanarWaterSurface)
				WaterDrawReflectionAndRefraction(mCamera);

			md3dImmediateContext->RSSetViewports(1, &pMeshManager->GetMeshPlanarRefractionMapViewport(j));
			md3dImmediateContext->ClearRenderTargetView(pMeshManager->GetMeshPlanarRefractionMapRTV(j), reinterpret_cast<const float*>(&Colors::Black));
			md3dImmediateContext->ClearDepthStencilView(pMeshManager->GetMeshPlanarRefractionMapDSV(j), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
			curRenderTargets[0] = pMeshManager->GetMeshPlanarRefractionMapRTV(j);

			Draw(mCamera, j, curRenderTargets, 1, oitUnorderedAccessViews, 2, 0, pMeshManager->GetMeshPlanarRefractionMapDSV(j), DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_COLOR);

			md3dImmediateContext->GenerateMips(pMeshManager->GetMeshPlanarRefractionMapSRV(j));
		}
		else;
	}
	if (ParticleSystems.size() > 0)
		ParticleSystemDrawRefraction(mCamera);
	if (pPlanarWaterSurface)
		WaterDrawReflectionAndRefraction(mCamera);
	if (pSPH)
		SPHFluidDrawRefraction(mCamera);

	md3dImmediateContext->RSSetViewports(1, &mScreenViewport);
	curRenderTargets[0] = renderTargets[0];
	md3dImmediateContext->ClearRenderTargetView(curRenderTargets[0], reinterpret_cast<const float*>(&Colors::Black));
	md3dImmediateContext->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	Draw(mCamera, -1, curRenderTargets, 1, oitUnorderedAccessViews, 2, 0, depthStencilView, DRAW_SHADOW_TYPE_NO_SHADOW, -1, DrawPassType::DRAW_PASS_TYPE_COLOR);
}

void QGE_GPU_Framework::DrawDebuggingTexture(ID3D11RenderTargetView** renderTargets,
	ID3D11DepthStencilView* depthStencilView,
	ID3D11ShaderResourceView* debuggingTex)
{
	md3dImmediateContext->OMSetRenderTargets(1, renderTargets, depthStencilView);

	md3dImmediateContext->IASetInputLayout(pMeshManager->GetInputLayout());
	md3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const UINT stride = sizeof(MeshVertex);
	const UINT offset = 0;

	XMFLOAT4X4 Identity;
	XMStoreFloat4x4(&Identity, I);

	D3DX11_TECHNIQUE_DESC techDesc;

	mDrawScreenQuadTech->GetDesc(&techDesc);
	for (int p = 0; p < techDesc.Passes; ++p)
	{
		md3dImmediateContext->IASetVertexBuffers(0, 1, &mScreenQuadVB, &stride, &offset);
		md3dImmediateContext->IASetIndexBuffer(mScreenQuadIB, DXGI_FORMAT_R32_UINT, 0);
		md3dImmediateContext->RSSetState(pRenderStates->NoCullRS);

		mfxWorld->SetMatrix(reinterpret_cast<float*>(&Identity));
		mfxView->SetMatrix(reinterpret_cast<const float*>(&Identity));
		mfxProj->SetMatrix(reinterpret_cast<float*>(&Identity));
		mfxWorldInvTranspose->SetMatrix(reinterpret_cast<float*>(&Identity));
		mfxWorldViewProj->SetMatrix(reinterpret_cast<float*>(&Identity));
		mfxViewProj->SetMatrix(reinterpret_cast<float*>(&Identity));
		TexTransform->SetRawValue(&Identity, 0, sizeof(I));
		TexDiffuseMapArray->SetResource(debuggingTex);

		mDrawScreenQuadTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
		md3dImmediateContext->DrawIndexed(6, 0, 0);
	}
	md3dImmediateContext->RSSetState(0);
	md3dImmediateContext->OMSetDepthStencilState(0, 0);
}

void QGE_GPU_Framework::BuildTextureDebuggerBuffers()
{
	GeometryGenerator::MeshData quad;

	GeometryGenerator geoGen;
	geoGen.CreateFullscreenQuad(quad);

	std::vector<MeshVertex> vertices(quad.Vertices.size());

	for (int i = 0; i < quad.Vertices.size(); ++i)
	{
		vertices[i].Pos = quad.Vertices[i].Position;
		vertices[i].Normal = quad.Vertices[i].Normal;
		vertices[i].TexCoord = quad.Vertices[i].TexC;
	}

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(MeshVertex) * quad.Vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &vertices[0];
	HR(md3dDevice->CreateBuffer(&vbd, &vinitData, &mScreenQuadVB));

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(int)* quad.Indices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &quad.Indices[0];
	HR(md3dDevice->CreateBuffer(&ibd, &iinitData, &mScreenQuadIB));
}

void QGE_GPU_Framework::BuildFX()
{
	mFX = d3dHelper::CreateEffectFromMemory(L"FX/Effects.cso", md3dDevice);

	//Draw pass type
	mfxDrawPassType = mFX->GetVariableByName("gDrawPassType")->AsScalar();

	//Buffers
	mfxMeshType = mFX->GetVariableByName("gMeshType")->AsScalar();

	//Techniques
	mBasicTech = mFX->GetTechniqueByName("BasicTech");
	mDrawScreenQuadTech = mFX->GetTechniqueByName("DrawScreenQuadTech");
	mTessTech = mFX->GetTechniqueByName("TessTech");

	//Per-object matrices
	mfxWorldViewProj = mFX->GetVariableByName("gWorldViewProj")->AsMatrix();
	mfxViewProj = mFX->GetVariableByName("gViewProj")->AsMatrix();
	mfxProj = mFX->GetVariableByName("gProj")->AsMatrix();
	mfxView = mFX->GetVariableByName("gView")->AsMatrix();
	mfxWorld = mFX->GetVariableByName("gWorld")->AsMatrix();
	mfxInvWorld = mFX->GetVariableByName("gInvWorld")->AsMatrix();
	mfxWorldInvTranspose = mFX->GetVariableByName("gWorldInvTranspose")->AsMatrix();

	//Eye position
	mfxEyePosW = mFX->GetVariableByName("gEyePosW")->AsVector();

	//Lighting
	mfxDirLight = mFX->GetVariableByName("gDirLight");
	mfxSpotLight = mFX->GetVariableByName("gSpotLight");
	mfxPointLight = mFX->GetVariableByName("gPointLight");
	mfxMaterial = mFX->GetVariableByName("gMaterial");
	mfxDirLightsNum = mFX->GetVariableByName("gDirLightsNum")->AsScalar();
	mfxSpotLightsNum = mFX->GetVariableByName("gSpotLightsNum")->AsScalar();
	mfxPointLightsNum = mFX->GetVariableByName("gPointLightsNum")->AsScalar();

	//Texturing
	TexDiffuseMapArray = mFX->GetVariableByName("gDiffuseMapArray")->AsShaderResource();
	ExtTexDiffuseMapArray = mFX->GetVariableByName("gExtDiffuseMapArray")->AsShaderResource();
	NormalMap = mFX->GetVariableByName("gNormalMap")->AsShaderResource();
	ReflectMap = mFX->GetVariableByName("gReflectMap")->AsShaderResource();
	TexTransform = mFX->GetVariableByName("gTexTransform");
	TexCount = mFX->GetVariableByName("gTexCount")->AsScalar();
	TexType = mFX->GetVariableByName("gTexType");
	UseTexture = mFX->GetVariableByName("gUseTexture")->AsScalar();
	UseExtTextureArray = mFX->GetVariableByName("gUseExtTextureArray")->AsScalar();
	UseNormalMapping = mFX->GetVariableByName("gUseNormalMapping")->AsScalar();
	UseDisplacementMapping = mFX->GetVariableByName("gUseDisplacementMapping")->AsScalar();
	mfxMaterialType = mFX->GetVariableByName("gMaterialType")->AsScalar();

	//Time
	mfxTime = mFX->GetVariableByName("gTime")->AsScalar();

	//Tesselation
	MaxTessFactor = mFX->GetVariableByName("gMaxTessFactor")->AsScalar();
	MaxLODdist = mFX->GetVariableByName("gMaxLODdist")->AsScalar();
	MinLODdist = mFX->GetVariableByName("gMinLODdist")->AsScalar();
	ConstantLODTessFactor = mFX->GetVariableByName("gConstantLODTessFactor")->AsScalar();
	EnableDynamicLOD = mFX->GetVariableByName("gEnableDynamicLOD")->AsScalar();
	mfxHeightScale = mFX->GetVariableByName("gHeightScale")->AsScalar();

	//Clipping
	UseBlackClipping = mFX->GetVariableByName("gUseBlackClipping")->AsScalar();
	UseAlphaClipping = mFX->GetVariableByName("gUseAlphaClipping")->AsScalar();

	//Reflection / refraction & enviroment maps
	mfxReflectionType = mFX->GetVariableByName("gReflectionType")->AsScalar();
	mfxRefractionType = mFX->GetVariableByName("gRefractionType")->AsScalar();
	mfxEnvMap = mFX->GetVariableByName("gCubeMap")->AsShaderResource();
	mfxObjectDynamicCubeMapWorld = mFX->GetVariableByName("gObjectDynamicCubeMapWorld")->AsVector();
	mfxApproxSceneSz = mFX->GetVariableByName("gApproxSceneSz")->AsScalar();
	mfxPlanarRefractionMap = mFX->GetVariableByName("gPlanarRefractionMap")->AsShaderResource();

	//Shadow mapping
	mfxDirLightShadowTransformArray = mFX->GetVariableByName("gDirLightShadowTransformArray");
	mfxDirLightSplitDistances = mFX->GetVariableByName("gSplitDistances");
	mfxDirLightShadowMapArray = mFX->GetVariableByName("gDirLightShadowMapArray")->AsShaderResource();
	mfxDirLightIndex = mFX->GetVariableByName("gDirLightIndex")->AsScalar();

	mfxSpotLightShadowTransformArray = mFX->GetVariableByName("gSpotLightShadowTransformArray");
	mfxSpotLightShadowMapArray = mFX->GetVariableByName("gSpotLightShadowMapArray")->AsShaderResource();
	mfxSpotLightIndex = mFX->GetVariableByName("gSpotLightIndex")->AsScalar();

	mfxPointLightCubicShadowMapArray = mFX->GetVariableByName("gPointLightCubicShadowMapArray")->AsShaderResource();
	mfxPointLightIndex = mFX->GetVariableByName("gPointLightIndex")->AsScalar();

	mfxShadowType = mFX->GetVariableByName("gShadowType")->AsScalar();
	mBuildShadowMapBasicTech = mFX->GetTechniqueByName("BuildShadowMapBasicTech");
	mBuildShadowMapTessTech = mFX->GetTechniqueByName("BuildShadowMapTessTech");

	//Ssao
	mBuildNormalDepthMapBasicTech = mFX->GetTechniqueByName("BuildNormalDepthMapBasicTech");
	mBuildNormalDepthMapTessTech = mFX->GetTechniqueByName("BuildNormalDepthMapTessTech");

	mBuildAmbientMapBasicTech = mFX->GetTechniqueByName("BuildAmbientMapBasicTech");
	mBuildAmbientMapTessTech = mFX->GetTechniqueByName("BuildAmbientMapTessTech");

	mBuildDiffuseMapBasicTech = mFX->GetTechniqueByName("BuildDiffuseMapBasicTech");
	mBuildDiffuseMapTessTech = mFX->GetTechniqueByName("BuildDiffuseMapTessTech");

	SsaoMap = mFX->GetVariableByName("gSsaoMap")->AsShaderResource();
	mfxEnableSsao = mFX->GetVariableByName("gEnableSsao")->AsScalar();

	//SSLR
	mBuildDepthMapBasicTech = mFX->GetTechniqueByName("BuildDepthMapBasicTech");
	mBuildDepthMapTessTech = mFX->GetTechniqueByName("BuildDepthMapTessTech");

	mBuildWSPositionMapBasicTech = mFX->GetTechniqueByName("BuildWSPositionMapBasicTech");
	mBuildWSPositionMapTessTech = mFX->GetTechniqueByName("BuildWSPositionMapTessTech");

	mBuildSpecularMapBasicTech = mFX->GetTechniqueByName("BuildSpecularMapBasicTech");
	mBuildSpecularMapTessTech = mFX->GetTechniqueByName("BuildSpecularMapTessTech");

	SSLRMap = mFX->GetVariableByName("gSSLRMap")->AsShaderResource();
	mfxEnableSSLR = mFX->GetVariableByName("gEnableSSLR")->AsScalar();

	//PPLL
	mBuildPPLLBasicTech = mFX->GetTechniqueByName("BuildPPLLBasicTech");
	mBuildPPLLTessTech = mFX->GetTechniqueByName("BuildPPLLTessTech");

	//GPU particle-based phycics
	mBuildBodiesPPLLBasicTech = mFX->GetTechniqueByName("BuildBodiesPPLLBasicTech");
	mBuildBodiesPPLLTessTech = mFX->GetTechniqueByName("BuildBodiesPPLLTessTech");

	//Global illumination
	mGenerateGridBasicTech = mFX->GetTechniqueByName("GenerateGridBasicTech");
	mGenerateGridTessTech = mFX->GetTechniqueByName("GenerateGridTessTech");
	mfxEnableGI = mFX->GetVariableByName("gEnableGI")->AsScalar();

	//SVO building
	mBuildSvoBasicTech = mFX->GetTechniqueByName("BuildSVOBasicTech");
	mBuildSvoTessTech = mFX->GetTechniqueByName("BuildSVOTessTech");

	//SVO ray tracing
	SVORTMap = mFX->GetVariableByName("gSVORTMap")->AsShaderResource();
	mfxEnableRT = mFX->GetVariableByName("gEnableRT")->AsScalar();

	//Bounding volumes
	mfxBoundingSphereRadius = mFX->GetVariableByName("gBoundingSphereRadius")->AsScalar();
}

void QGE_GPU_Framework::BuildCubeFaceCamera(float x, float y, float z)
{
	XMFLOAT3 center(x, y, z);
	static const XMFLOAT3 worldUp(0.0f, 1.0f, 0.0f);

	const XMFLOAT3 targets[6] =
	{
		XMFLOAT3(x + 1.0f, y, z), // +X
		XMFLOAT3(x - 1.0f, y, z), // -X
		XMFLOAT3(x, y + 1.0f, z), // +Y
		XMFLOAT3(x, y - 1.0f, z), // -Y
		XMFLOAT3(x, y, z + 1.0f), // +Z
		XMFLOAT3(x, y, z - 1.0f)  // -Z
	};

	const XMFLOAT3 ups[6] =
	{
		XMFLOAT3(0.0f, 1.0f, 0.0f),  // +X
		XMFLOAT3(0.0f, 1.0f, 0.0f),  // -X
		XMFLOAT3(0.0f, 0.0f, -1.0f), // +Y
		XMFLOAT3(0.0f, 0.0f, +1.0f), // -Y
		XMFLOAT3(0.0f, 1.0f, 0.0f),	 // +Z
		XMFLOAT3(0.0f, 1.0f, 0.0f)	 // -Z
	};

	for (int i = 0; i < 6; ++i)
	{
		mCubeMapCamera[i].LookAt(center, targets[i], ups[i]);
		mCubeMapCamera[i].SetLens(0.5f*XM_PI, 1.0f, 0.1f, 1000.0f);
		mCubeMapCamera[i].UpdateViewMatrix();
	}
}

void QGE_GPU_Framework::BuildOffscreenViews()
{
	ReleaseCOM(mAmbientMapSRV);
	ReleaseCOM(mAmbientMapRTV);
	ReleaseCOM(mDiffuseMapSRV);
	ReleaseCOM(mDiffuseMapRTV);
	ReleaseCOM(mWSPositionMapSRV);
	ReleaseCOM(mWSPositionMapRTV);
	ReleaseCOM(mSpecularMapSRV);
	ReleaseCOM(mSpecularMapRTV);
	ReleaseCOM(mOffscreenSRV);
	ReleaseCOM(mOffscreenRTV);
	ReleaseCOM(mOffscreenUAV);

	D3D11_TEXTURE2D_DESC texDesc;

	texDesc.Width = mClientWidth;
	texDesc.Height = mClientHeight;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	ID3D11Texture2D* offscreenTex = 0;
	HR(md3dDevice->CreateTexture2D(&texDesc, 0, &offscreenTex));

	HR(md3dDevice->CreateShaderResourceView(offscreenTex, 0, &mOffscreenSRV));
	HR(md3dDevice->CreateRenderTargetView(offscreenTex, 0, &mOffscreenRTV));
	HR(md3dDevice->CreateUnorderedAccessView(offscreenTex, 0, &mOffscreenUAV));

	ReleaseCOM(offscreenTex);

	texDesc.Width = mClientWidth;
	texDesc.Height = mClientHeight;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	ID3D11Texture2D* ambientTex = 0;
	HR(md3dDevice->CreateTexture2D(&texDesc, 0, &ambientTex));

	HR(md3dDevice->CreateShaderResourceView(ambientTex, 0, &mAmbientMapSRV));
	HR(md3dDevice->CreateRenderTargetView(ambientTex, 0, &mAmbientMapRTV));

	ID3D11Texture2D* diffuseTex = 0;
	HR(md3dDevice->CreateTexture2D(&texDesc, 0, &diffuseTex));

	HR(md3dDevice->CreateShaderResourceView(diffuseTex, 0, &mDiffuseMapSRV));
	HR(md3dDevice->CreateRenderTargetView(diffuseTex, 0, &mDiffuseMapRTV));

	ReleaseCOM(diffuseTex);
	ReleaseCOM(ambientTex);

	D3D11_TEXTURE2D_DESC posTexDesc;

	posTexDesc.Width = mClientWidth;
	posTexDesc.Height = mClientHeight;
	posTexDesc.MipLevels = 1;
	posTexDesc.ArraySize = 1;
	posTexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	posTexDesc.SampleDesc.Count = 1;
	posTexDesc.SampleDesc.Quality = 0;
	posTexDesc.Usage = D3D11_USAGE_DEFAULT;
	posTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	posTexDesc.CPUAccessFlags = 0;
	posTexDesc.MiscFlags = 0;

	ID3D11Texture2D* wsPosTex = 0;
	HR(md3dDevice->CreateTexture2D(&posTexDesc, 0, &wsPosTex));

	HR(md3dDevice->CreateShaderResourceView(wsPosTex, 0, &mWSPositionMapSRV));
	HR(md3dDevice->CreateRenderTargetView(wsPosTex, 0, &mWSPositionMapRTV));

	ReleaseCOM(wsPosTex);

	D3D11_TEXTURE2D_DESC specTexDesc;

	specTexDesc.Width = mClientWidth;
	specTexDesc.Height = mClientHeight;
	specTexDesc.MipLevels = 1;
	specTexDesc.ArraySize = 1;
	specTexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	specTexDesc.SampleDesc.Count = 1;
	specTexDesc.SampleDesc.Quality = 0;
	specTexDesc.Usage = D3D11_USAGE_DEFAULT;
	specTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	specTexDesc.CPUAccessFlags = 0;
	specTexDesc.MiscFlags = 0;

	ID3D11Texture2D* specTex = 0;
	HR(md3dDevice->CreateTexture2D(&specTexDesc, 0, &specTex));

	HR(md3dDevice->CreateShaderResourceView(specTex, 0, &mSpecularMapSRV));
	HR(md3dDevice->CreateRenderTargetView(specTex, 0, &mSpecularMapRTV));

	ReleaseCOM(specTex);
}