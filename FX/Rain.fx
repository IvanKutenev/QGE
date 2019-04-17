#include "ParticleDefinitions.fx"

Particle StreamOutVS(Particle vin)
{
	return vin;
}

[maxvertexcount(100)]
void StreamOutGS(point Particle gin[1], 
                 inout PointStream<Particle> ptStream)
{	
	gin[0].Age += gTimeStep;

	if( gin[0].Type == PT_EMITTER )
	{	
		// time to emit a new particle?
		if( gin[0].Age > 0.0009f )
		{
			for(int i = 0; i < 50; ++i)
			{
				// Spread rain drops out above the camera.
				float3 vRandom = 10.0f*RandVec3((float)i/95.0f);
				vRandom.y = 10.0f;
			
				float t = sqrt((gRainCloudsHeight - (vRandom.y + gEmitPosW.y)) / abs(g.y));

				Particle p;
				p.InitialPosW = gEmitPosW.xyz + vRandom;
				p.InitialVelW = float3(0.0f, g.y*t, 0.0f);
				p.SizeW       = float2(1.0f, 1.0f);
				p.Age         = 0.0f;
				p.Type        = PT_FLARE;
			
				ptStream.Append(p);
			}
			
			// reset the time to emit
			gin[0].Age = 0.0f;
		}

		// always keep emitters
		ptStream.Append(gin[0]);
	}
	else
	{
		// Specify conditions to keep particle; this may vary from system to system.
		if (gin[0].Age <= 1.0f){
			float3 a = RainDropAccelerationCounter(RainDropMassCounter(gin[0].SizeW.x, gin[0].SizeW.y), gin[0].InitialVelW);
			gin[0].InitialPosW += 0.5*gTimeStep*gTimeStep*a + gTimeStep*gin[0].InitialVelW;
			gin[0].InitialVelW += gTimeStep*a;
			ptStream.Append(gin[0]);
		}
	}		
}

GeometryShader gsStreamOut = ConstructGSWithSO( 
	CompileShader( gs_5_0, StreamOutGS() ), 
	"POSITION.xyz; VELOCITY.xyz; SIZE.xy; AGE.x; TYPE.x" );
	
technique11 StreamOutTech
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_5_0, StreamOutVS() ) );
		SetHullShader(NULL);
		SetDomainShader(NULL);
        SetGeometryShader( gsStreamOut );
        
        // disable pixel shader for stream-out only
        SetPixelShader(NULL);
        
        // we must also disable the depth buffer for stream-out only
        SetDepthStencilState( DisableDepth, 0 );
    }
}

struct VertexOut
{
	float3 PosW  : POSITION;
	float3 VelocityW : VELOCITY;
	float3 InitVelocityW : INIT_VELOCITY;
	uint   Type  : TYPE;
};

VertexOut DrawVS(Particle vin)
{
	VertexOut vout;
	
	float t = vin.Age;
	
	vout.PosW = vin.InitialPosW;
	vout.VelocityW = vin.InitialVelW;
	vout.InitVelocityW = vin.InitialVelW;
	vout.Type  = vin.Type;
	
	return vout;
}

struct GeoOut
{
	float4 PosH  : SV_Position;
	float2 Tex   : TEXCOORD;
	float3 NormalW : NORMAL;
	float3 PosW : POSITION;
};

