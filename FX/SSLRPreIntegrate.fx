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

cbuffer cbCamFarNearPlanes
{
	float gFarZ;
	float gNearZ;
};

Texture2D gPrevHiZBufferMip;
Texture2D gCurHiZBufferMip;
Texture2D gPrevVisibilityBufferMip;

SamplerState samPointClamp
{
	Filter = MIN_MAG_MIP_POINT;

	AddressU = CLAMP;
	AddressV = CLAMP;
};

float linearizeDepth(float depth)
{
	return (gNearZ) / (gFarZ - depth * (gFarZ - gNearZ));
}

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	vout.PosH = float4(vin.PosL, 1.0f);
	vout.Tex = vin.Tex;
	return vout;
}

float PS(VertexOut pin) : SV_TARGET
{
	float4 fineZ;
	fineZ.x = linearizeDepth(gPrevHiZBufferMip.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(0, 0)).x);
	fineZ.y = linearizeDepth(gPrevHiZBufferMip.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(0, -1)).x);
	fineZ.z = linearizeDepth(gPrevHiZBufferMip.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(-1, 0)).x);
	fineZ.w = linearizeDepth(gPrevHiZBufferMip.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(-1, -1)).x);

	float minZ = linearizeDepth(gCurHiZBufferMip.SampleLevel(samPointClamp, pin.Tex, 0.0f).x);
	float maxZ = linearizeDepth(gCurHiZBufferMip.SampleLevel(samPointClamp, pin.Tex, 0.0f).y);

	float coarseVolume = 1.0f / (maxZ - minZ);

	float4 	visibility;
	visibility.x = gPrevVisibilityBufferMip.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(0, 0)).x;
	visibility.y = gPrevVisibilityBufferMip.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(0, -1)).x;
	visibility.z = gPrevVisibilityBufferMip.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(-1, 0)).x;
	visibility.w = gPrevVisibilityBufferMip.SampleLevel(samPointClamp, pin.Tex, 0.0f, int2(-1, -1)).x;

	float4 integration = (1.0f - saturate((fineZ.xyzw - minZ) * abs(coarseVolume))) * visibility.xyzw;
	return dot(0.25, integration.xyzw);
}

technique11 SSLRPreIntegrateTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, PS()));
	}
};