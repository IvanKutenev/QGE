#ifndef _BLUR_EFFECT
#define _BLUR_EFFECT

#include "../Common/d3dUtil.h"

class BlurEffect
{
public:
	BlurEffect(ID3D11Device* device, const std::wstring& filename);

	void SetWeights(const float weights[9]){ Weights->SetFloatArray(weights, 0, 9); }
	void SetInputMap(ID3D11ShaderResourceView* tex){ InputMap->SetResource(tex); }
	void SetOutputMap(ID3D11UnorderedAccessView* tex){ OutputMap->SetUnorderedAccessView(tex); }

	ID3DX11EffectTechnique* HorzGaussianBlurTech; // Tech 0
	ID3DX11EffectTechnique* VertGaussianBlurTech; //
	
	ID3DX11EffectTechnique* HorzLinBilateralBlurTech; // Tech 1
	ID3DX11EffectTechnique* VertLinBilateralBlurTech; //

	ID3DX11EffectTechnique* HorzSqrBilateralBlurTech; // Tech 2
	ID3DX11EffectTechnique* VertSqrBilateralBlurTech; //

	ID3DX11EffectScalarVariable* Weights;
	ID3DX11EffectShaderResourceVariable* InputMap;
	ID3DX11EffectUnorderedAccessViewVariable* OutputMap;

	static const INT ChosenBlurTech  = 1;
};

#endif