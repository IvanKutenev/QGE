#define PT_EMITTER 0
#define PT_FLARE 1

Texture2DArray gTexArray;
Texture2DArray gNormalArray;

TextureCube gParticleCubeMap;

Texture1D gRandomTex;

Texture2D gParticleSystemRefractionTex;

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

struct Particle
{
	float3 InitialPosW : POSITION;
	float3 InitialVelW : VELOCITY;
	float2 SizeW       : SIZE;
	float Age : AGE;
	uint Type          : TYPE;
};

Material gLiquidWaterMaterial = { float4(0, 0, 0, 1), float4(1, 1, 1, 0.7), float4(1, 1, 1, 1), float3(1.33, 0.020320, 7.2792e-9) };

SamplerState samLinear
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = WRAP;
	AddressV = WRAP;
};

SamplerState samAnisotropic
{
	Filter = ANISOTROPIC;
	MaxAnisotropy = 16;

	AddressU = WRAP;
	AddressV = WRAP;
};

DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
};

DepthStencilState NoDepthWrites
{
	DepthEnable = TRUE;
	DepthWriteMask = ZERO;
};

DepthStencilState DepthWrites
{
	DepthEnable = TRUE;
	DepthWriteMask = ALL;
};


BlendState AdditiveBlending
{
	AlphaToCoverageEnable = FALSE;
	BlendEnable[0] = TRUE;
	SrcBlend = SRC_ALPHA;
	DestBlend = ONE;
	BlendOp = ADD;
	SrcBlendAlpha = ZERO;
	DestBlendAlpha = ZERO;
	BlendOpAlpha = ADD;
	RenderTargetWriteMask[0] = 0x0F;
};

cbuffer cbPerFrame
{
	float3 gEyePosW;

	float3 gEmitPosW;
	float3 gEmitDirW;

	float gGameTime;
	float gTimeStep;
	float4x4 gViewProj;
	//
	DirectionalLight gDirLight[16];
	PointLight gPointLight[16];
	SpotLight gSpotLight[16];

	int gDirLightsNum;
	int gPointLightsNum;
	int gSpotLightsNum;

	float gClientHeight;
	float gClientWidth;
	//
};

cbuffer cbFixed
{
	//Fire

	float3 gFireParticleAccW = { 0.0f, 7.8f, 0.0f };
	float2 gFireQuadTex[4] =
	{
		float2(0.0f, 1.0f),
		float2(1.0f, 1.0f),
		float2(0.0f, 0.0f),
		float2(1.0f, 0.0f)
	};

	//Rain

	float3 gRainParticleAccW = { -1.0f, -9.8f, 0.0f };
	float3 g = { 0.0f, -9.8f, 0.0f };
	float3 windAcc = { -1.0f, 0.0f, 0.0f };
	float gRainDropWidth = 0.03;
	float gRainDropProportionCoeff = 0.07f;
	float gRainCloudsHeight = 500;
	float FrictionCoeff = 0.0;
	float VelocityPower = 2;
	float WaterDensity = 1000;
};


float3 RandUnitVec3(float offset)
{
	float u = (gGameTime + offset);
	float3 v = gRandomTex.SampleLevel(samLinear, u, 0).xyz;
	return normalize(v);
}

float3 RandVec3(float offset)
{
	float u = (gGameTime + offset);
	float3 v = gRandomTex.SampleLevel(samLinear, u, 0).xyz;
	return v;
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

float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
	float3 normalT = 2.0f*normalMapSample - 1.0f;

	float3 N = unitNormalW;
	float3 T = normalize(tangentW - dot(tangentW, N)*N);
	float3 B = cross(N, T);

	float3x3 TBN = float3x3(T, B, N);

	float3 bumpedNormalW = mul(normalT, TBN);

	return bumpedNormalW;
}

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
		float n = M.Reflect.x;
		float sr = M.Reflect.y;
		float k = M.Reflect.z;
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

		float n = M.Reflect.x;
		float sr = M.Reflect.y;
		float k = M.Reflect.z;
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

		float n = M.Reflect.x;
		float sr = M.Reflect.y;
		float k = M.Reflect.z;
		float SpecularFactor = CookTorrance(normal, lightVec, EyePos, sr, n, k);

		specular = SpecularFactor*M.Specular*gSpotLight[spotLightNum].Specular;
		diffuse = DiffuseFactor*M.Diffuse*gSpotLight[spotLightNum].Diffuse;
	}

	float Spot = pow(max(dot(-lightVec, gSpotLight[spotLightNum].Direction), 0.0f), gSpotLight[spotLightNum].Spot);

	ambient *= Spot;

	if (gSpotLight[spotLightNum].Attenuation.x == 0 && gSpotLight[spotLightNum].Attenuation.y == 0 && gSpotLight[spotLightNum].Attenuation.z == 0)
		;
	else
	{
		float Attenuation = Spot / length(gSpotLight[spotLightNum].Attenuation*float3(1.0f, d, d*d));

		specular *= Attenuation;
		diffuse *= Attenuation;
	}
}

float3 GravityForce(float mass)
{
	return mass*g;
}

float3 WindAcc(void)
{
	return windAcc;
}

float3 FrictionForce(float3 velocity)
{
	float v = length(velocity);
	velocity = normalize(velocity);

	return -velocity*FrictionCoeff*pow(v, VelocityPower);
}

float RainDropMassCounter(float H, float W)
{
	return 1 / 2 * WaterDensity*W*W*H;
}

float3 RainDropAccelerationCounter(float RainDropMass, float3 velocity)
{
	return g/* + (FrictionForce(velocity)) / RainDropMass */+ WindAcc();
}