//Shadow mapping
#define CASCADE_COUNT 4
#define DRAW_SHADOW_TYPE_DIRECTIONAL_LIGHT 0
#define DRAW_SHADOW_TYPE_SPOT_LIGHT 1
#define DRAW_SHADOW_TYPE_POINT_LIGHT 2
#define DRAW_SHADOW_TYPE_NO_SHADOW 3

//Lighting
#define MAX_DIR_LIGHTS_NUM 3
#define MAX_POINT_LIGHTS_NUM 3
#define MAX_SPOT_LIGHTS_NUM 3

//Texrtures constants
#define TEX_TYPE_DIFFUSE_MAP 0.0f
#define TEX_TYPE_SHININESS_MAP 1.0f
#define TEX_TYPE_NORMAL_MAP 2.0f
#define TEX_TYPE_TRANSPARENT_MAP 3.0f
#define TEX_TYPE_UNKNOWN 4.0f

#define MAX_TEXTURE_COUNT 10
#define MAX_INSTANCES_COUNT 200

//Mesh type
#define PROCEDURAL_MESH 0
#define FBX_MESH 1
#define CUSTOM_FORMAT_MESH 2

//Reflection/refraction
#define REFLECTION_TYPE_NONE 0
#define REFLECTION_TYPE_STATIC_CUBE_MAP 1
#define REFLECTION_TYPE_DYNAMIC_CUBE_MAP 2
#define REFLECTION_TYPE_STATIC_CUBE_MAP_AND_SSLR 3
#define REFLECTION_TYPE_DYNAMIC_CUBE_MAP_AND_SSLR 4
#define REFLECTION_TYPE_SSLR 5
#define REFLECTION_TYPE_SVO_CONE_TRACING 6
#define REFRACTION_TYPE_NONE 0
#define REFRACTION_TYPE_STATIC_CUBE_MAP 1
#define REFRACTION_TYPE_DYNAMIC_CUBE_MAP 2
#define REFRACTION_TYPE_PLANAR 3

#define MESH_PLANAR_REFRACTION_DISTORTION 0.011f

//draw pass types
#define DRAW_PASS_TYPE_COLOR 0
#define DRAW_PASS_TYPE_NORMAL_DEPTH 1
#define DRAW_PASS_TYPE_AMBIENT 2
#define DRAW_PASS_TYPE_DIFFUSE 3
#define DRAW_PASS_TYPE_SPECULAR 4
#define DRAW_PASS_TYPE_DEPTH 5
#define DRAW_PASS_TYPE_POSITION_WS 6
#define DRAW_PASS_TYPE_COLOR_NO_REFLECTION 7
#define DRAW_PASS_TYPE_MATERIAL_SPECULAR 8
#define DRAW_PASS_TYPE_SHADOW_MAP_BUILD 9

//Material type
#define MTL_TYPE_REFLECT_MAP 1 // <- material is provided by gReflectMap texture
#define MTL_TYPE_DEFAULT 0     //<- material is provided by gMaterial structure

//Water surface rendering parameters
#define WORLD_POSITION_CLIP_TRESHOLD 0.5f
#define WATER_PLANAR_REFRACTION_DISTORTION 0.02f
#define WATER_PLANAR_REFLECTION_DISTORTION 0.02f

static const float SMAP_SIZE = 4096.0f;
static const float SMAP_DX = 1.0f / SMAP_SIZE;
static const float offset_X = SMAP_DX;
static const float offset_Y = SMAP_DX;
static const float offset_Z = SMAP_DX;

static const float PCF_KERNEL_SZ = -1;// Example: -1 -> 3x3 PCF blur, -4 -> 9x9 PCF blur, x (x<0) -> (abs(x)*2+1)x(abs(x)*2+1) PCF blur
static const float BLOCKER_FINDER_KERNEL_SZ = -1;// -||-

static const float DEPTH_BIAS = 0.009f;
static const float NORMAL_OFFSET = 0.5f;

static const float DIR_LIGHT_PENUMBRA_SCALE = 2.0f;
static const float DIR_LIGHT_MIN_PENUMBRA_SZ = 0.05f;// 0.0f <= MIN_PENUMBRA_SZ < inf
static const float DIR_LIGHT_MAX_PENUMBRA_SZ = 100.0f;// 0.0f < MAX_PENUMBRA_SZ < inf , MIN_PENUMBRA_SZ < MAX_PENUMBRA_SZ
static const float DIR_LIGHT_SIZE = 100.0f;
static const float DIR_LIGHT_HEIGHT = 2000.0f;

static const float SPOT_LIGHT_PENUMBRA_SCALE = 1.0f;
static const float SPOT_LIGHT_MIN_PENUMBRA_SZ = 0.08f;// 0.0f <= MIN_PENUMBRA_SZ < inf
static const float SPOT_LIGHT_MAX_PENUMBRA_SZ = 100.0f;// 0.0f < MAX_PENUMBRA_SZ < inf , MIN_PENUMBRA_SZ < MAX_PENUMBRA_SZ
static const float SPOT_LIGHT_SIZE = 5.0f;

static const float POINT_LIGHT_PENUMBRA_SCALE = 1.0f;
static const float POINT_LIGHT_MIN_PENUMBRA_SZ = 0.05f;// 0.0f <= MIN_PENUMBRA_SZ < inf
static const float POINT_LIGHT_MAX_PENUMBRA_SZ = 100.0f;// 0.0f < MAX_PENUMBRA_SZ < inf , MIN_PENUMBRA_SZ < MAX_PENUMBRA_SZ
static const float POINT_LIGHT_SIZE = 5.0f;

static const int PCF_UNROLL_VALUE = abs(PCF_KERNEL_SZ) * 2 + 1;
static const int BLOCKER_FINDER_UNROLL_VALUE = abs(BLOCKER_FINDER_KERNEL_SZ) * 2 + 1;

static const float EPSILON = 0.0000001f;

RasterizerState WireframeRS{
	FillMode = Wireframe;
	CullMode = back;
	FrontCounterClockwise = false;
	MultisampleEnable = true;
};

RasterizerState SolidRS{
	FillMode = Solid;
	CullMode = back;
	FrontCounterClockwise = false;
	MultisampleEnable = true;
};

RasterizerState NoCullRS{
	FillMode = Solid;
	CullMode = none;
	FrontCounterClockwise = false;
	MultisampleEnable = true;
};

