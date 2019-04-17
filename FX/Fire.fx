#include "ParticleDefinitions.fx"
  
Particle StreamOutVS(Particle vin)
{
	return vin;
}

[maxvertexcount(2)]
void StreamOutGS(point Particle gin[1], 
                 inout PointStream<Particle> ptStream)
{	
	gin[0].Age += gTimeStep/10;
	
	if( gin[0].Type == PT_EMITTER )
	{	
		// time to emit a new particle?
		if( gin[0].Age > 0.001f )
		{
			float3 vRandom = RandUnitVec3(0.0f);
			vRandom.x *= 0.6f;
			vRandom.z *= 0.6f;
			
			Particle p;
			p.InitialPosW = gEmitPosW.xyz;
			p.InitialVelW = 4.0f*vRandom;
			p.SizeW       = float2(3.0f, 3.0f);
			p.Age         = 0.0f;
			p.Type        = PT_FLARE;
			
			ptStream.Append(p);
			
			// reset the time to emit
			gin[0].Age = 0.0f;
		}
		
		// always keep emitters
		ptStream.Append(gin[0]);
	}
	else
	{
		// Specify conditions to keep particle; this may vary from system to system.
		if( gin[0].Age <= 0.6f )
			ptStream.Append(gin[0]);
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
	float2 SizeW : SIZE;
	float4 Color : COLOR;
	uint   Type  : TYPE;
};

VertexOut DrawVS(Particle vin)
{
	VertexOut vout;
	
	float t = vin.Age;
	
	// constant acceleration equation
	vout.PosW = 0.5f*t*t*gFireParticleAccW + t*vin.InitialVelW + vin.InitialPosW;
	
	// fade color with time
	float opacity = 1.0f - smoothstep(0.0f, 1.0f, t/1.0f);
	vout.Color = float4(1.0f, 1.0f, 1.0f, opacity);
	
	vout.SizeW = vin.SizeW;
	vout.Type  = vin.Type;
	
	return vout;
}

struct GeoOut
{
	float4 PosH  : SV_Position;
	float4 Color : COLOR;
	float2 Tex   : TEXCOORD;
};

// The draw GS just expands points into camera facing quads.
[maxvertexcount(4)]
void DrawGS(point VertexOut gin[1], 
            inout TriangleStream<GeoOut> triStream)
{	
	// do not draw emitter particles.
	if( gin[0].Type != PT_EMITTER )
	{
		//
		// Compute world matrix so that billboard faces the camera.
		//
		float3 look  = normalize(gEyePosW.xyz - gin[0].PosW);
		float3 right = normalize(cross(float3(0,1,0), look));
		float3 up    = cross(look, right);
		
		//
		// Compute triangle strip vertices (quad) in world space.
		//
		float halfWidth  = 0.5f*gin[0].SizeW.x;
		float halfHeight = 0.5f*gin[0].SizeW.y;
	
		float4 v[4];
		v[0] = float4(gin[0].PosW + halfWidth*right - halfHeight*up, 1.0f);
		v[1] = float4(gin[0].PosW + halfWidth*right + halfHeight*up, 1.0f);
		v[2] = float4(gin[0].PosW - halfWidth*right - halfHeight*up, 1.0f);
		v[3] = float4(gin[0].PosW - halfWidth*right + halfHeight*up, 1.0f);
		
		//
		// Transform quad vertices to world space and output 
		// them as a triangle strip.
		//
		GeoOut gout;
		[unroll]
		for(int i = 0; i < 4; ++i)
		{
			gout.PosH  = mul(v[i], gViewProj);
			gout.Tex   = gFireQuadTex[i];
			gout.Color = gin[0].Color;
			triStream.Append(gout);
		}	
	}
}

float4 DrawPS(GeoOut pin) : SV_TARGET
{
	return gTexArray.Sample(samLinear, float3(pin.Tex, 0))*pin.Color;
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
        
        SetBlendState(AdditiveBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xffffffff);
        SetDepthStencilState( NoDepthWrites, 0 );
    }
}