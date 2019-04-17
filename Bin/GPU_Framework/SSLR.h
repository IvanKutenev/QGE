#ifndef _SSLR
#define _SSLR
#include "d3dUtil.h"

#define WEIGHTING_MODE_GLOSS 0
#define WEIGHTING_MODE_VISIBILITY 1

struct SSLRVertex
{
	XMFLOAT3 Pos;
	XMFLOAT2 Tex;
};

template<typename T>
T log2(T x)
{
	return log(x) / log(2.0f);
};

class SSLR
{
public:
	SSLR::SSLR(ID3D11Device* device, ID3D11DeviceContext* dc, float ClientW, float ClientH,
		ID3D11ShaderResourceView* BackbufferColorMapSRV, ID3D11ShaderResourceView* NormalDepthMapSRV,
		ID3D11ShaderResourceView* WSPositionMapSRV, ID3D11ShaderResourceView* SpecularBufferSRV, BOOL ForceMipsCount,
		UINT ForcedMipsCount, UINT WeightingMode, BOOL enableTraceIntervalRestrictions);
	void SSLR::RenderReflections(XMMATRIX ViewMat, XMMATRIX ProjMat, FLOAT NearZ, FLOAT FarZ);
	void SSLR::OnSize(float ClientW, float ClientH, ID3D11ShaderResourceView* BackbufferColorMapSRV, ID3D11ShaderResourceView* NormalDepthMapSRV,
		ID3D11ShaderResourceView* WSPositionMapSRV, ID3D11ShaderResourceView* SpecularBufferSRV);
	
	ID3D11ShaderResourceView* SSLR::GetHiZBufferSRV(UINT MipMapNum) const;
	ID3D11RenderTargetView* SSLR::GetHiZBufferRTV(UINT MipMapNum) const;
	ID3D11ShaderResourceView* SSLR::GetVisibilityBufferSRV() const;
	ID3D11ShaderResourceView* SSLR::GetVisibilityBufferSRV(UINT MipMapNum) const;
	ID3D11RenderTargetView* SSLR::GetVisibilityBufferRTV(UINT MipMapNum) const;
	ID3D11ShaderResourceView* SSLR::GetRayTracedPositionMapSRV() const;
	ID3D11RenderTargetView* SSLR::GetRayTracedPositionMapRTV() const;
	ID3D11ShaderResourceView* SSLR::GetPreConvolutionBufferSRV(UINT MipMapNum) const;
	ID3D11ShaderResourceView* SSLR::GetPreConvolutionBufferSRV() const;
	ID3D11RenderTargetView* SSLR::GetPreConvolutionBufferRTV(UINT MipMapNum) const;
	ID3D11ShaderResourceView* SSLR::GetSSLROutputSRV() const;
	ID3D11RenderTargetView* SSLR::GetSSLROutputRTV() const;
private:
	SSLR::SSLR(const SSLR& rhs);
	SSLR& SSLR::operator=(const SSLR& rhs);

	void SSLR::BuildFX();
	void SSLR::BuildTextureViews(float ClientW, float ClientH, ID3D11ShaderResourceView* BackbufferColorMapSRV, ID3D11ShaderResourceView* NormalDepthMapSRV,
		ID3D11ShaderResourceView* WSPositionMapSRV, ID3D11ShaderResourceView* SpecularBufferSRV);
	void SSLR::BuildFullScreenQuad();
	void SSLR::BuildInputLayout();

	void SSLR::HiZBuildPass();
	void SSLR::PreIntegrationPass(FLOAT NearZ, FLOAT FarZ);
	void SSLR::RayTracingPass(XMMATRIX ViewMat, XMMATRIX ProjMat);
	void SSLR::PreConvolutionPass();
	void SSLR::ConeTracingPass(XMMATRIX ViewMat);
private:
	ID3D11Device* md3dDevice;
	ID3D11DeviceContext* mDC;

	D3D11_VIEWPORT mScreenViewport;
	std::vector<D3D11_VIEWPORT>mScreenViewports;

	ID3D11InputLayout* mSSLRHiZInputLayout;
	ID3D11InputLayout* mSSLRPIInputLayout;
	ID3D11InputLayout* mSSLRRTInputLayout;
	ID3D11InputLayout* mSSLRPCInputLayout;
	ID3D11InputLayout* mSSLRCTInputLayout;

	ID3D11Buffer* mScreenQuadVB;
	ID3D11Buffer* mScreenQuadIB;

	ID3D11ShaderResourceView* mNormalDepthMapSRV;
	ID3D11ShaderResourceView* mWSPositionMapSRV;
	ID3D11ShaderResourceView* mSpecularBufferSRV;
	//////////////////////////////////////////////////