RasterizerState RenderBackfaceRS {
	FillMode = Solid;
	CullMode = front;
	FrontCounterClockwise = false;
	MultisampleEnable = true;
};

struct Material
{
	float4 Ambient;
	float4 Diffuse;
	float4 Specular;
	float3 Reflect;
};

struct DirectionalLight
{
	float4 Ambient;
	float4 Diffuse;
	float4 Specular;
	float3 Direction;
	float pad;
};

struct PointLight
{
	float4 Ambient;
	float4 Diffuse;
	float4 Specular;

	float3 Position;
	float Range;

	float3 Attenuation;
	float pad;
};

struct SpotLight
{
	float4 Ambient;
	float4 Diffuse;
	float4 Specular;

	float3 Position;
	float Range;

	float3 Direction;
	float Spot;

	float3 Attenuation;
	float pad;
};

SamplerState samAnisotropic
{
	Filter = ANISOTROPIC;
	MaxAnisotropy = 16;

	AddressU = WRAP;
	AddressV = WRAP;
};

SamplerState samLinear
{
	Filter = MIN_MAG_MIP_LINEAR;

	AddressU = WRAP;
	AddressV = WRAP;
};

SamplerState samShadow
{
	Filter = MIN_MAG_MIP_LINEAR;

	AddressU = BORDER;
	AddressV = BORDER;
};

SamplerState samHeightmap
{
	Filter = MIN_MAG_LINEAR_MIP_POINT;

	AddressU = CLAMP;
	AddressV = CLAMP;
};

SamplerState samPointClamp
 {
	Filter = MIN_MAG_MIP_POINT;

	AddressU = CLAMP;
	AddressV = CLAMP;
 };

cbuffer cbLightsAndShadowing
{
	DirectionalLight gDirLight[16];
	PointLight gPointLight[16];
	SpotLight gSpotLight[16];

	float4 gSplitDistances[CASCADE_COUNT];
	row_major float4x4 gDirLightShadowTransformArray[CASCADE_COUNT * MAX_DIR_LIGHTS_NUM];
	float gDirLightIndex;

	row_major float4x4 gSpotLightShadowTransformArray[MAX_POINT_LIGHTS_NUM];
	float gSpotLightIndex;

	float gPointLightIndex;

	int gDirLightsNum;
	int gPointLightsNum;
	int gSpotLightsNum;
};

cbuffer cbCamera
{
	float3 gEyePosW;
};

cbuffer cbTimer
{
	float gTime;
};

cbuffer cbTesselation
{
	float gMaxTessFactor;
	float gMaxLODdist;
	float gMinLODdist;
	bool gEnableDynamicLOD;
	float gConstantLODTessFactor;
};
cbuffer cbWaterSurface
{
	float gWaterHeight;
};

cbuffer cbPerFrameFlags
{
	bool gDrawWaterReflection;
	bool gDrawWaterRefraction;
	bool gEnableSSLR;
	bool gEnableSsao;
	bool gEnableRT;
	bool gEnableGI;
	int gDrawPassType;
	int gShadowType;
};

cbuffer cbTerrain
{
	float gMinDist;
	float gMaxDist;
	float gMinTess;
	float gMaxTess;
	float gTexelCellSpaceU;
	float gTexelCellSpaceV;
	float gWorldCellSpace;
	float2 gTexScale = 50.0f;
	float4 gWorldFrustrumPlanes[6];
};

cbuffer cbObject
{
	float4x4 gWorld[MAX_INSTANCES_COUNT];
	float4x4 gView;
	float4x4 gProj;
	float4x4 gInvWorld[MAX_INSTANCES_COUNT];
	float4x4 gWorldViewProj[MAX_INSTANCES_COUNT];
	float4x4 gViewProj;
	float4x4 gWorldInvTranspose[MAX_INSTANCES_COUNT];
	row_major float4x4 gTexTransform[MAX_TEXTURE_COUNT];

	bool gUseTexture;
	bool gUseExtTextureArray; // determine which type of tex array should use
	bool gUseTextureArray;
	bool gUseBlackClipping;
	bool gUseAlphaClipping;
	bool gUseBlending;
	bool gUseDisplacementMapping;
	bool gUseNormalMapping;

	float4 gTexType[MAX_TEXTURE_COUNT];
	
	float3 Pad;
	int gTexCount;

	float3 gObjectDynamicCubeMapWorld[MAX_INSTANCES_COUNT];
	int gMeshType;

	float gApproxSceneSz;
	float gHeightScale;
	
	int gReflectionType;
	int gRefractionType;

	Material gMaterial;
	int gMaterialType;

	float gBoundingSphereRadius;
};

Texture2DArray gDiffuseMapArray;
Texture2D gExtDiffuseMapArray[MAX_TEXTURE_COUNT]; // used to store textures of different types

Texture2D gReflectMap;
Texture2D gNormalMap;

TextureCube gCubeMap;
Texture2D gPlanarRefractionMap;
Texture2D gSSLRMap;
Texture2D gSsaoMap;
Texture2D gSVORTMap;

Texture2DArray gLayerMapArray;
Texture2DArray gLayerNormalMapArray;

Texture2D gBlendMap;
Texture2D gHeightMap;

Texture2D gWaterReflectionTex;
Texture2D gWaterRefractionTex;

Texture2DArray gDirLightShadowMapArray;
Texture2DArray gSpotLightShadowMapArray;
TextureCubeArray gPointLightCubicShadowMapArray;

static float4x4 toTexSpace = 
{
	0.5f, 0.0f, 0.0f, 0.0f,
	0.0f, -0.5f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.5f, 0.5f, 0.0f, 1.0f 
};

static float4x4 Mr =
{
	0.5, 0, 0, 0,
	0, 0.5, 0, 0,
	0, 0, 0.5, 0,
	0.5, 0.5, 0.5, 1
};

struct VertexIn
{
	float3 PosL : POSITION;
	float3 NormalL : NORMAL;
	float2 Tex : TEXCOORD;
	float3 TangentL : TANGENT;
};

struct ShaderOut
{
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
	float2 Tex : TEXCOORD0;
	float4 PosSS: TEXCOORD1;
	float3 TangentW : TANGENT;
	uint InstanceID : INSTANCE_ID;
};

struct HullOut{
	float3 PosW : POSITION;
	float2 Tex  : TEXCOORD0;
};

struct DomainOut{
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float2 Tex : TEXCOORD0;
	float2 TiledTex : TEXCOORD1;
	float4 PosSS : TEXCOORD2;
};

