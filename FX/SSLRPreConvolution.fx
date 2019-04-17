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

Texture2D gInputMap;

SamplerState samPointClamp
{
	Filter = MIN_MAG_MIP_POINT;

	AddressU = CLAMP;
	AddressV = CLAMP;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	vout.PosH = float4(vin.PosL, 1.0f);
	vout.Tex = vin.Tex;
	return vout;
}

float4 HorzPS(VertexOut pin) : SV_TARGET
{
	float4 color = float4(0.0f, 0.0f, 0.0f, 1.0f);

	color += saturate(gInputMap.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(-3, 0))) * 0.001f;
	color += saturate(gInputMap.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(-2, 0))) * 0.028f;
	color += saturate(gInputMap.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(-1, 0))) * 0.233f;
	color += saturate(gInputMap.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(0, 0))) * 0.474f;
	color += saturate(gInputMap.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(1, 0))) * 0.233f;
	color += saturate(gInputMap.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(2, 0))) * 0.028f;
	color += saturate(gInputMap.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(3, 0))) * 0.001f;

	return color;
}

float4 VertPS(VertexOut pin) : SV_TARGET
{
	float4 color = float4(0.0f, 0.0f, 0.0f, 1.0f);

	color += saturate(gInputMap.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(0, -3))) * 0.001f;
	color += saturate(gInputMap.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(0, -2))) * 0.028f;
	color += saturate(gInputMap.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(0, -1))) * 0.233f;
	color += saturate(gInputMap.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(0, 0))) * 0.474f;
	color += saturate(gInputMap.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(0, 1))) * 0.233f;
	color += saturate(gInputMap.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(0, 2))) * 0.028f;
	color += saturate(gInputMap.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(0, 3))) * 0.001f;

	return color;
}

technique11 SSLRHorzBlurTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, HorzPS()));
	}
};

technique11 SSLRVertBlurTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, VertPS()));
	}
};