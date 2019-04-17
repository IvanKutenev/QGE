#ifndef _SSAO_H_
#define _SSAO_H_

#include "d3dUtil.h"
#include "Camera.h"

struct SsaoVertex
{
	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
	XMFLOAT2 Tex;
};

class Ssao
{
public:
	Ssao(ID3D11Device* device, ID3D11DeviceContext* dc, int width, int height, float fovy, float farZ);
	~Ssao();

	ID3D11ShaderResourceView* NormalDepthSRV();
	ID3D11ShaderResourceView* AmbientSRV();
	ID3D11RenderTargetView* NormalDepthRTV();

	void OnSize(float width, float height, float fovy, float farZ);

	void SetNormalDepthRenderTarget(ID3D11DepthStencilView* dsv);

	void ComputeSsao(const Camera& camera);

	void BlurAmbientMap(int blurCount);

	ID3D11ShaderResourceView* GetNormalDepthSRV(){ return mNormalDepthSRV; }
private:
	Ssao(const Ssao& rhs);

	void BlurAmbientMap(ID3D11ShaderResourceView* inputSRV, ID3D11RenderTargetView* outputRTV, bool horzBlur);

	void BuildFrustumFarCorners(float fovy, float farZ);

	void BuildFullScreenQuad();

	void BuildTextureViews();
	void ReleaseTextureViews();

	void BuildRandomVectorTexture();

	void BuildOffsetVectors();

	void BuildFX();

	void BuildInputLayout();
private:
	ID3D11Device* md3dDevice;
	ID3D11DeviceContext* mDC;

	ID3D11Buffer* mScreenQuadVB;
	ID3D11Buffer* mScreenQuadIB;

	ID3D11ShaderResourceView* mRandomVectorSRV;

	ID3D11RenderTargetView* mNormalDepthRTV;
	ID3D11ShaderResourceView* mNormalDepthSRV;

	// Need two for ping-ponging during blur.
	ID3D11RenderTargetView* mAmbientRTV0;
	ID3D11ShaderResourceView* mAmbientSRV0;

	ID3D11RenderTargetView* mAmbientRTV1;
	ID3D11ShaderResourceView* mAmbientSRV1;

	int mRenderTargetWidth;
	int mRenderTargetHeight;

	XMFLOAT4 mFrustumFarCorner[4];

	XMFLOAT4 mOffsets[14];

	D3D11_VIEWPORT mAmbientMapViewport;

	ID3D11InputLayout* mSsaoInputLayout;

	ID3DX11EffectTechnique* SsaoTech;
	ID3DX11Effect* mSsaoFX;

	ID3DX11EffectMatrixVariable* ViewToTexSpace;
	ID3DX11EffectVectorVariable* OffsetVectors;
	ID3DX11EffectVectorVariable* FrustumCorners;
	ID3DX11EffectScalarVariable* OcclusionRadius;
	ID3DX11EffectScalarVariable* OcclusionFadeStart;
	ID3DX11EffectScalarVariable* OcclusionFadeEnd;
	ID3DX11EffectScalarVariable* SurfaceEpsilon;

	ID3DX11EffectShaderResourceVariable* NormalDepthMap;
	ID3DX11EffectShaderResourceVariable* RandomVecMap;

	ID3DX11EffectTechnique* HorzBlurTech;
	ID3DX11EffectTechnique* VertBlurTech;
	ID3DX11Effect* mSsaoBlurFX;

	ID3DX11EffectScalarVariable* TexelWidth;
	ID3DX11EffectScalarVariable* TexelHeight;

	ID3DX11EffectShaderResourceVariable* blurPassNormalDepthMap;
	ID3DX11EffectShaderResourceVariable* InputImage;
};

#endif