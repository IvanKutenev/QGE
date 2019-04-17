#include "Definitions.fx"

////////////////////////////Shaders////////////////////////////////////////////////////////////////

InstancedShaderOut VS(InstancedVertexIn vin)
{
	InstancedShaderOut vout;
	
	vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
	vout.NormalW = mul(float4(vin.NormalL, 1.0f), gWorldInvTranspose).xyz;
	
	vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
	
	vout.Tex = mul(float4(vin.Tex, 0.0f, 1.0f), gTexTransform).xy;
	vout.Tex = mul(float4(vout.Tex, 0.0f, 1.0f), gAddTexTransform).xy;

	vout.TangentW = ComputeTangent(vout.NormalW);
	vout.InstanceId = vin.InstanceId;
	return vout;
}

PatchTess ConstantHS(InputPatch<InstancedShaderOut, 3>patch, uint patchID : SV_PrimitiveID)
{
	PatchTess hsOut = (PatchTess)0;

	float3 triangleVP[3];
	triangleVP[0] = patch[0].PosW;
	triangleVP[1] = patch[1].PosW;
	triangleVP[2] = patch[2].PosW;

	float3 triangleN[3];
	triangleN[0] = patch[0].NormalW;
	triangleN[1] = patch[1].NormalW;
	triangleN[2] = patch[2].NormalW;

	hsOut.controlP = calculateControlPoints(triangleVP, triangleN);

	float tess = 1.0f;

	if (gEnableDynamicLOD == true){
		float d = distance(gEyePosW, hsOut.controlP.B111);

		const float d0 = gMaxLODdist;
		const float d1 = gMinLODdist;
		tess = 64.0f*saturate((d1 - d) / (d1 - d0));

		if (tess < 1.0f)
			tess = 1.0f;

		if (tess > gMaxTessFactor)
			tess = gMaxTessFactor;
	}
	else
		tess = gConstantLODTessFactor;
	hsOut.EdgeTess[0] = tess;
	hsOut.EdgeTess[1] = tess;
	hsOut.EdgeTess[2] = tess;

	hsOut.InsideTess = tess;

	return hsOut;
}

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
InstancedShaderOut HS(InputPatch<InstancedShaderOut, 3> p,
	uint i : SV_OutputControlPointID,
	uint patchId : SV_PrimitiveID)
{
	InstancedShaderOut hout;

	hout.PosW    = p[i].PosW;
	hout.NormalW = p[i].NormalW;
	hout.PosH    = float4(1.0f, 1.0f, 1.0f, 1.0f);
	hout.Tex     = p[i].Tex;
	hout.TangentW = p[i].TangentW;

	return hout;
}

[domain("tri")]
InstancedShaderOut DS(PatchTess patchTess,
	float3 bary : SV_DomainLocation,
	const OutputPatch<InstancedShaderOut, 3>bezPatch)
{
	InstancedShaderOut dsOut = (InstancedShaderOut)0;

	float4 triangleVP[3];
	triangleVP[0] = float4(bezPatch[0].PosW, 1.0f);
	triangleVP[1] = float4(bezPatch[1].PosW, 1.0f);
	triangleVP[2] = float4(bezPatch[2].PosW, 1.0f);
	float3 triangleN[3];
	triangleN[0] = bezPatch[0].NormalW;
	triangleN[1] = bezPatch[1].NormalW;
	triangleN[2] = bezPatch[2].NormalW;
	float3 position = calculatePositionfromUVW(triangleVP, bary,
		patchTess.controlP);
	
	float3 normal = calculateNormalfromUVW(triangleN, bary,
		patchTess.controlP);
	
	dsOut.Tex = interpolateTexC(bezPatch[0].Tex, bezPatch[1].Tex, bezPatch[2].Tex,
		bary);
	
	dsOut.NormalW = normal;
	
	dsOut.PosW = position;
	
	dsOut.TangentW = ComputeTangent(dsOut.NormalW);

	if (gUseDisplacementMapping)
	{
		const float MipInterval = 20.0f;
		float mipLevel = clamp((distance(dsOut.PosW, gEyePosW) - MipInterval) / MipInterval, 0.0f, 6.0f);

		float h = gNormalMap.SampleLevel(samLinear, dsOut.Tex, 0).a;

		dsOut.NormalW = normalize(dsOut.NormalW);

		dsOut.PosW += (gHeightScale * (h - 1.0))*dsOut.NormalW;
	}

	dsOut.PosH = mul(float4(dsOut.PosW, 1.0f), gViewProj);

	return dsOut;
}

