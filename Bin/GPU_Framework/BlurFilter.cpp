#include "BlurFilter.h"

BlurFilter::BlurFilter(ID3D11Device* device, int width, int height, DXGI_FORMAT format, bool Enable4xMsaa, int MsaaQuality)
	: mBlurredOutputTexSRV(0), mBlurredOutputTexUAV(0), pBlurEffect(0)
{
	OnSize(device, width, height, format, Enable4xMsaa, MsaaQuality);
	pBlurEffect = new BlurEffect(device, L"FX/Blur.cso");
}

BlurFilter::~BlurFilter()
{
	ReleaseCOM(mBlurredOutputTexSRV);
	ReleaseCOM(mBlurredOutputTexUAV);
}

ID3D11ShaderResourceView* BlurFilter::GetBlurredOutput()
{
	return mBlurredOutputTexSRV;
}

void BlurFilter::SetGaussianWeights(float sigma)
{
	float d = 2.0f*sigma*sigma;

	float weights[9];
	float sum = 0.0f;
	for (int i = 0; i < 8; ++i)
	{
		float x = (float)i;
		weights[i] = expf(-x*x / d);

		sum += weights[i];
	}

	// Divide by the sum so all the weights add up to 1.0.
	for (int i = 0; i < 8; ++i)
	{
		weights[i] /= sum;
	}

	pBlurEffect->SetWeights(weights);
}

void BlurFilter::SetWeights(const float weights[9])
{
	pBlurEffect->SetWeights(weights);
}

void BlurFilter::OnSize(ID3D11Device* device, int width, int height, DXGI_FORMAT format, bool Enable4xMsaa, int MsaaQuality)
{
	// Start fresh.
	ReleaseCOM(mBlurredOutputTexSRV);
	ReleaseCOM(mBlurredOutputTexUAV);

	mWidth = width;
	mHeight = height;
	mFormat = format;

	// Note, compressed formats cannot be used for UAV.  We get error like:
	// ERROR: ID3D11Device::CreateTexture2D: The format (0x4d, BC3_UNORM) 
	// cannot be bound as an UnorderedAccessView, or cast to a format that
	// could be bound as an UnorderedAccessView.  Therefore this format 
	// does not support D3D11_BIND_UNORDERED_ACCESS.

	D3D11_TEXTURE2D_DESC blurredTexDesc;
	blurredTexDesc.Width = width;
	blurredTexDesc.Height = height;
	blurredTexDesc.MipLevels = 1;
	blurredTexDesc.ArraySize = 1;
	blurredTexDesc.Format = format;
	// Use 4X MSAA? --must match swap chain MSAA values.
	if (Enable4xMsaa)
	{
		blurredTexDesc.SampleDesc.Count = 4;
		blurredTexDesc.SampleDesc.Quality = MsaaQuality - 1;
	}
	// No MSAA
	else
	{
		blurredTexDesc.SampleDesc.Count = 1;
		blurredTexDesc.SampleDesc.Quality = 0;
	}
	blurredTexDesc.SampleDesc.Count = 1;
	blurredTexDesc.SampleDesc.Quality = 0;
	blurredTexDesc.Usage = D3D11_USAGE_DEFAULT;
	blurredTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	blurredTexDesc.CPUAccessFlags = 0;
	blurredTexDesc.MiscFlags = 0;

	ID3D11Texture2D* blurredTex = 0;
	HR(device->CreateTexture2D(&blurredTexDesc, 0, &blurredTex));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	HR(device->CreateShaderResourceView(blurredTex, &srvDesc, &mBlurredOutputTexSRV));

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.Format = format;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	HR(device->CreateUnorderedAccessView(blurredTex, &uavDesc, &mBlurredOutputTexUAV));

	// Views save a reference to the texture so we can release our reference.
	ReleaseCOM(blurredTex);
}

void BlurFilter::BlurInPlace(ID3D11DeviceContext* dc,
	ID3D11ShaderResourceView* inputSRV,
	ID3D11UnorderedAccessView* inputUAV,
	int blurCount)
{
	//
	// Run the compute shader to blur the offscreen texture.
	// 

	ID3DX11EffectTechnique* ChosenHorzBlurTech[3] = {
		pBlurEffect->HorzGaussianBlurTech,
		pBlurEffect->HorzLinBilateralBlurTech,
		pBlurEffect->HorzSqrBilateralBlurTech
	};
	ID3DX11EffectTechnique* ChosenVertBlurTech[3] = {
		pBlurEffect->VertGaussianBlurTech,
		pBlurEffect->VertLinBilateralBlurTech,
		pBlurEffect->VertSqrBilateralBlurTech
	};

	for (int i = 0; i < blurCount; ++i)
	{
		// HORIZONTAL blur pass.
		D3DX11_TECHNIQUE_DESC techDesc;
		ChosenHorzBlurTech[BlurEffect::ChosenBlurTech]->GetDesc(&techDesc);
		for (int p = 0; p < techDesc.Passes; ++p)
		{
			pBlurEffect->SetInputMap(inputSRV);
			pBlurEffect->SetOutputMap(mBlurredOutputTexUAV);
			ChosenHorzBlurTech[BlurEffect::ChosenBlurTech]->GetPassByIndex(p)->Apply(0, dc);

			// How many groups do we need to dispatch to cover a row of pixels, where each
			// group covers 256 pixels (the 256 is defined in the ComputeShader).
			int numGroupsX = (int)ceilf(mWidth / 256.0f);
			dc->Dispatch(numGroupsX, mHeight, 1);
		}

		// Unbind the input texture from the CS for good housekeeping.
		ID3D11ShaderResourceView* nullSRV[1] = { 0 };
		dc->CSSetShaderResources(0, 1, nullSRV);

		// Unbind output from compute shader (we are going to use this output as an input in the next pass, 
		// and a resource cannot be both an output and input at the same time.
		ID3D11UnorderedAccessView* nullUAV[1] = { 0 };
		dc->CSSetUnorderedAccessViews(0, 1, nullUAV, 0);

		// VERTICAL blur pass.
		ChosenVertBlurTech[BlurEffect::ChosenBlurTech]->GetDesc(&techDesc);
		for (int p = 0; p < techDesc.Passes; ++p)
		{
			pBlurEffect->SetInputMap(mBlurredOutputTexSRV);
			pBlurEffect->SetOutputMap(inputUAV);
			ChosenVertBlurTech[BlurEffect::ChosenBlurTech]->GetPassByIndex(p)->Apply(0, dc);

			// How many groups do we need to dispatch to cover a column of pixels, where each
			// group covers 256 pixels  (the 256 is defined in the ComputeShader).
			int numGroupsY = (int)ceilf(mHeight / 256.0f);
			dc->Dispatch(mWidth, numGroupsY, 1);
		}

		dc->CSSetShaderResources(0, 1, nullSRV);
		dc->CSSetUnorderedAccessViews(0, 1, nullUAV, 0);
	}

	// Disable compute shader.
	dc->CSSetShader(0, 0, 0);
}