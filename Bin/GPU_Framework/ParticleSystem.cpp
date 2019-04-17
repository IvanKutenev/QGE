#include "ParticleSystem.h"

ID3D11RenderTargetView* ParticleSystem::mRefractionRTV;
ID3D11ShaderResourceView* ParticleSystem::mRefractionSRV;
ID3D11DepthStencilView* ParticleSystem::mRefractionDSV;
D3D11_VIEWPORT ParticleSystem::mRefractionViewport;

ParticleSystem::ParticleSystem(ID3D11Device* device, ID3D11DeviceContext* dc, ParticleSystemInitData* psid)
{
	mDevice = device;
	mDC = dc;

	mMaxParticles = psid->mMaxParticles;
	mFirstRun = psid->mFirstRun;
	mGameTime = psid->mGameTime;
	mTimeStep = psid->mTimeStep;
	mAge = psid->mAge;

	mEmitPosW = psid->mEmitPosW;
	mEmitDirW = psid->mEmitDirW;

	mRandomTexSRV = d3dHelper::CreateRandomTexture1DSRV(device);

	if (mUseTextures = psid->mUseTextures)
		mTexArraySRV = d3dHelper::CreateTexture2DArraySRVW(mDevice, mDC, psid->mParticleTexturesFilenames);
	else
		mTexArraySRV = NULL;

	if (mUseNormalMapping = psid->mUseNormalMapping)
		mNormalArraySRV = d3dHelper::CreateTexture2DArraySRVW(mDevice, mDC, psid->mParticleNormalMapFilenames);
	else
		mNormalArraySRV = NULL;

	mUseLighting = psid->mUseLighting;

	BuildFX(psid->mParticleEffectFilename);
	BuildVertexInputLayout();
	BuildVB();
}

void ParticleSystem::Reset(ParticleSystemInitData* psid)
{
	mFirstRun = psid->mFirstRun;
	mGameTime = psid->mGameTime;
	mTimeStep = psid->mTimeStep;
	mAge = psid->mAge;
}

float ParticleSystem::GetAge() const
{
	return mAge;
}

void ParticleSystem::SetEmitPos(const XMFLOAT3& emitPosW)
{
	mEmitPosW = emitPosW;
}

void ParticleSystem::SetEmitDir(const XMFLOAT3& emitDirW)
{
	mEmitDirW = emitDirW;
}

void ParticleSystem::Update(float dt, float gameTime)
{
	mGameTime = gameTime;
	mTimeStep = dt;
	mAge += dt;
}

void ParticleSystem::Draw(const Camera& cam, ID3D11RenderTargetView** renderTargets, ID3D11DepthStencilView* depthStencilView, FLOAT clientH, FLOAT clientW,
	ID3D11ShaderResourceView* envMapSRV, DirectionalLight* dirLight, UINT dirLightsNum, PointLight* pointLight, UINT pointLightsNum, SpotLight* spotLight, UINT spotLightsNum)
{
	mDC->OMSetRenderTargets(1, renderTargets, depthStencilView);

	XMMATRIX VP = cam.ViewProj();

	mfxViewProj->SetMatrix(reinterpret_cast<const float*>(&VP));
	mfxGameTime->SetFloat(mGameTime);
	mfxTimeStep->SetFloat(mTimeStep);
	mfxEyePosW->SetRawValue(&cam.GetPosition(), 0, sizeof(XMFLOAT3));
	mfxEmitPosW->SetRawValue(&mEmitPosW, 0, sizeof(XMFLOAT3));
	mfxEmitDirW->SetRawValue(&mEmitDirW, 0, sizeof(XMFLOAT3));
	mfxTexArray->SetResource(mTexArraySRV);
	mfxNormalArray->SetResource(mNormalArraySRV);
	mfxParticleCubeMap->SetResource(envMapSRV);
	mfxRandomTex->SetResource(mRandomTexSRV);
	mfxRefractionTex->SetResource(mRefractionSRV);

	mfxDirLightsNum->SetInt(static_cast<int>(dirLightsNum));
	mfxDirLight->SetRawValue(dirLight, 0, MAX_DIR_LIGHTS_NUM * sizeof(DirectionalLight));

	mfxSpotLightsNum->SetInt(static_cast<int>(spotLightsNum));
	mfxSpotLight->SetRawValue(spotLight, 0, MAX_SPOT_LIGHTS_NUM * sizeof(SpotLight));

	mfxPointLightsNum->SetInt(static_cast<int>(pointLightsNum));
	mfxPointLight->SetRawValue(pointLight, 0, MAX_POINT_LIGHTS_NUM * sizeof(PointLight));

	mfxClientHeight->SetFloat(clientH);
	mfxClientWidth->SetFloat(clientW);

	mDC->IASetInputLayout(mInputLayout);
	mDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	const UINT stride = sizeof(ParticleSystemVertex);
	const UINT offset = 0;

	if (mFirstRun)
		mDC->IASetVertexBuffers(0, 1, &mInitVB, &stride, &offset);
	else
		mDC->IASetVertexBuffers(0, 1, &mDrawVB, &stride, &offset);

	mDC->SOSetTargets(1, &mStreamOutVB, &offset);

	D3DX11_TECHNIQUE_DESC techDesc;
	mStreamOutTech->GetDesc(&techDesc);
	for (int p = 0; p < techDesc.Passes; ++p)
	{
		mStreamOutTech->GetPassByIndex(p)->Apply(0, mDC);

		if (mFirstRun)
		{
			mDC->Draw(1, 0);
			mFirstRun = false;
		}
		else
		{
			mDC->DrawAuto();
		}
	}

	// done streaming-out--unbind the vertex buffer
	ID3D11Buffer* bufferArray[1] = { 0 };
	mDC->SOSetTargets(1, bufferArray, &offset);

	// ping-pong the vertex buffers
	std::swap(mDrawVB, mStreamOutVB);

	//
	// Draw the updated particle system we just streamed-out. 
	//
	mDC->IASetVertexBuffers(0, 1, &mDrawVB, &stride, &offset);

	mDrawTech->GetDesc(&techDesc);
	for (int p = 0; p < techDesc.Passes; ++p)
	{
		mDrawTech->GetPassByIndex(p)->Apply(0, mDC);
		mDC->DrawAuto();
	}
}

