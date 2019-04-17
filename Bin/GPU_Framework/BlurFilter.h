#ifndef _BLUR_FILTER
#define _BLUR_FILTER

#include <directxmath.h> 
#include "Blur.h"

class BlurFilter
{
public:
	BlurFilter(ID3D11Device* device, int width, int height, DXGI_FORMAT format, bool Enable4xMsaa, int MsaaQuality);

	void OnSize(ID3D11Device* device, int width, int height, DXGI_FORMAT format, bool Enable4xMsaa, int MsaaQuality);

	void BlurInPlace(ID3D11DeviceContext* dc, ID3D11ShaderResourceView* inputSRV, ID3D11UnorderedAccessView* inputUAV, int blurCount);

	ID3D11ShaderResourceView* GetBlurredOutput();
private:
	// Generate Gaussian blur weights.
	void SetGaussianWeights(float sigma);

	// Manually specify blur weights.
	void SetWeights(const float weights[9]);

	~BlurFilter();
private:
	BlurEffect* pBlurEffect;

	int mWidth;
	int mHeight;
	DXGI_FORMAT mFormat;

	ID3D11ShaderResourceView* mBlurredOutputTexSRV;
	ID3D11UnorderedAccessView* mBlurredOutputTexUAV;
};

#endif