struct TerrainVertexIn{
	float3 PosL: POSITION;
	float2 Tex: TEXCOORD0;
	float2 BoundsY: TEXCOORD1;
};

struct TerrainVertexOut{
	float3 PosW: POSITION;
	float2 Tex: TEXCOORD0;
	float2 BoundsY: TEXCOORD1;
};

struct ControlPoints{
	float3 B210 : POSITION3;
	float3 B120 : POSITION4;
	float3 B021 : POSITION5;
	float3 B012 : POSITION6;
	float3 B102 : POSITION7;
	float3 B201 : POSITION8;
	float3 B111 : CENTER;

	float3 N110 : NORMAL3;
	float3 N011 : NORMAL4;
	float3 N101 : NORMAL5;
};

struct PatchTess{
	float EdgeTess[3] : SV_TessFactor;
	float InsideTess : SV_InsideTessFactor;

	ControlPoints controlP;
};

struct TerrainPatchTess
{
	float EdgeTess[4] : SV_TessFactor;
	float InsideTess[2] : SV_InsideTessFactor;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float CalcTessFactor(float3 p){

	float d = distance(p, gEyePosW);

	float s = saturate((d - gMinDist) / (gMaxDist - gMinDist));

	return pow(2, (lerp(gMaxTess, gMinTess, s)));
}

bool AABBBehindPlaneTest(float3 center, float3 extents, float4 plane)
{
	float3 n = abs(plane.xyz);

		float r = dot(extents, n);

	float s = dot(float4(center, 1.0f), plane);

	return (s + r) < 0.0f;
}

bool AABBOutsideFrustrumTest(float3 center, float3 extents, float4 frustrumPlanes[6])
{
	[unroll(6)]
	for (int i = 0; i < 6; ++i)
	{
		if (AABBBehindPlaneTest(center, extents, frustrumPlanes[i]))
		{
			return true;
		}
	}
	return false;
}

float3 calculatePositionfromUVW(float4 triangleVP[3], float3 barycentricCoords, ControlPoints controlP)
{
	float u = barycentricCoords.x;
	float v = barycentricCoords.y;
	float w = barycentricCoords.z;


	float uu = u * u;
	float vv = v * v;
	float ww = w * w;
	float uu3 = uu * 3.0f;
	float vv3 = vv * 3.0f;
	float ww3 = ww * 3.0f;

	float3 position = triangleVP[0] * ww * w +
		triangleVP[1] * uu * u +
		triangleVP[2] * vv * v +
		controlP.B210 * ww3 * u +
		controlP.B120 * w * uu3 +
		controlP.B201 * ww3 * v +
		controlP.B021 * uu3 * v +
		controlP.B102 * w * vv3 +
		controlP.B012 * u * vv3 +
		controlP.B111 * 6.0f * w * u * v;

	return position;
}

float3 calculateNormalfromUVW(float3 triangleN[3], float3 barycentricCoords, ControlPoints controlP)
{
	float u = barycentricCoords.x;
	float v = barycentricCoords.y;
	float w = barycentricCoords.z;

	float uu = u * u;
	float vv = v * v;
	float ww = w * w;
	float3 normal = triangleN[0] * ww +
		triangleN[1] * uu +
		triangleN[2] * vv +
		controlP.N110 * w * u +
		controlP.N011 * u * v +
		controlP.N101 * w * v;
	normal = normalize(normal);

	return normal;
}

float2 interpolateTexC(float2 tex1, float2 tex2, float2 tex3, float3 barycentricCoord)
{
	return tex2 * barycentricCoord.x + tex3 * barycentricCoord.y + tex1 * barycentricCoord.z;
}

ControlPoints calculateControlPoints(float3 pos[3], float3 normal[3])
{
	ControlPoints retval = (ControlPoints)0;

	float3 B300 = pos[0];
	float3 B030 = pos[1];
	float3 B003 = pos[2];

	float3 N200 = normal[0];
	float3 N020 = normal[1];
	float3 N002 = normal[2];

	retval.B210 = (2 * B300 + B030 - dot((B030 - B300), N200) * N200) / 3;
	retval.B120 = (2 * B030 + B300 - dot((B300 - B030), N020) * N020) / 3;

	retval.B021 = (2 * B030 + B003 - dot((B003 - B030), N020) * N020) / 3;
	retval.B012 = (2 * B003 + B030 - dot((B030 - B003), N002) * N002) / 3;

	retval.B102 = (2 * B003 + B300 - dot((B300 - B003), N002) * N002) / 3;
	retval.B201 = (2 * B300 + B003 - dot((B003 - B300), N200) * N200) / 3;

	float3 E = (retval.B210 + retval.B120 + retval.B021 + retval.B012 + retval.B102 + retval.B201) / 6;
	float3 V = (B003 + B030 + B300) / 3;
	retval.B111 = E + (E - V) / 2;

	float v12 = 2.0f * dot((B030 - B300), (N200 + N020)) / dot((B030 - B300), (B030 - B300));
	float v23 = 2.0f * dot((B003 - B030), (N020 + N002)) / dot((B003 - B030), (B003 - B030));
	float v31 = 2.0f * dot((B300 - B003), (N002 + N200)) / dot((B300 - B003), (B300 - B003));

	retval.N110 = normalize(N200 + N020 - v12 * (B030 - B300));
	retval.N011 = normalize(N020 + N002 - v23 * (B003 - B030));
	retval.N101 = normalize(N002 + N200 - v31 * (B300 - B003));

	return retval;
}

int wrap(int inval, int max, int min)
{
	if (inval < min)
		return max - (min - inval);
	else if (inval >= max)
		return min + (inval - max);
	else
		return inval;
}

float3 ComputeNormal(float3 v1, float3 v2, float3 v3)
{
	float3 a, b, c, p, q, n;

	a = v1;
	b = v2;
	c = v3;

	p = b - a;
	q = c - a;

	n = cross(p, q);
	n = normalize(n);

	return n;
}

float3 ComputeTangent(float3 normal)
{
	float3 t, b, c1, c2;
	c1 = cross(normal, float3(0.0, 0.0, 1.0));
	c2 = cross(normal, float3(0.0, 1.0, 0.0));
	if (length(c1) > length(c2))
		t = c1;
	else
		t = c2;
	t = normalize(t);
	return t;
}

float3 ComputeBinormal(float3 normal, float3 tangent)
{
	return normalize(cross(normal, tangent));
}

float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
	// Uncompress each component from [0,1] to [-1,1].
	float3 normalT = 2.0f*normalMapSample - 1.0f;