void ParticleSystem::BuildVB()
{
	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_DEFAULT;
	vbd.ByteWidth = sizeof(ParticleSystemVertex) * 1;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;

	ParticleSystemVertex p;
	ZeroMemory(&p, sizeof(ParticleSystemVertex));
	p.Age = 0.0f;
	p.Type = 0;

	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &p;

	HR(mDevice->CreateBuffer(&vbd, &vinitData, &mInitVB));

	vbd.ByteWidth = sizeof(ParticleSystemVertex) * mMaxParticles;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_STREAM_OUTPUT;

	HR(mDevice->CreateBuffer(&vbd, 0, &mDrawVB));
	HR(mDevice->CreateBuffer(&vbd, 0, &mStreamOutVB));
}

void ParticleSystem::BuildFX(std::wstring& filename) {

	mFX = d3dHelper::CreateEffectFromMemory(filename.c_str(), mDevice);

	mStreamOutTech = mFX->GetTechniqueByName("StreamOutTech");
	mDrawTech = mFX->GetTechniqueByName("DrawTech");

	mfxViewProj = mFX->GetVariableByName("gViewProj")->AsMatrix();
	mfxGameTime = mFX->GetVariableByName("gGameTime")->AsScalar();
	mfxTimeStep = mFX->GetVariableByName("gTimeStep")->AsScalar();
	mfxEyePosW = mFX->GetVariableByName("gEyePosW")->AsVector();
	mfxEmitPosW = mFX->GetVariableByName("gEmitPosW")->AsVector();
	mfxEmitDirW = mFX->GetVariableByName("gEmitDirW")->AsVector();
	mfxTexArray = mFX->GetVariableByName("gTexArray")->AsShaderResource();
	mfxNormalArray = mFX->GetVariableByName("gNormalArray")->AsShaderResource();
	mfxParticleCubeMap = mFX->GetVariableByName("gParticleCubeMap")->AsShaderResource();
	mfxRandomTex = mFX->GetVariableByName("gRandomTex")->AsShaderResource();
	mfxDirLight = mFX->GetVariableByName("gDirLight");
	mfxSpotLight = mFX->GetVariableByName("gSpotLight");
	mfxPointLight = mFX->GetVariableByName("gPointLight");
	mfxDirLightsNum = mFX->GetVariableByName("gDirLightsNum")->AsScalar();
	mfxSpotLightsNum = mFX->GetVariableByName("gSpotLightsNum")->AsScalar();
	mfxPointLightsNum = mFX->GetVariableByName("gPointLightsNum")->AsScalar();
	mfxRefractionTex = mFX->GetVariableByName("gParticleSystemRefractionTex")->AsShaderResource();
	mfxClientWidth = mFX->GetVariableByName("gClientWidth")->AsScalar();
	mfxClientHeight = mFX->GetVariableByName("gClientHeight")->AsScalar();
}

void ParticleSystem::BuildVertexInputLayout()
{
	static const D3D11_INPUT_ELEMENT_DESC ParticleDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "VELOCITY", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "AGE", 0, DXGI_FORMAT_R32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TYPE", 0, DXGI_FORMAT_R32_UINT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3DX11_PASS_DESC passDesc;

	mDrawTech->GetPassByIndex(0)->GetDesc(&passDesc);

	HR(mDevice->CreateInputLayout(ParticleDesc, 5, passDesc.pIAInputSignature,
		passDesc.IAInputSignatureSize, &mInputLayout));
}

void ParticleSystem::BuildRefractionViews(ID3D11Device* device)
{
	mRefractionRTV = 0;

	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = REFRACTION_RT_RESOLUTION;
	texDesc.Height = REFRACTION_RT_RESOLUTION;
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
	HR(device->CreateTexture2D(&texDesc, 0, &Tex));

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = texDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2DArray.ArraySize = 1;
	rtvDesc.Texture2DArray.MipSlice = 0;

	HR(device->CreateRenderTargetView(Tex, &rtvDesc, &mRefractionRTV));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = -1;

	HR(device->CreateShaderResourceView(Tex, &srvDesc, &mRefractionSRV));

	ReleaseCOM(Tex);

	D3D11_TEXTURE2D_DESC depthTexDesc;
	depthTexDesc.Width = REFRACTION_RT_RESOLUTION;
	depthTexDesc.Height = REFRACTION_RT_RESOLUTION;
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
	HR(device->CreateTexture2D(&depthTexDesc, 0, &depthTex));

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Format = depthTexDesc.Format;
	dsvDesc.Flags = 0;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	HR(device->CreateDepthStencilView(depthTex, &dsvDesc, &mRefractionDSV));

	ReleaseCOM(depthTex);

	InitViewport(&mRefractionViewport, 0.0f, 0.0f, REFRACTION_RT_RESOLUTION, REFRACTION_RT_RESOLUTION, 0.0f, 1.0f);
};