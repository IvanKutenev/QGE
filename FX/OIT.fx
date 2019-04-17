#include "OITSort.fx"

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	vout.PosH = float4(vin.PosL, 1.0f);
	vout.PosSS = mul(vout.PosH, toTexSpace);
	vout.Tex = vin.Tex;
	return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
	pin.PosSS /= pin.PosSS.w;
	uint2 screenXY = uint2(ClientW, ClientH) * pin.PosSS.xy;

	if (HeadIndexBuffer[screenXY] == 0xffffffff)
		discard;

	uint Indices[MAX_PPLL_BUFFER_DEPTH];
	int IndicesCount = 0;
	[unroll(MAX_PPLL_BUFFER_DEPTH)]
	for (int CurIndex = HeadIndexBuffer[screenXY]; CurIndex != 0xffffffff && IndicesCount < MAX_PPLL_BUFFER_DEPTH; CurIndex = FragmentsListBuffer[CurIndex].NextIndex)
	{
		Indices[IndicesCount] = CurIndex;
		++IndicesCount;
	}
	
	ListNode Data[MAX_PPLL_BUFFER_DEPTH];
	for (int i = 0; i < IndicesCount; ++i)
		Data[i] = FragmentsListBuffer[Indices[i]];
	
	InsertionSort(Data, IndicesCount);

	float3 resColor = float3(0.0f, 0.0f, 0.0f);
	float alpha = 1.0f;

	[unroll(MAX_PPLL_BUFFER_DEPTH)]
	for (int i = 0; i < IndicesCount; ++i)
	{
		alpha *= 1.0f - Data[i].PackedData.a;
		resColor = lerp(resColor, Data[i].PackedData.rgb, Data[i].PackedData.a);
	}

	return float4(resColor, alpha);
}

technique11 OITDrawTech
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