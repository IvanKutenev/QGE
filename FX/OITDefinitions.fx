#define MAX_PPLL_BUFFER_DEPTH 8//32

static float4x4 toTexSpace =
{
	0.5f, 0.0f, 0.0f, 0.0f,
	0.0f, -0.5f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.5f, 0.5f, 0.0f, 1.0f
};

struct ListNode
{
	float4 PackedData;
	float2 DepthAndCoverage;//coverage for MSAA
	uint NextIndex;
};

StructuredBuffer<ListNode>FragmentsListBuffer;
Texture2D<uint>HeadIndexBuffer;

cbuffer cbOIT
{
	uint ClientH;
	uint ClientW;
};

struct VertexIn
{
	float3 PosL : POSITION;
	float2 Tex : TEXCOORD;
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float2 Tex : TEXCOORD0;
	float4 PosSS : TEXCOORD1;
};