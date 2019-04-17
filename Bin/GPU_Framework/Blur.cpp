#include "Blur.h"

BlurEffect::BlurEffect(ID3D11Device* device, const std::wstring& filename)
{
	ID3DX11Effect* FX = d3dHelper::CreateEffectFromMemory(filename, device);

	HorzGaussianBlurTech = FX->GetTechniqueByName("HorzGaussianBlur");
	VertGaussianBlurTech = FX->GetTechniqueByName("VertGaussianBlur");

	HorzLinBilateralBlurTech = FX->GetTechniqueByName("HorzLinBilateralBlur");
	VertLinBilateralBlurTech = FX->GetTechniqueByName("VertLinBilateralBlur");

	HorzSqrBilateralBlurTech = FX->GetTechniqueByName("HorzSqrBilateralBlur");
	VertSqrBilateralBlurTech = FX->GetTechniqueByName("VertSqrBilateralBlur");

	Weights = FX->GetVariableByName("gWeights")->AsScalar();
	InputMap = FX->GetVariableByName("gInput")->AsShaderResource();
	OutputMap = FX->GetVariableByName("gOutput")->AsUnorderedAccessView();
}