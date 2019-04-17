#ifndef _PPLL_H_
#define _PPLL_H_

#define MAX_PPLL_BUFFER_DEPTH 8//32
#define PPLL_BUFFER_CLEAN_VALUE 0xffffffff

#include "d3dUtil.h"

struct ListNode
{
	XMFLOAT4 PackedData;
	XMFLOAT2 DepthAndCoverage;
	UINT NextIndex;
};

class PPLL
{
private:
	ID3D11Device* md3dDevice;
	ID3D11DeviceContext* mDC;

	ID3DX11Effect* mPPLL_FX;

	ID3D11UnorderedAccessView* mHeadIndexBufferUAV;
	ID3D11ShaderResourceView* mHeadIndexBufferSRV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxHeadIndexBufferUAV;
	
	ID3D11Buffer* FragmentsListBuffer;
	ID3D11UnorderedAccessView* mFragmentsListBufferUAV;
	ID3D11ShaderResourceView* mFragmentsListBufferSRV;
	ID3DX11EffectUnorderedAccessViewVariable* mfxFragmentsListBufferUAV;

	ID3DX11EffectScalarVariable* mfxClientH;
	ID3DX11EffectScalarVariable* mfxClientW;

	FLOAT ClientH;
	FLOAT ClientW;
private:
	void PPLL::BuildFX();
	void PPLL::BuildBuffers();
public:
	PPLL::PPLL(ID3D11Device* device, ID3D11DeviceContext* dc, ID3DX11Effect* fx, FLOAT clientH, FLOAT clientW);

	void PPLL::Setup();
	void PPLL::OnSize(FLOAT clientH, FLOAT clientW);
	void PPLL::SetResources();

	ID3D11UnorderedAccessView* PPLL::GetHeadIndexBufferUAV() const { return mHeadIndexBufferUAV; };
	ID3D11UnorderedAccessView* PPLL::GetFragmentsListBufferUAV() const { return mFragmentsListBufferUAV; };

	ID3D11ShaderResourceView* PPLL::GetHeadIndexBufferSRV() const { return mHeadIndexBufferSRV; };
	ID3D11ShaderResourceView* PPLL::GetFragmentsListBufferSRV() const { return mFragmentsListBufferSRV; };
};

#endif