[maxvertexcount(6)]
void DrawGS(point VertexOut gin[1], 
            inout TriangleStream<GeoOut> triStream)
{	
	if( gin[0].Type != PT_EMITTER )
	{
		float3 p0 = gin[0].PosW;
		float3 p1 = gin[0].PosW + gRainDropProportionCoeff*gRainParticleAccW;

		float3 Look = 0.5*(p0 + p1) - gEyePosW;
		float3 P1P0 = p1 - p0;

		float3 LookxP1P0 = normalize(cross(Look, P1P0));

		float d = distance(p0, p1);

		GeoOut v0;
		v0.PosH = mul(float4(p0 - LookxP1P0 * d * gRainDropWidth, 1.0f), gViewProj);
		v0.Tex = float2(0, 0);
		v0.NormalW = normalize(cross(LookxP1P0, P1P0));
		v0.PosW = p0 - LookxP1P0 * d * gRainDropWidth;
		GeoOut v1;
		v1.PosH = mul(float4(p0 + LookxP1P0 * d * gRainDropWidth, 1.0f), gViewProj);
		v1.Tex = float2(1, 0);
		v1.NormalW = normalize(cross(LookxP1P0, P1P0));
		v1.PosW = p0 + LookxP1P0 * d * gRainDropWidth;
		GeoOut v2;
		v2.PosH = mul(float4(p1 - LookxP1P0 * d * gRainDropWidth, 1.0f), gViewProj);
		v2.Tex = float2(0, 1);
		v2.NormalW = normalize(cross(LookxP1P0, P1P0));
		v2.PosW = p1 - LookxP1P0 * d * gRainDropWidth;
		GeoOut v3;
		v3.PosH = mul(float4(p1 + LookxP1P0 * d * gRainDropWidth, 1.0f), gViewProj);
		v3.Tex = float2(1, 1);
		v3.NormalW = normalize(cross(LookxP1P0, P1P0));
		v3.PosW = p1 + LookxP1P0 * d * gRainDropWidth;

		triStream.Append(v0);
		triStream.Append(v1);
		triStream.Append(v2);

		triStream.Append(v1);
		triStream.Append(v3);
		triStream.Append(v2);

		
	}
}

float4 DrawPS(GeoOut pin) : SV_TARGET
{
	float3 normal = gNormalArray.Sample(samLinear, float3(pin.Tex, 0)).xyz;
	if (gNormalArray.Sample(samLinear, float3(pin.Tex, 0)).w == 0)
		clip(-1);
	float3 tangent = ComputeTangent(pin.NormalW);
	pin.NormalW = NormalSampleToWorldSpace(normal, pin.NormalW, tangent);

	float n = gLiquidWaterMaterial.Reflect.x;
	float sr = gLiquidWaterMaterial.Reflect.y;
	float k = gLiquidWaterMaterial.Reflect.z;

	float3 toEyeW = gEyePosW - pin.PosW;

	float distToEye = length(toEyeW);

	toEyeW /= distToEye;

	float3 incident = -toEyeW;

	float FresnelMultiplier = FresnelSchlickApprox(FresnelCoeff(n, k), -incident, pin.NormalW);

	float3 reflectionVector = reflect(incident, pin.NormalW);

	float4 EnvReflColor = gParticleCubeMap.Sample(samAnisotropic, reflectionVector);

	float2 refractTexCoord;
	refractTexCoord.x = pin.PosH.x / gClientWidth;
	refractTexCoord.y = pin.PosH.y / gClientHeight;
	refractTexCoord = refractTexCoord + (normal.xy * 0.06);
	float4 refractionColor = gParticleSystemRefractionTex.Sample(samAnisotropic, refractTexCoord);

	//
	//Lighting
	//

	float4 ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
		float4 diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
		float4 spec = float4(0.0f, 0.0f, 0.0f, 0.0f);

		float4 A, D, S;

	for (int i = 0; i < gDirLightsNum; ++i){
		ComputeDirectionalLight(gLiquidWaterMaterial, pin.NormalW,
			toEyeW, i, A, D, S);

		ambient += A;
		diffuse += D;
		spec += S;
	}

	for (int g = 0; g < gPointLightsNum; ++g){
		ComputePointLight(gLiquidWaterMaterial, pin.PosW, pin.NormalW,
			toEyeW, g, A, D, S);

		ambient += A;
		diffuse += D;
		spec += S;
	}

	for (int k = 0; k < gSpotLightsNum; ++k){
		ComputeSpotLight(gLiquidWaterMaterial, pin.PosW, pin.NormalW,
			toEyeW, k, A, D, S);

		ambient += A;
		diffuse += D;
		spec += S;
	}

	float4 litColor = ambient + diffuse + spec;

	litColor = lerp(litColor, EnvReflColor, FresnelMultiplier);
	litColor = lerp(refractionColor, litColor, FresnelMultiplier);

	litColor.a = gLiquidWaterMaterial.Diffuse.a;

	return litColor;
}

technique11 DrawTech
{
    pass P0
    {
        SetVertexShader(   CompileShader( vs_5_0, DrawVS() ) );
		SetHullShader(NULL);
		SetDomainShader(NULL);
        SetGeometryShader( CompileShader( gs_5_0, DrawGS() ) );
        SetPixelShader(    CompileShader( ps_5_0, DrawPS() ) );
        
		SetDepthStencilState( DepthWrites, 0 );
    }
}