#define WEIGHTING_MODE_GLOSS 0
#define WEIGHTING_MODE_VISIBILITY 1

struct VertexIn
{
	float3 PosL : POSITION;
	float2 Tex : TEXCOORD;
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float2 Tex : TEXCOORD;
};

SamplerState samPointClamp
{
	Filter = MIN_MAG_MIP_POINT;

	AddressU = CLAMP;
	AddressV = CLAMP;
};

SamplerState samTrilinearClamp
{
	Filter = MIN_MAG_MIP_LINEAR;

	AddressU = CLAMP;
	AddressV = CLAMP;
};

cbuffer cbPerFrame
{
	float4x4 gView;
	float gClientWidth;
	float gClientHeight;
	int MIPS_COUNT;
	int gWeightingMode;
}

Texture2D gVisibilityBuffer;
Texture2D gSpecularBuffer;
Texture2D gColorBuffer;
Texture2D gHiZBuffer;
Texture2D gRayTracedMap;
Texture2D gWSPositionMap;

static const float EPSILON = 0.0000001f;
static const float MAX_SPECULAR_EXP = 1024.0f;
static const float2 screenSize = float2(gClientWidth, gClientHeight);

float roughnessToSpecularPower(float sr)
{
	return 2.0f / (sr + EPSILON) / (sr + EPSILON) - 2.0f;
}

float specularPowerToConeAngle(float specularPower)
{
	if (specularPower >= exp2(MAX_SPECULAR_EXP))
	{
		return 0.0f;
	}
	const float xi = 0.244f;
	float exponent = 1.0f / (specularPower + 1.0f);
	return acos(pow(xi, exponent));
}

float isoscelesTriangleOpposite(float adjacentLength, float coneTheta)
{
	return 2.0f * tan(coneTheta * 0.5f) * adjacentLength;
}

float isoscelesTriangleInRadius(float a, float h)
{
	float a2 = a * a;
	float fh2 = 4.0f * h * h;
	return (a * (sqrt(a2 + fh2) - a)) / (4.0f * h);
}
float4 coneSampleVisibilityWeightedColor(float2 samplePos, float mipChannel)
{
	float3 SampledColor = gColorBuffer.SampleLevel(samTrilinearClamp, samplePos, mipChannel).rgb;
	float visibility = gVisibilityBuffer.SampleLevel(samTrilinearClamp, samplePos, mipChannel).r;
	
	return float4(SampledColor * visibility, visibility);
}

float4 coneSampleGlossWeightedColor(float2 samplePos, float mipChannel, float gloss)
{
	float3 SampledColor = gColorBuffer.SampleLevel(samTrilinearClamp, samplePos, mipChannel).rgb;
	
	return float4(SampledColor * gloss, gloss);
}

float isoscelesTriangleNextAdjacent(float adjacentLength, float incircleRadius)
{
	// subtract the diameter of the incircle to get the adjacent side of the next level on the cone
	return adjacentLength - (incircleRadius * 2.0f);
}


VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	vout.PosH = float4(vin.PosL, 1.0f);
	vout.Tex = vin.Tex;
	return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
	// calculate the ray
	float3 raySS = gRayTracedMap.SampleLevel(samTrilinearClamp, pin.Tex, 0).xyz;
	if (raySS.z < 0)
		return 0;
	// perform cone-tracing steps

	// get specular power from roughness
	float gloss = 1 - gSpecularBuffer.SampleLevel(samPointClamp, pin.Tex, 0.0f).y;
	float specularPower = roughnessToSpecularPower(1 - gloss);

	// convert to cone angle (maximum extent of the specular lobe aperture
	float coneTheta = specularPowerToConeAngle(specularPower);

	// P1 = positionSS, P2 = raySS, adjacent length = ||P2 - P1||

	// need to check if this is correct calculation or not
	float2 deltaP = raySS.xy - pin.Tex.xy;
	float adjacentLength = length(deltaP);

	// need to check if this is correct calculation or not
	float2 adjacentUnit = normalize(deltaP);

	float4 totalColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float glossMult = gloss;
	float remainingAlpha = 1.0f;
	// cone-tracing using an isosceles triangle to approximate a cone in screen space
	for (int i = 0; i < 14; ++i)
	{
		// intersection length is the adjacent side, get the opposite side using trig
		float oppositeLength = isoscelesTriangleOpposite(adjacentLength, coneTheta);

		// calculate in-radius of the isosceles triangle
		float incircleSize = isoscelesTriangleInRadius(oppositeLength, adjacentLength);

		// get the sample position in screen space
		float2 samplePos = pin.Tex.xy + adjacentUnit * (adjacentLength - incircleSize);

		// convert the in-radius into screen size then check what power N to raise 2 to reach it - that power N becomes mip level to sample from
		float mipChannel = clamp(log2(incircleSize * max(screenSize.x, screenSize.y)), 0, MIPS_COUNT - 1); // try this with min intead of max

		float4 newColor;
		switch (gWeightingMode)
		{
			case WEIGHTING_MODE_VISIBILITY:
				newColor = coneSampleVisibilityWeightedColor(samplePos, mipChannel);
				break;
			case WEIGHTING_MODE_GLOSS:
				newColor = coneSampleGlossWeightedColor(samplePos, mipChannel, glossMult);
				break;
			default:
				newColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
				break;
		};
		
		remainingAlpha -= newColor.a;
		if (remainingAlpha < 0.0f)
		{
			newColor.rgb *= (1.0f - abs(remainingAlpha));
		}
		totalColor += newColor;

		if (totalColor.a >= 1.0f)
		{
			break;
		}
		adjacentLength = isoscelesTriangleNextAdjacent(adjacentLength, incircleSize);
		glossMult *= gloss;
	}

	return float4(totalColor.rgb, 1.0f) + EPSILON;
}

technique11 SSLRConeTraceTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, PS()));
	}
}