	//Hi-z generation pass
	ID3DX11Effect* SSLRBuildHiZFX;
	ID3DX11EffectTechnique* SSLRBuildHiZTech;
	ID3DX11EffectShaderResourceVariable* mfxInputDepthBufferMip;

	std::vector<ID3D11RenderTargetView*>mHiZBufferRTVs;
	std::vector<ID3D11ShaderResourceView*>mHiZBufferSRVs;
	ID3D11RenderTargetView* mHiZBufferRTV;
	ID3D11ShaderResourceView*  mHiZBufferSRV;
	//Pre-integration pass
	ID3DX11Effect* SSLRPreIntegrateFX;
	ID3DX11EffectTechnique* SSLRPreIntegrateTech;
	ID3DX11EffectShaderResourceVariable* mfxPrevHiZBufferMip;
	ID3DX11EffectShaderResourceVariable* mfxCurHiZBufferMip;
	ID3DX11EffectShaderResourceVariable* mfxPrevVisibilityBufferMip;
	ID3DX11EffectScalarVariable* mfxNearZ;
	ID3DX11EffectScalarVariable* mfxFarZ;

	std::vector<ID3D11ShaderResourceView*>mVisibilityBufferSRVs;
	std::vector<ID3D11RenderTargetView*>mVisibilityBufferRTVs;
	ID3D11RenderTargetView* mVisibilityBufferRTV;
	ID3D11ShaderResourceView* mVisibilityBufferSRV;
	//Ray tracing pass
	ID3DX11Effect* SSLRRayTraceFX;
	ID3DX11EffectTechnique* SSLRRayTraceTech;
	ID3DX11EffectShaderResourceVariable* mfxHiZBuffer;
	ID3DX11EffectShaderResourceVariable* mfxColorBuffer;
	ID3DX11EffectShaderResourceVariable* mfxNormalDepthMap;
	ID3DX11EffectShaderResourceVariable* mfxWSPositionMap;
	ID3DX11EffectScalarVariable* mfxClientWidth;
	ID3DX11EffectScalarVariable* mfxClientHeight;
	ID3DX11EffectMatrixVariable* mfxView;
	ID3DX11EffectMatrixVariable* mfxProj;
	ID3DX11EffectScalarVariable* mfxMIPS_COUNT;
	ID3DX11EffectScalarVariable* mfxEnableTraceIntervalRestrictions;
	ID3D11ShaderResourceView* mRayTracedPositionMapSRV;
	ID3D11RenderTargetView* mRayTracedPositionMapRTV;
	FLOAT ClientW;
	FLOAT ClientH;

	UINT MIPS_COUNT;
	BOOL mEnableTraceIntervalRestrictions;
	//Pre-convolution pass
	ID3DX11Effect* SSLRPreConvolutionFX;
	ID3DX11EffectTechnique* SSLRHorzBlurTech;
	ID3DX11EffectTechnique* SSLRVertBlurTech;
	ID3DX11EffectShaderResourceVariable* mfxInputMap;

	std::vector<ID3D11ShaderResourceView*> mPreConvolutionBufferSRVs;
	std::vector<ID3D11RenderTargetView*> mPreConvolutionBufferRTVs;
	std::vector<ID3D11ShaderResourceView*> mPingPongMapSRVs;
	std::vector<ID3D11RenderTargetView*> mPingPongMapRTVs;
	ID3D11ShaderResourceView* mPreConvolutionBufferSRV;
	//Cone-traing pass
	ID3DX11Effect* SSLRConeTraceFX;
	ID3DX11EffectTechnique* SSLRConeTraceTech;

	ID3DX11EffectScalarVariable* mfxCWidth;
	ID3DX11EffectScalarVariable* mfxCHeight;
	ID3DX11EffectMatrixVariable* mfxViewMat;
	ID3DX11EffectShaderResourceVariable* mfxVisibilityBuffer;
	ID3DX11EffectShaderResourceVariable* mfxSpecularBuffer;
	ID3DX11EffectShaderResourceVariable* mfxColorBuf;
	ID3DX11EffectShaderResourceVariable* mfxHiZBuf;
	ID3DX11EffectShaderResourceVariable* mfxRayTracedMap;
	ID3DX11EffectShaderResourceVariable* mfxWSPosMap;
	ID3D11ShaderResourceView* mSSLROutputSRV;
	ID3D11RenderTargetView* mSSLROutputRTV;
	ID3DX11EffectScalarVariable* mfxMipsCount;

	UINT mWeightingMode;
	ID3DX11EffectScalarVariable* mfxWeightingMode;
};
#endif