[maxvertexcount(3)]
void ComputeNormalGS(triangle InstancedShaderOut gin[3], inout TriangleStream<InstancedShaderOut>triStream)
{
	float3 n;
	InstancedShaderOut gout[3];

	gout[0] = gin[0];
	gout[1] = gin[1];
	gout[2] = gin[2];

	n = ComputeNormal(gout[0].PosW, gout[1].PosW, gout[2].PosW);
	n = normalize(n);
	
	gout[0].NormalW = n;
	gout[1].NormalW = n;
	gout[2].NormalW = n;

	triStream.Append(gout[0]);
	triStream.Append(gout[1]);
	triStream.Append(gout[2]);
}

float4 PS(InstancedShaderOut pin) : SV_Target
{

	if (pin.PosW.y <= gWaterHeight && gDrawWaterReflection)
		clip(-1);

	if (pin.PosW.y >= gWaterHeight && gDrawWaterRefraction)
		clip(-1);

	float3 toEyeW = gEyePosW - pin.PosW;

	pin.NormalW = normalize(pin.NormalW);
	
	if (gUseNormalMapping||gUseDisplacementMapping)
	{
		float3 normalMapSample = gNormalMap.Sample(samLinear, pin.Tex).rgb;
		pin.NormalW = NormalSampleToWorldSpace(normalMapSample, pin.NormalW, pin.TangentW);

	}

	float distToEye = length(toEyeW);
	
	toEyeW /= distToEye;
	
	float4 texColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

	//
	//Texturing
	//

	if(gUseTexture)
	{
		if(gTexCount == 1)
			texColor = gDiffuseMap1.Sample(samAnisotropic, pin.Tex);
		else if(gTexCount == 2)
		{
			texColor  = gDiffuseMap1.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap2.Sample(samAnisotropic, pin.Tex);
		}
		else if(gTexCount == 3)
		{
			texColor  = gDiffuseMap1.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap2.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap3.Sample(samAnisotropic, pin.Tex);
		}
		else if(gTexCount == 4)
		{
			texColor  = gDiffuseMap1.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap2.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap3.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap4.Sample(samAnisotropic, pin.Tex);
		}
		else if(gTexCount == 5)
		{
			texColor  = gDiffuseMap1.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap2.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap3.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap4.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap5.Sample(samAnisotropic, pin.Tex);
		}
		else if(gTexCount == 6)
		{
			texColor  = gDiffuseMap1.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap2.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap3.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap4.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap5.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap6.Sample(samAnisotropic, pin.Tex);
		}
		else if(gTexCount == 7)
		{
			texColor  = gDiffuseMap1.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap2.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap3.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap4.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap5.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap6.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap7.Sample(samAnisotropic, pin.Tex);
		}
		else if(gTexCount == 8)
		{
			texColor  = gDiffuseMap1.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap2.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap3.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap4.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap5.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap6.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap7.Sample(samAnisotropic, pin.Tex);
			texColor *= gDiffuseMap8.Sample(samAnisotropic, pin.Tex);
		}

		if(gUseBlackClipping)
		{
			if(texColor.r <= 0.1f && texColor.g <= 0.1f && texColor.b <= 0.1f )
				clip(-1);
		}
		if(gUseAlphaClipping)
		{
			clip(texColor.a-0.01f);
		}
	}
	
	//
	//Lighting
	//

	float4 ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float4 spec = float4(0.0f, 0.0f, 0.0f, 0.0f);

	float4 A, D, S;
	
	for(unsigned int i = 0; i < gDirLightsNum; ++i){
		ComputeDirectionalLight(gMaterial, pin.NormalW,
			toEyeW, i,  A, D, S);

		ambient += A;
		diffuse += D;
		spec += S;
	}
	
	for(unsigned int g = 0; g < gPointLightsNum; ++g){
		ComputePointLight(gMaterial, pin.PosW, pin.NormalW,
			toEyeW, g, A, D, S);
	
		ambient += A;
		diffuse += D;
		spec += S;
	}

	for(unsigned int k = 0; k < gSpotLightsNum; ++k){
		ComputeSpotLight(gMaterial, pin.PosW, pin.NormalW,
			toEyeW, k, A, D, S);
	
		ambient += A;
		diffuse += D;
		spec += S;
	}
	//

	//
	//Cube mapping
	//

	float4 litColor = texColor*(ambient + diffuse) + spec;

	if (gUseStaticCubeMapping)
	{
		float n = gMaterial.Reflect.x;
		float sr = gMaterial.Reflect.y;
		float k = gMaterial.Reflect.z;

		float3 incident = -toEyeW;

		float FresnelMultiplier = FresnelSchlickApprox(FresnelCoeff(n, k), -incident, pin.NormalW);

		float3 PosL = mul(pin.PosW, gInvWorld);

		float3 reflectionVector = reflect(incident, pin.NormalW);

		float4 EnvReflColor = (gUseFog?gFogColor:gCubeMap.Sample(samAnisotropic, reflectionVector));

		float3 refractionVector = refract(incident, pin.NormalW, n);
		
		float4 EnvRefrColor = (gUseFog ? gFogColor : gCubeMap.Sample(samAnisotropic, refractionVector));

		if (gUseReflection)
		{
			litColor = lerp(litColor, EnvReflColor, FresnelMultiplier);
		}
		if (gUseRefraction)
		{
			litColor = lerp(EnvRefrColor, litColor, FresnelMultiplier);
		}
	}
	else if (gUseDynamicCubeMapping)
	{
		float n = gMaterial.Reflect.x;
		float sr = gMaterial.Reflect.y;
		float k = gMaterial.Reflect.z;

		float3 incident = -toEyeW;

		float FresnelMultiplier = FresnelSchlickApprox(FresnelCoeff(n, k), -incident, pin.NormalW);

		float3 PosL = mul(pin.PosW, gInvWorld);

		float3 reflectionVector = normalize(reflect(incident, pin.NormalW)) + (pin.PosW - gObjectDynamicCubeMapWorld) / (gApproxSceneSz);
		
		float4 EnvReflColor = gCubeMap.Sample(samAnisotropic, reflectionVector);

		float3 refractionVector = normalize(refract(incident, pin.NormalW, n)) + (pin.PosW - gObjectDynamicCubeMapWorld) / (gApproxSceneSz);
		
		float4 EnvRefrColor = gCubeMap.Sample(samAnisotropic, refractionVector);

		if (gUseReflection)
		{
			litColor = lerp(litColor, EnvReflColor, FresnelMultiplier);
		}
		if (gUseRefraction)
		{
			litColor = lerp(EnvRefrColor, litColor, FresnelMultiplier);
		}
	}

	//
	// Fog
	//

	if(gUseFog)
	{
		float s = saturate((distToEye-gFogStart)/gFogRange);
		litColor = lerp(litColor, gFogColor, s);
	}

	litColor.a = gMaterial.Diffuse.a;

	return litColor;
}

technique11 BasicTech
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_5_0, VS() ) );
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetGeometryShader(NULL);
        SetPixelShader( CompileShader( ps_5_0, PS() ) );
	}
};

technique11 TessTech
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetHullShader(CompileShader(hs_5_0, HS()));
		SetDomainShader(CompileShader(ds_5_0, DS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, PS()));
	}
};