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

Texture2D gInputDepthBufferMip;

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	vout.PosH = float4(vin.PosL, 1.0f);
	vout.Tex = vin.Tex;
	return vout;
}

float2 PS(VertexOut pin) : SV_TARGET
{
	float2 texcoords = pin.Tex;
	float4 minDepth = 0.0f;
	float4 maxDepth = 0.0f;

	float2 tx = gInputDepthBufferMip.SampleLevel(samPointClamp, texcoords, 0.0f, int2(0, 0)).rg;
	minDepth.r = tx.r;
	maxDepth.r = tx.g;

	float2 ty = gInputDepthBufferMip.SampleLevel(samPointClamp, texcoords, 0.0f, int2(0, -1)).rg;
	minDepth.g = ty.r;
	maxDepth.g = ty.g;

	float2 tz = gInputDepthBufferMip.SampleLevel(samPointClamp, texcoords, 0.0f, int2(-1, 0)).rg;
	minDepth.b = tz.r;
	maxDepth.b = tz.g;

	float2 tw = gInputDepthBufferMip.SampleLevel(samPointClamp, texcoords, 0.0f, int2(-1, -1)).rg;
	minDepth.a = tw.r;
	maxDepth.a = tw.g;

	return float2(
		min(min(minDepth.r, minDepth.g), min(minDepth.b, minDepth.a)),
		max(max(maxDepth.r, maxDepth.g), max(maxDepth.b, maxDepth.a)));
}

technique11 SSLRBuildHiZTech
{
	pass P0
	{
		SetVertexShader( CompileShader( vs_5_0, VS() ) );
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader( CompileShader( ps_5_0, PS() ) );
	}
}