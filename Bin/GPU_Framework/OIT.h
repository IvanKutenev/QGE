#ifndef _OIT_H_
#define _OIT_H_

#include "d3dUtil.h"
#include "RenderStates.h"

struct OITVertex
{
	XMFLOAT3 PosL;
	XMFLOAT2 Tex;
};

class OIT
{
private:
	ID3D11Device* md3dDevice;
	ID3D11DeviceContext* mDC;

	ID3D11Buffer* mScreenQuadVB;
	ID3D11Buffer* mScreenQuadIB;

	ID3D11InputLayout* mOITInputLayout;

	ID3DX11Effect* mOIT_FX;
	ID3DX11EffectTechnique* OITDrawTech;

	ID3D11ShaderResourceView* mHeadIndexBufferSRV;
	ID3DX11EffectShaderResourceVariable* mfxHeadIndexBufferSRV;

	ID3D11ShaderResourceView* mFragmentsListBufferSRV;
	ID3DX11EffectShaderResourceVariable* mfxFragmentsListBufferSRV;

	ID3DX11EffectScalarVariable* mfxClientH;
	ID3DX11EffectScalarVariable* mfxClientW;

	FLOAT ClientH;
	FLOAT ClientW;

	D3D11_VIEWPORT mScreenViewport;

	ID3D11ShaderResourceView* mOIT_SRV;
	ID3D11RenderTargetView* mOIT_RTV;

	RenderStates* pRenderStates;
private:
	void OIT::BuildFX(void);
	void OIT::BuildInputLayout(void);
	void OIT::BuildOffscreenQuad(void);
	void OIT::BuildTextureViews(void);
public:
	OIT::OIT(ID3D11Device* device, ID3D11DeviceContext* dc, ID3D11ShaderResourceView* headibSRV, ID3D11ShaderResourceView* fraglbSRV, FLOAT clientH, FLOAT clientW);	
	void OIT::OnSize(ID3D11ShaderResourceView* headibSRV, ID3D11ShaderResourceView* fraglbSRV, FLOAT clientH, FLOAT clientW);
	void OIT::Compute(void);
	void OIT::Compute(ID3D11RenderTargetView** rtv);
	ID3D11ShaderResourceView* OIT::GetOIT_SRV(void) const { return mOIT_SRV; };

};

#endif
