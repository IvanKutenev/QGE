#ifndef _PARTICLE_SYSTEM_H_
#define _PARTICLE_SYSTEM_H_

#include "../Common/d3dUtil.h"
#include "Camera.h"

#define REFRACTION_RT_RESOLUTION 64

struct ParticleSystemVertex
{
	XMFLOAT3 InitialPos;
	XMFLOAT3 InitialVel;
	XMFLOAT2 Size;
	float Age;
	int Type;
};

struct ParticleSystemInitData {
	bool mFirstRun;
	float mGameTime;
	float mTimeStep;
	float mAge;

	int mMaxParticles;

	XMFLOAT3 mEmitPosW;
	XMFLOAT3 mEmitDirW;

	bool mUseTextures;
	bool mUseNormalMapping;
	bool mUseLighting;

	std::wstring mParticleEffectFilename;
	std::vector<std::wstring>&mParticleNormalMapFilenames;
	std::vector<std::wstring>&mParticleTexturesFilenames;
};

static ID3D11RenderTargetView* mRefractionRTV = NULL;
static ID3D11ShaderResourceView* mRefractionSRV = NULL;
static ID3D11DepthStencilView* mRefractionDSV = NULL;
static D3D11_VIEWPORT mRefractionViewport;

class ParticleSystem
{
private:
	ID3D11Device* mDevice;
	ID3D11DeviceContext* mDC;

	ID3DX11EffectTechnique* mStreamOutTech;
	ID3DX11EffectTechnique* mDrawTech;
	ID3DX11Effect* mFX;
	ID3D11InputLayout* mInputLayout;

	ID3DX11EffectMatrixVariable* mfxViewProj;
	ID3DX11EffectScalarVariable* mfxGameTime;
	ID3DX11EffectScalarVariable* mfxTimeStep;
	ID3DX11EffectVectorVariable* mfxEyePosW;
	ID3DX11EffectVectorVariable* mfxEmitPosW;
	ID3DX11EffectVectorVariable* mfxEmitDirW;
	ID3DX11EffectScalarVariable* mfxClientWidth;
	ID3DX11EffectScalarVariable* mfxClientHeight;
	ID3DX11EffectShaderResourceVariable* mfxTexArray;
	ID3DX11EffectShaderResourceVariable* mfxNormalArray;
	ID3DX11EffectShaderResourceVariable* mfxParticleCubeMap;
	ID3DX11EffectShaderResourceVariable* mfxRandomTex;
	ID3DX11EffectShaderResourceVariable* mfxRefractionTex;

	ID3D11Buffer* mInitVB;
	ID3D11Buffer* mDrawVB;
	ID3D11Buffer* mStreamOutVB;

	ID3D11ShaderResourceView* mTexArraySRV;
	ID3D11ShaderResourceView* mNormalArraySRV;
	ID3D11ShaderResourceView* mRandomTexSRV;

	ID3DX11EffectVariable* mfxDirLight;
	ID3DX11EffectVariable* mfxPointLight;
	ID3DX11EffectVariable* mfxSpotLight;

	ID3DX11EffectScalarVariable* mfxDirLightsNum;
	ID3DX11EffectScalarVariable* mfxSpotLightsNum;
	ID3DX11EffectScalarVariable* mfxPointLightsNum;

	ID3DX11EffectScalarVariable* mfxUseTextures;
	ID3DX11EffectScalarVariable* mfxUseNormalMapping;
	ID3DX11EffectScalarVariable* mfxUseLighting;

	int mMaxParticles;
	bool mFirstRun;

	float mGameTime;
	float mTimeStep;
	float mAge;

	XMFLOAT3 mEmitPosW;
	XMFLOAT3 mEmitDirW;

	bool mUseTextures;
	bool mUseNormalMapping;
	bool mUseLighting;

	static ID3D11RenderTargetView* mRefractionRTV;
	static ID3D11ShaderResourceView* mRefractionSRV;
	static ID3D11DepthStencilView* mRefractionDSV;
	static D3D11_VIEWPORT mRefractionViewport;
private:
	void ParticleSystem::BuildVB();
	void ParticleSystem::BuildFX(std::wstring& filename);
	void ParticleSystem::BuildVertexInputLayout();
public:
	static void ParticleSystem::BuildRefractionViews(ID3D11Device* device);
	ParticleSystem::ParticleSystem(ID3D11Device* device, ID3D11DeviceContext* dc, ParticleSystemInitData* psid);

	float ParticleSystem::GetAge() const;
	void ParticleSystem::SetEmitPos(const XMFLOAT3& emitPosW);
	void ParticleSystem::SetEmitDir(const XMFLOAT3& emitDirW);

	void ParticleSystem::Reset(ParticleSystemInitData* psid);
	void ParticleSystem::Update(float dt, float gameTime);
	
	void ParticleSystem::Draw(const Camera& cam, ID3D11RenderTargetView** renderTargets, ID3D11DepthStencilView* depthStencilView, FLOAT clientH, FLOAT clientW,
		ID3D11ShaderResourceView* envMapSRV, DirectionalLight* dirLight, UINT dirLightsNum, PointLight* pointLight, UINT pointLightsNum, SpotLight* spotLight, UINT spotLightsNum);

	static ID3D11RenderTargetView* ParticleSystem::GetRefractionRTV() { return ParticleSystem::mRefractionRTV; };
	static ID3D11DepthStencilView* ParticleSystem::GetRefractionDSV() { return ParticleSystem::mRefractionDSV; };
	static ID3D11ShaderResourceView* ParticleSystem::GetRefractionSRV() { return ParticleSystem::mRefractionSRV; };
	static D3D11_VIEWPORT ParticleSystem::GetRefractionViewport() { return ParticleSystem::mRefractionViewport; };
};
#endif