	// Build orthonormal basis.
	float3 N = unitNormalW;
	float3 T = normalize(tangentW - dot(tangentW, N)*N);
	float3 B = cross(N, T);

	float3x3 TBN = float3x3(T, B, N);

	// Transform from tangent space to world space.
	float3 bumpedNormalW = mul(normalT, TBN);

	return bumpedNormalW;
}

void Subdivide(ShaderOut inVerts[3], out ShaderOut outVerts[6])
{
	ShaderOut m[3];

	m[0].PosW = 0.5f*(inVerts[0].PosW + inVerts[1].PosW);
	m[1].PosW = 0.5f*(inVerts[1].PosW + inVerts[2].PosW);
	m[2].PosW = 0.5f*(inVerts[2].PosW + inVerts[0].PosW);

	m[0].NormalW = m[0].PosW;
	m[1].NormalW = m[1].PosW;
	m[2].NormalW = m[2].PosW;

	m[0].Tex = 0.5f*(inVerts[0].Tex + inVerts[1].Tex);
	m[1].Tex = 0.5f*(inVerts[1].Tex + inVerts[2].Tex);
	m[2].Tex = 0.5f*(inVerts[2].Tex + inVerts[0].Tex);

	m[0].PosH = m[0].PosH;
	m[1].PosH = m[1].PosH;
	m[2].PosH = m[2].PosH;

	outVerts[0] = inVerts[0];
	outVerts[1] = m[0];
	outVerts[2] = m[2];
	outVerts[3] = m[1];
	outVerts[4] = inVerts[2];
	outVerts[5] = inVerts[1];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float FresnelCoeff(float n, float k){
	if (k == -1)
		return pow(1.0 - n, 2.0) / pow(1.0 + n, 2.0);
	else
		return (pow(1.0 - n, 2.0) + k*k) / (pow(1.0 + n, 2.0) + k*k);
}

float FresnelSchlickApprox(float FresnelCoeff, float3 ViewVector, float3 NormalVector)
{
	ViewVector = normalize(ViewVector);
	NormalVector = normalize(NormalVector);

	return saturate(FresnelCoeff + pow(1 - dot(ViewVector, NormalVector), 5.0f)*(1 - FresnelCoeff));
}

float BeckmannDistribCoeff(float3 LightSourcePos, float3 ViewVector, float3 NormalVector, float SurfaceRoughness)
{
	float3 H = normalize(ViewVector + LightSourcePos);

	return 1 / (4 * SurfaceRoughness*SurfaceRoughness*pow(dot(H, NormalVector), 4.0f))*exp((pow(dot(H, NormalVector), 2.0f) - 1) / (SurfaceRoughness*SurfaceRoughness*(pow(dot(H, NormalVector), 2.0f))));
}

float GeometryCoeff(float3 View, float3 Normal, float3 LightPos)
{
	float3 H = normalize(View + LightPos);
	float G1 = min(1.0f, 2 * dot(H, Normal)*dot(View, Normal) / dot(View, Normal));
	float G2 = min(1.0f, 2 * dot(H, Normal)*dot(LightPos, Normal) / dot(View, Normal));

	return min(G1, G2);
}

float CookTorrance(float3 Normal, float3 LightPos, float3 View, float SurfaceRoughness, float n, float k)
{
	if (SurfaceRoughness <= 0.0f)
		return 0.0f;
	else{
		float VdotN = dot(Normal, View);
		float LdotN = dot(LightPos, Normal);
		float F = FresnelSchlickApprox(FresnelCoeff(n, k), View, Normal);
		float G = GeometryCoeff(View, Normal, LightPos);
		float D = BeckmannDistribCoeff(LightPos, View, Normal, SurfaceRoughness);
		return min(1.0f, F*G*D / (VdotN*LdotN + 0.0000001));
	}
}

//////////////////////////////Light source computing//////////////////////////////////////////////////

void ComputeDirectionalLight(Material M, float3 normal, float3 EyePos, int dirLightNum,
	out float4 ambient,
	out float4 diffuse,
	out float4 specular)
{
	ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
	diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
	specular = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float3 lightVec = -gDirLight[dirLightNum].Direction;

	ambient = M.Ambient*gDirLight[dirLightNum].Ambient;

	float DiffuseFactor = dot(normal, lightVec);

	if (DiffuseFactor > 0.0f)
	{
		float3 ReflectedRay = reflect(-lightVec, normal);
		float n = gMaterial.Reflect.x;
		float sr = gMaterial.Reflect.y;
		float k = gMaterial.Reflect.z;
		float SpecularFactor = CookTorrance(normal, lightVec, EyePos, sr, n, k);

		specular = SpecularFactor*M.Specular*gDirLight[dirLightNum].Specular;
		diffuse = DiffuseFactor*M.Diffuse*gDirLight[dirLightNum].Diffuse;
	}
}

void ComputePointLight(Material M, float3 pos, float3 normal,
	float3 EyePos, int pointLightNum,
	out float4 ambient,
	out float4 diffuse,
	out float4 specular)
{
	ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
	diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
	specular = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float3 lightVec = gPointLight[pointLightNum].Position - pos;

	float d = length(lightVec);

	if (d > gPointLight[pointLightNum].Range)
		return;

	lightVec /= d;

	ambient = M.Ambient*gPointLight[pointLightNum].Ambient;

	float DiffuseFactor = dot(lightVec, normal);

	if (DiffuseFactor > 0.0f)
	{
		float3 ReflectedRay = reflect(-lightVec, normal);

		float n = gMaterial.Reflect.x;
		float sr = gMaterial.Reflect.y;
		float k = gMaterial.Reflect.z;
		float SpecularFactor = CookTorrance(normal, lightVec, EyePos, sr, n, k);

		specular = SpecularFactor*M.Specular*gPointLight[pointLightNum].Specular;
		diffuse = DiffuseFactor*M.Diffuse*gPointLight[pointLightNum].Diffuse;
	}

	if (gPointLight[pointLightNum].Attenuation.x == 0 && gPointLight[pointLightNum].Attenuation.y == 0 && gPointLight[pointLightNum].Attenuation.z == 0)
		;
	else
	{
		float Attenuation = 1.0 / length(gPointLight[pointLightNum].Attenuation*float3(1.0f, d, d*d));

		specular *= Attenuation;
		diffuse *= Attenuation;
	}
}

void ComputeSpotLight(Material M, float3 pos, float3 normal,
	float3 EyePos, int spotLightNum,
	out float4 ambient,
	out float4 diffuse,
	out float4 specular)
{
	ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
	diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
	specular = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float3 lightVec = gSpotLight[spotLightNum].Position - pos;

	float d = length(lightVec);

	if (d > gSpotLight[spotLightNum].Range)
		return;

	lightVec /= d;

	ambient = M.Ambient*gSpotLight[spotLightNum].Ambient;

	float DiffuseFactor = dot(lightVec, normal);

	if (DiffuseFactor > 0.0f)
	{
		float3 ReflectedRay = reflect(-lightVec, normal);

		float n = gMaterial.Reflect.x;
		float sr = gMaterial.Reflect.y;
		float k = gMaterial.Reflect.z;
		float SpecularFactor = CookTorrance(normal, lightVec, EyePos, sr, n, k);

		specular = SpecularFactor*M.Specular*gSpotLight[spotLightNum].Specular;
		diffuse = DiffuseFactor*M.Diffuse*gSpotLight[spotLightNum].Diffuse;
	}

	float Spot = pow(max(dot(-lightVec, gSpotLight[spotLightNum].Direction), 0.0f), gSpotLight[spotLightNum].Spot);

	if (gSpotLight[spotLightNum].Attenuation.x == 0 && gSpotLight[spotLightNum].Attenuation.y == 0 && gSpotLight[spotLightNum].Attenuation.z == 0)
		;
	else
	{
		float Attenuation = Spot / length(gSpotLight[spotLightNum].Attenuation*float3(1.0f, d, d*d));

		specular *= Attenuation;
		diffuse *= Attenuation;
	}
}

float CalcDirLightShadowFactor(
	SamplerState samShadow,
	Texture2DArray shadowMap,
	int ArrayIndex,
	float4 shadowPosH,
	float3 LightDirW,
	float3 PixelPosW,
	float3 PixelNormalW)
{
	shadowPosH.xyz /= shadowPosH.w;

	float DistToReceiver = dot(LightDirW, PixelPosW + normalize(PixelNormalW) * NORMAL_OFFSET) / length(LightDirW);
	
	float avgDistToBlocker = 0.0f;
	float numBlockers = 0.0f;

	float blockerSum = 0.0f;

	[unroll(BLOCKER_FINDER_UNROLL_VALUE)]
	for (int i = BLOCKER_FINDER_KERNEL_SZ; i <= -BLOCKER_FINDER_KERNEL_SZ; ++i)
	{
		[unroll(BLOCKER_FINDER_UNROLL_VALUE)]
		for (int j = BLOCKER_FINDER_KERNEL_SZ; j <= -BLOCKER_FINDER_KERNEL_SZ; ++j)
		{
			float d = shadowMap.SampleLevel(samShadow, float3(shadowPosH + float2(i * offset_X, j * offset_Y),
				ArrayIndex), 0).x;
			
			if (d < DistToReceiver)
			{
				blockerSum += d;
				numBlockers++;
			}
		}
	}
	if (numBlockers < 1.0f)
		return 1.0f;
	avgDistToBlocker = blockerSum / numBlockers;

	float PenumbraSize = clamp(DIR_LIGHT_PENUMBRA_SCALE * (DistToReceiver - avgDistToBlocker) * DIR_LIGHT_SIZE / (DIR_LIGHT_HEIGHT + avgDistToBlocker),
		DIR_LIGHT_MIN_PENUMBRA_SZ, DIR_LIGHT_MAX_PENUMBRA_SZ);

	float percentLit = 0.0f;

	[unroll(PCF_UNROLL_VALUE)]
	for (int i = PCF_KERNEL_SZ; i <= -PCF_KERNEL_SZ; ++i)
	{
		[unroll(PCF_UNROLL_VALUE)]
		for (int j = PCF_KERNEL_SZ; j <= -PCF_KERNEL_SZ; ++j)
		{
			float ShadowMapDistance = shadowMap.SampleLevel(samShadow, float3(shadowPosH + float2(i * offset_X, j * offset_Y) * PenumbraSize,
				ArrayIndex), 0).x;
			
			if (DistToReceiver > ShadowMapDistance)
				;
			else
				percentLit += 1.0f;
		}
	}
	return percentLit /= pow(abs(PCF_KERNEL_SZ) * 2.0f + 1.0f, 2.0f);
}

float CalcSpotLightShadowFactor(
	SamplerState samShadow,
	Texture2DArray shadowMap,
	int ArrayIndex,
	float4 shadowPosH,
	float3 LightPosW,
	float3 PixelPosW,
	float3 PixelNormalW)
{
	shadowPosH.xyz /= shadowPosH.w;

	float DistToReceiver = length(LightPosW - (PixelPosW + normalize(PixelNormalW) * NORMAL_OFFSET));

	float avgDistToBlocker = 0.0f;
	float numBlockers = 0.0f;

	float blockerSum = 0.0f;

	[unroll(BLOCKER_FINDER_UNROLL_VALUE)]
	for (int i = BLOCKER_FINDER_KERNEL_SZ; i <= -BLOCKER_FINDER_KERNEL_SZ; ++i)
	{
		[unroll(BLOCKER_FINDER_UNROLL_VALUE)]
		for (int j = BLOCKER_FINDER_KERNEL_SZ; j <= -BLOCKER_FINDER_KERNEL_SZ; ++j)
		{
			float d = shadowMap.SampleLevel(samShadow, float3(shadowPosH + float2(i * offset_X, j * offset_Y),
				ArrayIndex), 0).x;

			if (d < DistToReceiver)
			{
				blockerSum += d;
				numBlockers++;
			}
		}
	}
	if (numBlockers < 1.0f)
		return 1.0f;
	avgDistToBlocker = blockerSum / numBlockers;

	float PenumbraSize = clamp(SPOT_LIGHT_PENUMBRA_SCALE * (DistToReceiver - avgDistToBlocker) * SPOT_LIGHT_SIZE / (avgDistToBlocker),
		SPOT_LIGHT_MIN_PENUMBRA_SZ, SPOT_LIGHT_MAX_PENUMBRA_SZ);

	float percentLit = 0.0f;

	[unroll(PCF_UNROLL_VALUE)]
	for (int i = PCF_KERNEL_SZ; i <= -PCF_KERNEL_SZ; ++i)
	{
		[unroll(PCF_UNROLL_VALUE)]
		for (int j = PCF_KERNEL_SZ; j <= -PCF_KERNEL_SZ; ++j){
			float ShadowMapDistance = shadowMap.SampleLevel(samShadow, float3(shadowPosH + float2(i * offset_X, j * offset_Y) * PenumbraSize, ArrayIndex), 0).x;
			if (DistToReceiver > ShadowMapDistance)
				;
			else
				percentLit += 1.0f;
		}
	}
	return percentLit /= pow(abs(PCF_KERNEL_SZ) * 2.0f + 1.0f, 2.0f);
}

float CalcPointLightShadowFactor(
	SamplerState samShadow,
	TextureCubeArray shadowMap,
	int ArrayIndex,
	float3 LightPosW,
	float3 PixelPosW,
	float3 PixelNormalW)
{
	float DistToReceiver = length(LightPosW - (PixelPosW + normalize(PixelNormalW) * NORMAL_OFFSET));
	
	float3 LightDir = normalize(LightPosW - PixelPosW);
	
	float avgDistToBlocker = 0.0f;
	float numBlockers = 0.0f;

	float blockerSum = 0.0f;

	[unroll(BLOCKER_FINDER_UNROLL_VALUE)]
	for (int i = BLOCKER_FINDER_KERNEL_SZ; i <= -BLOCKER_FINDER_KERNEL_SZ; ++i)
	{
		[unroll(BLOCKER_FINDER_UNROLL_VALUE)]
		for (int j = BLOCKER_FINDER_KERNEL_SZ; j <= -BLOCKER_FINDER_KERNEL_SZ; ++j)
		{
			[unroll(BLOCKER_FINDER_UNROLL_VALUE)]
			for (int k = BLOCKER_FINDER_KERNEL_SZ; k <= -BLOCKER_FINDER_KERNEL_SZ; ++k)
			{
				float d = shadowMap.SampleLevel(samShadow, float4(-(LightDir.xyz) + float3(i * offset_X, j * offset_Y, k * offset_Z), ArrayIndex), 0).x;

				if (d < DistToReceiver)
				{
					blockerSum += d;
					numBlockers++;
				}
			}
		}
	}
	if (numBlockers < 1.0f)
		return 1.0f;
	avgDistToBlocker = blockerSum / numBlockers;

	float PenumbraSize = clamp(POINT_LIGHT_PENUMBRA_SCALE * (DistToReceiver - avgDistToBlocker) * POINT_LIGHT_SIZE / (avgDistToBlocker),
		POINT_LIGHT_MIN_PENUMBRA_SZ, POINT_LIGHT_MAX_PENUMBRA_SZ);

	float percentLit = 0.0f;

	[unroll(PCF_UNROLL_VALUE)]
	for (int i = PCF_KERNEL_SZ; i <= -PCF_KERNEL_SZ; ++i)
	{
		[unroll(PCF_UNROLL_VALUE)]
		for (int j = PCF_KERNEL_SZ; j <= -PCF_KERNEL_SZ; ++j)
		{
			[unroll(PCF_UNROLL_VALUE)]
			for (int k = PCF_KERNEL_SZ; k <= -PCF_KERNEL_SZ; ++k)
			{
				float ShadowMapDistance = shadowMap.SampleLevel(samShadow, float4(-(LightDir.xyz) + float3(i * offset_X, j * offset_Y, k * offset_Z) * PenumbraSize, ArrayIndex), 0).x;
				if (DistToReceiver > ShadowMapDistance)
					;
				else
					percentLit += 1.0f;
			}
		}
	}
	return percentLit /= pow(abs(PCF_KERNEL_SZ) * 2.0f + 1.0f, 3.0f);
}

float CalcDirLightShadowFactorNoPCSS(
	SamplerState samShadow,
	Texture2DArray shadowMap,
	int ArrayIndex,
	float4 shadowPosH,
	float3 LightDirW,
	float3 PixelPosW,
	float3 PixelNormalW)
{
	shadowPosH.xyz /= shadowPosH.w;

	float DistToReceiver = dot(LightDirW, PixelPosW + normalize(PixelNormalW) * NORMAL_OFFSET) / length(LightDirW);
	float ShadowMapDistance = shadowMap.SampleLevel(samShadow, float3(shadowPosH.xy, ArrayIndex), 0).x;
			
	if (DistToReceiver > ShadowMapDistance)
		return 0.0f;
	else
		return 1.0f;
}

float CalcSpotLightShadowFactorNoPCSS(
	SamplerState samShadow,
	Texture2DArray shadowMap,
	int ArrayIndex,
	float4 shadowPosH,
	float3 LightPosW,
	float3 PixelPosW,
	float3 PixelNormalW)
{
	shadowPosH.xyz /= shadowPosH.w;

	float DistToReceiver = length(LightPosW - (PixelPosW + normalize(PixelNormalW) * NORMAL_OFFSET));
	float ShadowMapDistance = shadowMap.SampleLevel(samShadow, float3(shadowPosH.xy, ArrayIndex), 0).x;
	if (DistToReceiver > ShadowMapDistance)
		return 0.0f;
	else
		return 1.0f;
}

float CalcPointLightShadowFactorNoPCSS(
	SamplerState samShadow,
	TextureCubeArray shadowMap,
	int ArrayIndex,
	float3 LightPosW,
	float3 PixelPosW,
	float3 PixelNormalW)
{
	float3 LightDir = normalize(LightPosW - PixelPosW);

	float DistToReceiver = length(LightPosW - (PixelPosW + normalize(PixelNormalW) * NORMAL_OFFSET));	
	float ShadowMapDistance = shadowMap.SampleLevel(samShadow, float4(-(LightDir.xyz), ArrayIndex), 0).x;
	
	if (DistToReceiver > ShadowMapDistance)
		return 0.0f;
	else
		return 1.0f;
}

float4 ToonShade(int DiscIntCnt, float4 InputColor)
{
	return float4(floor(InputColor.xyz / DiscIntCnt) * DiscIntCnt, InputColor.a);
}

float CalculateViewSpaceDepth(float3 PosW)
{
	return mul(float4(PosW, 1.0f), gView).z;
}

float CalculatePostProjectedDepth(float3 PosW)
{
	return mul(mul(float4(PosW, 1.0f), gView), gProj).z / mul(mul(float4(PosW, 1.0f), gView), gProj).w;
}

float4 CalculateToEyeW(float3 PosW)
{
	float3 toEyeW = gEyePosW - PosW;
	float distToEye = length(toEyeW);
	toEyeW /= distToEye;
	return float4(toEyeW, distToEye);
}

float4x4 CalculateLitColor(Material mat, float3 NormalW, float3 PosW, float3 toEyeW, float PixelDepthV)
{
	float4 dirLightShadowPosH[CASCADE_COUNT * MAX_DIR_LIGHTS_NUM];
	[unroll(MAX_DIR_LIGHTS_NUM)]
	for (int j = 0; j < gDirLightsNum; ++j) {
		[unroll(CASCADE_COUNT)]
		for (int i = 0; i < CASCADE_COUNT; ++i) {
			dirLightShadowPosH[i + j * CASCADE_COUNT] = mul(float4(PosW, 1.0f), gDirLightShadowTransformArray[i + j * CASCADE_COUNT]);
		}
	}

	float4 spotLightShadowPosH[MAX_SPOT_LIGHTS_NUM];
	[unroll(MAX_SPOT_LIGHTS_NUM)]
	for (int i = 0; i < gSpotLightsNum; ++i)
		spotLightShadowPosH[i] = mul(float4(PosW, 1.0f), gSpotLightShadowTransformArray[i]);

	float4x4 ADS = 0.0f;
	float4 A, D, S;

	[unroll(MAX_DIR_LIGHTS_NUM)]
	for (int i = 0; i < gDirLightsNum; ++i) {
		ComputeDirectionalLight(mat, NormalW,
			toEyeW, i, A, D, S);

		int curCascade = 0;
		[unroll(CASCADE_COUNT)]
		for (int cIndex = 0; cIndex < CASCADE_COUNT; cIndex++)
		{
			if (PixelDepthV < gSplitDistances[cIndex].x)
			{
				curCascade = cIndex;
				break;
			}
		}

		float shadow = CalcDirLightShadowFactor(samShadow, gDirLightShadowMapArray, curCascade + i * CASCADE_COUNT, dirLightShadowPosH[curCascade + i * CASCADE_COUNT], gDirLight[i].Direction, PosW, NormalW).x;

		ADS[0] += A;
		ADS[1] += D*shadow;
		ADS[2] += S*shadow;
	}

	[unroll(MAX_POINT_LIGHTS_NUM)]
	for (int g = 0; g < gPointLightsNum; ++g) {
		ComputePointLight(mat, PosW, NormalW,
			toEyeW, g, A, D, S);

		float shadow = CalcPointLightShadowFactor(samLinear, gPointLightCubicShadowMapArray, g, gPointLight[g].Position, PosW, NormalW).x;

		ADS[0] += A;
		ADS[1] += D*shadow;
		ADS[2] += S*shadow;
	}

	[unroll(MAX_SPOT_LIGHTS_NUM)]
	for (int m = 0; m < gSpotLightsNum; ++m) {
		ComputeSpotLight(mat, PosW, NormalW,
			toEyeW, m, A, D, S);

		float shadow = CalcSpotLightShadowFactor(samLinear, gSpotLightShadowMapArray, m, spotLightShadowPosH[m], gSpotLight[m].Position, PosW, NormalW).x;

		ADS[0] += A;
		ADS[1] += D*shadow;
		ADS[2] += S*shadow;
	}
	return ADS;
}

float4 CalculateCustomMeshFbxTexColor(float2 TexC)
{
	float4 texColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

	[unroll(MAX_TEXTURE_COUNT)]
	for (int i = 0; i < gTexCount; ++i)
	{
		float4 color = gDiffuseMapArray.Sample(samAnisotropic, float3(TexC, i));
		texColor *= color*color.a;
	}

	clip(texColor.a - 0.01f);
	
	return texColor;
}

float4 CalculateFbxTexColor(float2 TexC)
{
	float4 texColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

	[unroll(MAX_TEXTURE_COUNT)]
	for (int i = 0; i < gTexCount; ++i)
	{
		if (abs(gTexType[i].x - TEX_TYPE_DIFFUSE_MAP) < 0.1f)
		{
			float4 color = gExtDiffuseMapArray[i].Sample(samAnisotropic, TexC);
			texColor *= color*color.a;
		}
		else if (abs(gTexType[i].x - TEX_TYPE_TRANSPARENT_MAP) < 0.1f)
		{
			float4 color = gExtDiffuseMapArray[i].Sample(samAnisotropic, TexC);
			texColor *= color.r;
		}
	}

	clip(texColor.a - 0.01f);

	return texColor;
}

float4 CalculateTexColor(float2 TexC)
{
	if (gUseTexture)
	{
		if (gMeshType == PROCEDURAL_MESH || gMeshType == CUSTOM_FORMAT_MESH)
		{
			return CalculateCustomMeshFbxTexColor(TexC);
		}
		else if (gMeshType == FBX_MESH)
		{
			return CalculateFbxTexColor(TexC);
		}
	}
	return 1;
}

float PulseFunction(float x)
{
	float treshold = 0.45f;
	float val = abs(x - 0.5f);
	if (val >= treshold)
		return pow(10 * (0.5 - val), 1);
	else
		return 1.0f;
}

float2 PulseFunction2D(float2 x)
{
	return float2(PulseFunction(x.x), PulseFunction(x.y));
}

float4 CalculateReflectionsAndRefractions(float4 litColor, Material mMaterial, float3 toEyeW, float3 NormalW, float3 PosW, float4 PosSS, bool IsFrontFace, uint InstanceId)
{
	float4 reflectionColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
	float4 refractionColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
	float3 reflectionVector = float3(0.0f, 0.0f, 0.0f);
	float3 refractionVector = float3(0.0f, 0.0f, 0.0f);

	float n = mMaterial.Reflect.x;
	float sr = mMaterial.Reflect.y;
	float k = mMaterial.Reflect.z;
	float3 incident = -toEyeW;
	float FresnelMultiplier = FresnelSchlickApprox(FresnelCoeff(n, k), -incident, NormalW);
	float3 PosL = mul(PosW, gInvWorld[InstanceId]);
	float3 PosVS = mul(float4(PosW, 1.0f), gView).xyz;
	float3 NormalVS = mul(NormalW, (float3x3)gView).xyz;
	float3 ReflectionVectorVS = reflect(PosVS, NormalVS);
	float fade = dot(normalize(ReflectionVectorVS), normalize(PosVS));

	switch (gReflectionType)
	{
	case REFLECTION_TYPE_STATIC_CUBE_MAP:
		reflectionVector = reflect(incident, NormalW);
		reflectionColor = gCubeMap.Sample(samAnisotropic, reflectionVector);
		litColor = lerp(litColor, reflectionColor, FresnelMultiplier);
		break;
	case REFLECTION_TYPE_DYNAMIC_CUBE_MAP:
		reflectionVector = normalize(reflect(incident, NormalW)) + (PosW - gObjectDynamicCubeMapWorld[InstanceId]) / (gApproxSceneSz);
		reflectionColor = gCubeMap.Sample(samAnisotropic, reflectionVector);
		litColor = lerp(litColor, reflectionColor, FresnelMultiplier);
		break;
	case REFLECTION_TYPE_STATIC_CUBE_MAP_AND_SSLR:
		reflectionVector = reflect(incident, NormalW);
		if (gEnableSSLR && IsFrontFace)
			reflectionColor = gSSLRMap.SampleLevel(samPointClamp, PosSS.xy, 0.0f) * fade;
		if (reflectionColor.x == 0.0f && reflectionColor.y == 0.0f && reflectionColor.z == 0.0f)
			reflectionColor = gCubeMap.Sample(samAnisotropic, reflectionVector);
		litColor = lerp(litColor, reflectionColor, FresnelMultiplier);
		break;
	case REFLECTION_TYPE_DYNAMIC_CUBE_MAP_AND_SSLR:
		reflectionVector = normalize(reflect(incident, NormalW)) + (PosW - gObjectDynamicCubeMapWorld[InstanceId]) / (gApproxSceneSz);
		if (gEnableSSLR && IsFrontFace)
			reflectionColor = gSSLRMap.SampleLevel(samPointClamp, PosSS.xy, 0.0f) * fade;
		if (reflectionColor.x == 0.0f && reflectionColor.y == 0.0f && reflectionColor.z == 0.0f)
			reflectionColor = gCubeMap.Sample(samAnisotropic, reflectionVector);
		litColor = lerp(litColor, reflectionColor, FresnelMultiplier);
		break;
	case REFLECTION_TYPE_SSLR:
		if (gEnableSSLR && IsFrontFace)
			reflectionColor = gSSLRMap.SampleLevel(samPointClamp, PosSS.xy, 0.0f) * fade;
		litColor = lerp(litColor, reflectionColor, FresnelMultiplier);
		break;
	case REFLECTION_TYPE_SVO_CONE_TRACING:
		if (gEnableRT && IsFrontFace)
			reflectionColor = gSVORTMap.SampleLevel(samPointClamp, PosSS.xy, 0.0f);
		litColor = lerp(litColor, reflectionColor, FresnelMultiplier);
		break;
	default:
		break;
	};

	switch (gRefractionType)
	{
	case REFRACTION_TYPE_STATIC_CUBE_MAP:
		refractionVector = refract(incident, NormalW, n);
		refractionColor = gCubeMap.Sample(samAnisotropic, refractionVector);
		litColor = lerp(refractionColor, litColor, FresnelMultiplier);
		break;
	case REFRACTION_TYPE_DYNAMIC_CUBE_MAP:
		refractionVector = normalize(refract(incident, NormalW, n)) + (PosW - gObjectDynamicCubeMapWorld[InstanceId]) / (gApproxSceneSz);
		refractionColor = gCubeMap.Sample(samAnisotropic, refractionVector);
		litColor = lerp(refractionColor, litColor, FresnelMultiplier);
		break;
	case REFRACTION_TYPE_PLANAR:
		refractionColor = gPlanarRefractionMap.Sample(samAnisotropic, PosSS.xy + MESH_PLANAR_REFRACTION_DISTORTION * NormalVS.xy * PulseFunction2D(PosSS));
		litColor = lerp(refractionColor, litColor, FresnelMultiplier);
		break;
	default:
		break;
	};

	return litColor;
}

float4 CalculateTerrainNormalW(float2 Tex, float2 TiledTex)
{
	float2 leftTex = Tex + float2(-gTexelCellSpaceU, 0.0f);
	float2 rightTex = Tex + float2(gTexelCellSpaceU, 0.0f);
	float2 bottomTex = Tex + float2(0.0f, gTexelCellSpaceV);
	float2 topTex = Tex + float2(0.0f, -gTexelCellSpaceV);

	float leftY = gHeightMap.SampleLevel(samHeightmap, leftTex, 0).r;
	float rightY = gHeightMap.SampleLevel(samHeightmap, rightTex, 0).r;
	float bottomY = gHeightMap.SampleLevel(samHeightmap, bottomTex, 0).r;
	float topY = gHeightMap.SampleLevel(samHeightmap, topTex, 0).r;

	float3 tangent = normalize(float3(2.0f*gWorldCellSpace, rightY - leftY, 0.0f));
	float3 bitan = normalize(float3(0.0f, bottomY - topY, -2.0f*gWorldCellSpace));

	float4 normalW = float4(cross(tangent, bitan), 1.0f);
	float4 normalMapSample = float4(1.0f, 1.0f, 1.0f, 1.0f);

	float4 n[5];
	[unroll(5)]
	for (int i = 0; i < 5; ++i)
		n[i] = gLayerNormalMapArray.SampleLevel(samLinear, float3(TiledTex, i), 0.0f);

	float4 t = gBlendMap.SampleLevel(samLinear, Tex, 0.0f);

	normalMapSample = n[0];
	[unroll(4)]
	for (int i = 0; i < 4; ++i)
		normalMapSample = lerp(normalMapSample, n[i + 1], t[i]);

	return float4(NormalSampleToWorldSpace(normalMapSample, normalW, tangent), normalMapSample.w);
}

float4 CalculateTerrainTexColor(float2 Tex, float2 TiledTex)
{
	float4 texColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

	float4 c[5];
	[unroll(5)]
	for (int i = 0; i < 5; ++i)
		c[i] = gLayerMapArray.Sample(samLinear, float3(TiledTex, i));

	float4 t = gBlendMap.Sample(samLinear, Tex);

	texColor = c[0];
	[unroll(4)]
	for (int i = 0; i < 4; ++i)
		texColor = lerp(texColor, c[i + 1], t[i]);

	clip(texColor.a - 0.01f);

	return texColor;
}