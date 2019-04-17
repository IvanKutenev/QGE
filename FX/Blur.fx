#define GAUSSIAN 1
#define BILATERAL_LIN 2
#define BILATERAL_SQR 3

cbuffer cbSettings
{
	float gWeights[11] = 
	{
		0.05f, 0.05f, 0.1f, 0.1f, 0.1f, 0.2f, 0.1f, 0.1f, 0.1f, 0.05f, 0.05f,
	};
};

cbuffer cbFixed
{
	static const int gBlurRadius = 5;
	static const float gRangeCoeff = 0.05;
	static const float PI = 3.14159265f;
};

Texture2D gInput;
RWTexture2D<float4> gOutput;

#define N 256
#define CacheSize (N + 2*gBlurRadius)
groupshared float4 gCache[CacheSize];

float ComputeGaussian(float Sigma, float x)
{
	return exp(-(x*x) / (2 * Sigma * Sigma));
}

float IntensityLin(float4 color)
{
	return (color.r + color.g + color.b) / 3;
}
float4 IntensitySqr(float4 color)
{
	return sqrt(color.r*color.r + color.g*color.g + color.b*color.b) / sqrt(3);
}
[numthreads(N, 1, 1)]
void HorzBlurCS(int3 groupThreadID : SV_GroupThreadID,
				int3 dispatchThreadID : SV_DispatchThreadID, uniform int FilterType)
{
	//
	// Fill local thread storage to reduce bandwidth.  To blur 
	// N pixels, we will need to load N + 2*BlurRadius pixels
	// due to the blur radius.
	//
	
	// This thread group runs N threads.  To get the extra 2*BlurRadius pixels, 
	// have 2*BlurRadius threads sample an extra pixel.
	if(groupThreadID.x < gBlurRadius)
	{
		// Clamp out of bound samples that occur at image borders.
		int x = max(dispatchThreadID.x - gBlurRadius, 0);
		gCache[groupThreadID.x] = gInput[int2(x, dispatchThreadID.y)];
	}
	if(groupThreadID.x >= N-gBlurRadius)
	{
		// Clamp out of bound samples that occur at image borders.
		int x = min(dispatchThreadID.x + gBlurRadius, gInput.Length.x-1);
		gCache[groupThreadID.x+2*gBlurRadius] = gInput[int2(x, dispatchThreadID.y)];
	}

	// Clamp out of bound samples that occur at image borders.
	gCache[groupThreadID.x+gBlurRadius] = gInput[min(dispatchThreadID.xy, gInput.Length.xy-1)];

	// Wait for all threads to finish.
	GroupMemoryBarrierWithGroupSync();
	
	//
	// Now blur each pixel.
	//

	float4 CenterColor = gCache[groupThreadID.x + gBlurRadius];
	float4 result = CenterColor;
	float Normalization = 1;

	if (FilterType == GAUSSIAN)
	{
		float4 blurColor = float4(0, 0, 0, 0);

		[unroll]
		for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
		{
			int k = groupThreadID.x + gBlurRadius + i;

			blurColor += gWeights[i + gBlurRadius] * gCache[k];
		}

		gOutput[dispatchThreadID.xy] = blurColor;
	}
	else if (FilterType == BILATERAL_LIN)
	{
		[unroll]
		for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
		{
			int k = groupThreadID.x + gBlurRadius + i;
			float4 sampling = gCache[k];

			float GaussianCoeff = ComputeGaussian(gBlurRadius, i);
			//float GaussianCoeff = gWeights[i + gBlurRadius];
			float IntensityCoeff = ComputeGaussian(gRangeCoeff, IntensityLin(CenterColor) - IntensityLin(sampling));

			float sampleWeight = IntensityCoeff*GaussianCoeff;

			result += sampling*sampleWeight;
			Normalization += sampleWeight;

		}

		gOutput[dispatchThreadID.xy] = result / Normalization;
	}
	else
	{
		[unroll]
		for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
		{
			int k = groupThreadID.x + gBlurRadius + i;
			float4 sampling = gCache[k];

			float GaussianCoeff = ComputeGaussian(gBlurRadius, i);
			//float GaussianCoeff = gWeights[i + gBlurRadius];
			float IntensityCoeff = ComputeGaussian(gRangeCoeff, IntensitySqr(CenterColor) - IntensitySqr(sampling));

			float sampleWeight = IntensityCoeff*GaussianCoeff;

			result += sampling*sampleWeight;
			Normalization += sampleWeight;

		}

		gOutput[dispatchThreadID.xy] = result / Normalization;
	}
}

[numthreads(1, N, 1)]
void VertBlurCS(int3 groupThreadID : SV_GroupThreadID,
				int3 dispatchThreadID : SV_DispatchThreadID, uniform int FilterType)
{
	//
	// Fill local thread storage to reduce bandwidth.  To blur 
	// N pixels, we will need to load N + 2*BlurRadius pixels
	// due to the blur radius.
	//
	
	// This thread group runs N threads.  To get the extra 2*BlurRadius pixels, 
	// have 2*BlurRadius threads sample an extra pixel.
	if(groupThreadID.y < gBlurRadius)
	{
		// Clamp out of bound samples that occur at image borders.
		int y = max(dispatchThreadID.y - gBlurRadius, 0);
		gCache[groupThreadID.y] = gInput[int2(dispatchThreadID.x, y)];
	}
	if(groupThreadID.y >= N-gBlurRadius)
	{
		// Clamp out of bound samples that occur at image borders.
		int y = min(dispatchThreadID.y + gBlurRadius, gInput.Length.y-1);
		gCache[groupThreadID.y+2*gBlurRadius] = gInput[int2(dispatchThreadID.x, y)];
	}
	
	// Clamp out of bound samples that occur at image borders.
	gCache[groupThreadID.y+gBlurRadius] = gInput[min(dispatchThreadID.xy, gInput.Length.xy-1)];


	// Wait for all threads to finish.
	GroupMemoryBarrierWithGroupSync();
	
	//
	// Now blur each pixel.
	//

	float4 CenterColor = gCache[groupThreadID.y + gBlurRadius];
	float4 result = CenterColor;
	float Normalization = 1;

	if (FilterType == GAUSSIAN)
	{
		float4 blurColor = float4(0, 0, 0, 0);

		[unroll]
		for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
		{
			int k = groupThreadID.y + gBlurRadius + i;

			blurColor += gWeights[i + gBlurRadius] * gCache[k];
		}

		gOutput[dispatchThreadID.xy] = blurColor;
	}
	else if (FilterType == BILATERAL_LIN)
	{
		[unroll]
		for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
		{
			int k = groupThreadID.y + gBlurRadius + i;
			float4 sampling = gCache[k];

			float GaussianCoeff = ComputeGaussian(gBlurRadius, i);
			//float GaussianCoeff = gWeights[i + gBlurRadius];
			float IntensityCoeff = ComputeGaussian(gRangeCoeff, IntensityLin(CenterColor) - IntensityLin(sampling));

			float sampleWeight = IntensityCoeff*GaussianCoeff;

			result += sampling*sampleWeight;
			Normalization += sampleWeight;

		}

		gOutput[dispatchThreadID.xy] = result / Normalization;
	}
	else
	{
		[unroll]
		for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
		{
			int k = groupThreadID.y + gBlurRadius + i;
			float4 sampling = gCache[k];

			float GaussianCoeff = ComputeGaussian(gBlurRadius, i);
			//float GaussianCoeff = gWeights[i + gBlurRadius];
			float IntensityCoeff = ComputeGaussian(gRangeCoeff, IntensitySqr(CenterColor) - IntensitySqr(sampling));

			float sampleWeight = IntensityCoeff*GaussianCoeff;

			result += sampling*sampleWeight;
			Normalization += sampleWeight;

		}

		gOutput[dispatchThreadID.xy] = result / Normalization;
	}
}

technique11 HorzGaussianBlur
{
    pass P0
    {
		SetVertexShader( NULL );
        SetPixelShader( NULL );
		SetComputeShader( CompileShader( cs_5_0, HorzBlurCS(1) ) );
    }
}

technique11 VertGaussianBlur
{
    pass P0
    {
		SetVertexShader( NULL );
        SetPixelShader( NULL );
		SetComputeShader( CompileShader( cs_5_0, VertBlurCS(1) ) );
    }
}

technique11 HorzLinBilateralBlur
{
    pass P0
    {
		SetVertexShader( NULL );
        SetPixelShader( NULL );
		SetComputeShader( CompileShader( cs_5_0, HorzBlurCS(2) ) );
    }
}

technique11 VertLinBilateralBlur
{
    pass P0
    {
		SetVertexShader( NULL );
        SetPixelShader( NULL );
		SetComputeShader( CompileShader( cs_5_0, VertBlurCS(2) ) );
    }
}

technique11 HorzSqrBilateralBlur
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, HorzBlurCS(3)));
	}
}

technique11 VertSqrBilateralBlur
{
	pass P0
	{
		SetVertexShader(NULL);
		SetPixelShader(NULL);
		SetComputeShader(CompileShader(cs_5_0, VertBlurCS(3)));
	}
}
