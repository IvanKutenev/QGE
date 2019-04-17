#define GI_VOXEL_GRID_RESOLUTION 20//40
#define GI_VOXELS_COUNT GI_VOXEL_GRID_RESOLUTION * GI_VOXEL_GRID_RESOLUTION * GI_VOXEL_GRID_RESOLUTION
#define GI_CS_THREAD_GROUPS_NUMBER 4
#define GI_CASCADES_COUNT 4//8

#define SOLID_ANGLE_A 0.0318842778f // (22.95668f/(4*180.0f)) 
#define SOLID_ANGLE_B 0.0336955972f // (24.26083f/(4*180.0f))
#define PI 3.14159265f

cbuffer cbGenGrid
{
	float4x4 gGridViewProj[6 * GI_CASCADES_COUNT];
	float4 gGridCenterPosW[GI_CASCADES_COUNT];
	float4 gInvertedGridCellSizes[GI_CASCADES_COUNT];
	float4 gGridCellSizes[GI_CASCADES_COUNT];
	float gGlobalIllumAttFactor;
	float gMeshCullingFactor;
	int gProcessingCascadeIndex;
};

struct Voxel
{
	uint colorMask;
	uint4 normalMasks;
};

struct VPL
{
	float4 redSH;
	float4 greenSH;
	float4 blueSH;
};

struct GenGridGSOut
{
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
	float2 Tex : TEXCOORD0;
	float4 PosSS: TEXCOORD1;
	float3 TangentW : TANGENT;
	int CascadeIndex : CASCADE_INDEX;
};

struct GenTerrainGridGSOut{
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float2 Tex : TEXCOORD0;
	float2 TiledTex : TEXCOORD1;
	float4 PosSS : TEXCOORD2;
	int CascadeIndex : CASCADE_INDEX;
};

static const float3 viewDirections[6] = 
{
	float3(0.0f, 0.0f, -1.0f),
	float3(-1.0f, 0.0f, 0.0f),
	float3(0.0f, -1.0f, 0.0f), 
	float3(0.0f, 0.0f, 1.0f),
	float3(1.0f, 0.0f, 0.0f),
	float3(0.0f, 1.0f, 0.0f)
};

static const float3 faceVectors[4] =
{   
	float3(0.0f, -0.57735026f, 0.81649661f),
	float3(0.0f, -0.57735026f, -0.81649661f),
	float3(-0.81649661f, 0.57735026f, 0.0f),
	float3(0.81649661f, 0.57735026f, 0.0f) 
};

static float3 neighborCellsDirections[6] = 
{ 
	float3(0.0f, 0.0f, 1.0f),
	float3(1.0f, 0.0f, 0.0f),
	float3(0.0f, 0.0f, -1.0f),
	float3(-1.0f, 0.0f, 0.0f),
	float3(0.0f, 1.0f, 0.0f),
	float3(0.0f, -1.0f, 0.0f) 
};

static float4 faceSHCoeffs[6] =
{ 
  float4(0.8862269521f, 0.0f, 1.0233267546f, 0.0f),  // ClampedCosineCoeffs(directions[0])
  float4(0.8862269521f, 0.0f, 0.0f, -1.0233267546f), // ClampedCosineCoeffs(directions[1])
  float4(0.8862269521f, 0.0f, -1.0233267546f, 0.0f), // ClampedCosineCoeffs(directions[2])
  float4(0.8862269521f, 0.0f, 0.0f, 1.0233267546f),  // ClampedCosineCoeffs(directions[3])
  float4(0.8862269521f, -1.0233267546f, 0.0f, 0.0f), // ClampedCosineCoeffs(directions[4])
  float4(0.8862269521f, 1.0233267546, 0.0f, 0.0f)    // ClampedCosineCoeffs(directions[5])
};

static int3 neighborCellsOffsets[6] = 
{
	int3(0, 0, 1),
	int3(1, 0, 0),
	int3(0, 0, -1),
	int3(-1, 0, 0),
	int3(0, 1, 0),
	int3(0, -1, 0) 
};

static const float3 cornersOffsets[8] =
{
	float3(-0.5f, -0.5f, -0.5f),
	float3(0.5f, -0.5f, -0.5f),
	float3(0.5f, -0.5f, 0.5f),
	float3(-0.5f, -0.5f, 0.5f),
	float3(-0.5f, 0.5f, -0.5f),
	float3(0.5f, 0.5f, -0.5f),
	float3(0.5f, 0.5f, 0.5f),
	float3(-0.5f, 0.5f, 0.5f)
};

static const float texelSize = 1.0f / 64.0f;

int GetViewIndex(float3 normal, int cascadeIndex)
{
	float dotProducts[6];
	[unroll(6)]
	for(int i = 0; i < 6; ++i)
	{
		dotProducts[i] = dot(-viewDirections[i], normal);
	}
	float maximum = -1.0f;
	int maxidx = 0;
	[unroll(6)]
	for(int i = 0; i < 6; ++i)
	{
		if(maximum < dotProducts[i])
		{
			maximum = dotProducts[i];
			maxidx = i;
		}
	}
	return maxidx + 6 * cascadeIndex;
}

int GetNormalIndex(float3 normal)
{
	float dotProducts[4];
	[unroll(4)]
	for(int i = 0; i < 4; ++i)
	{
		dotProducts[i] = dot(faceVectors[i], normal);
	}
	float maximum = -1.0f;
	int maxidx = 0;
	[unroll(4)]
	for(int i = 0; i < 4; ++i)
	{
		if(maximum < dotProducts[i])
		{
			maximum = dotProducts[i];
			maxidx = i;
		}
	}
	return maxidx;
}

int GetGridIndex(int3 position, int cascadeIndex)
{
	return (cascadeIndex * GI_VOXELS_COUNT + position.x * GI_VOXEL_GRID_RESOLUTION * GI_VOXEL_GRID_RESOLUTION + position.y * GI_VOXEL_GRID_RESOLUTION + position.z);
}

uint EncodeColor(float3 color)
{	
	//color
	uint3 iColor = uint3(color*255.0f);
	uint colorMask = (iColor.r<<16u) | (iColor.g<<8u) | iColor.b;
	
	//contrast
	float contrast = length(color.rrg-color.gbb)/(sqrt(2.0f)+color.r+color.g+color.b);
	uint iContrast = uint(contrast*127.0);
	colorMask |= iContrast<<24u;
	
	//occlusion
	colorMask |= 1<<31u;

	return colorMask;
}

// Decode specified mask into a float3 color (range 0.0f-1.0f).
float3 DecodeColor(uint colorMask)
{
	float3 color;
	color.r = (colorMask>>16u) & 0x000000ff;
	color.g = (colorMask>>8u) & 0x000000ff;
	color.b = colorMask & 0x000000ff;
	color /= 255.0f;
	
	return color;
}

uint EncodeNormal(float3 normal)
{
	int3 iNormal = int3(normal*255.0f); 
	uint3 iNormalSigns;  
	iNormalSigns.x = (iNormal.x>>5) & 0x04000000;
	iNormalSigns.y = (iNormal.y>>14) & 0x00020000; 
	iNormalSigns.z = (iNormal.z>>23) & 0x00000100;
	iNormal = abs(iNormal);
	uint normalMask = iNormalSigns.x | (iNormal.x<<18) | iNormalSigns.y | (iNormal.y<<9) | iNormalSigns.z | iNormal.z;
  
	int normalIndex = GetNormalIndex(normal);
	float dotProduct = dot(faceVectors[normalIndex], normal);
	uint iDotProduct = uint(saturate(dotProduct)*31.0f);
	normalMask |= iDotProduct<<27u;

	return normalMask;
}

float3 DecodeNormal(uint normalMask)
{
	int3 iNormal;
	iNormal.x = (normalMask>>18) & 0x000000ff;   
	iNormal.y = (normalMask>>9) & 0x000000ff;
	iNormal.z = normalMask & 0x000000ff;
	int3 iNormalSigns;
	iNormalSigns.x = (normalMask>>25) & 0x00000002;
	iNormalSigns.y = (normalMask>>16) & 0x00000002;
	iNormalSigns.z = (normalMask>>7) & 0x00000002;
	iNormalSigns = 1-iNormalSigns;
	float3 normal = float3(iNormal)/255.0f;
	normal *= iNormalSigns; 
	return normal;
}

float4 ClampedCosineCoeffs(float3 dir)
{
	float4 coeffs;  
	coeffs.x = 0.8862269262f;         // PI/(2*sqrt(PI))  
	coeffs.y = -1.0233267079f;        // -((2.0f*PI)/3.0f)*sqrt(3/(4*PI))
	coeffs.z = 1.0233267079f;         // ((2.0f*PI)/3.0f)*sqrt(3/(4*PI))
	coeffs.w = -1.0233267079f;        // -((2.0f*PI)/3.0f)*sqrt(3/(4*PI))
	coeffs.wyz *= dir;
	return coeffs;
}

float4 SH(float3 dir)
{
  float4 result;
  result.x = 0.2820947918f;         // 1/(2*sqrt(PI))    
  result.y = -0.4886025119f;        // -sqrt(3/(4*PI))*y
  result.z = 0.4886025119f;         // sqrt(3/(4*PI))*z
  result.w = -0.4886025119f;        // -sqrt(3/(4*PI))*x
  result.wyz *= dir;
  return result;
}

float3 GetClosestNormal(uint4 normalMasks, float3 direction)
{  
	float4x3 normalMatrix;
	normalMatrix[0] = DecodeNormal(normalMasks.x);
	normalMatrix[1] = DecodeNormal(normalMasks.y);
	normalMatrix[2] = DecodeNormal(normalMasks.z);
	normalMatrix[3] = DecodeNormal(normalMasks.w);
	float4 dotProducts = mul(normalMatrix, direction);
  
	float maximum = max(max(dotProducts.x, dotProducts.y), max(dotProducts.z, dotProducts.w));
	uint index;
	if(maximum == dotProducts.x)
		index = 0;
	else if(maximum == dotProducts.y)
		index = 1;
	else if(maximum == dotProducts.z)
		index = 2;
	else 
		index = 3;
	return normalMatrix[index];
}

#define NO_GEO -11.0f

VPL SampleSHBufferCascade(int3 curVoxel, float3 samplePos, StructuredBuffer<VPL>shBuffer, StructuredBuffer<Voxel>gridBuffer, int cascadeIndex)
{
	VPL vpl[7];
	int3 lerpVoxel = sign(samplePos - curVoxel) * int3(1, 1, 1) + curVoxel;
	float3 lerpParam = abs(lerpVoxel - samplePos);

	int lerpFrom[4] = { 
		GetGridIndex(int3(lerpVoxel.x, curVoxel.y, curVoxel.z), cascadeIndex),
		GetGridIndex(int3(lerpVoxel.x, curVoxel.y, lerpVoxel.z), cascadeIndex),
		GetGridIndex(int3(lerpVoxel.x, lerpVoxel.y, curVoxel.z), cascadeIndex),
		GetGridIndex(int3(lerpVoxel.x, lerpVoxel.y, lerpVoxel.z), cascadeIndex)
	};

	int lerpTo[4] = {
		GetGridIndex(int3(curVoxel.x, curVoxel.y, curVoxel.z), cascadeIndex),
		GetGridIndex(int3(curVoxel.x, curVoxel.y, lerpVoxel.z), cascadeIndex),
		GetGridIndex(int3(curVoxel.x, lerpVoxel.y, curVoxel.z), cascadeIndex),
		GetGridIndex(int3(curVoxel.x, lerpVoxel.y, lerpVoxel.z), cascadeIndex)
	};

	for(int i = 0; i < 4; ++i)
	{
		vpl[i].redSH = lerp(shBuffer[lerpFrom[i]].redSH, shBuffer[lerpTo[i]].redSH, lerpParam.x);
		vpl[i].greenSH = lerp(shBuffer[lerpFrom[i]].greenSH, shBuffer[lerpTo[i]].greenSH, lerpParam.x);
		vpl[i].blueSH = lerp(shBuffer[lerpFrom[i]].blueSH, shBuffer[lerpTo[i]].blueSH, lerpParam.x);	
	}

	for(int i = 4; i < 6; ++i)
	{
		vpl[i].redSH = lerp(vpl[i * 2 - 7].redSH, vpl[i * 2 - 8].redSH, lerpParam.z);
		vpl[i].greenSH = lerp(vpl[i * 2 - 7].greenSH, vpl[i * 2 - 8].greenSH, lerpParam.z);
		vpl[i].blueSH = lerp(vpl[i * 2 - 7].blueSH, vpl[i * 2 - 8].blueSH, lerpParam.z);	
	}

	vpl[6].redSH = lerp(vpl[5].redSH, vpl[4].redSH, lerpParam.y);
	vpl[6].greenSH = lerp(vpl[5].greenSH, vpl[4].greenSH, lerpParam.y);
	vpl[6].blueSH = lerp(vpl[5].blueSH, vpl[4].blueSH, lerpParam.y);
	
	return vpl[6];
}

float3 CalculateOffset(float3 PosW, int cascadeIndex)
{
	return (PosW - gGridCenterPosW[cascadeIndex]) * gInvertedGridCellSizes[cascadeIndex];
}

void CalculateGridPositions(float3 PosW, int cascadeIndex, out float3 samplePos, out int3 voxelPos)
{
	float3 offset = CalculateOffset(PosW, cascadeIndex);
	samplePos = float3(GI_VOXEL_GRID_RESOLUTION / 2, GI_VOXEL_GRID_RESOLUTION / 2, GI_VOXEL_GRID_RESOLUTION / 2) + offset;
	offset = round(offset);
	voxelPos = int3(GI_VOXEL_GRID_RESOLUTION / 2, GI_VOXEL_GRID_RESOLUTION / 2, GI_VOXEL_GRID_RESOLUTION / 2) + int3(offset);
}

VPL SampleSHBuffer(float3 PosW, StructuredBuffer<VPL>shBuffer, StructuredBuffer<Voxel>gridBuffer, int cascadeIndex)
{
	int3 curVoxel, nextCascadeVoxel;
	float3 samplePos, nextCascadeSamplePos;
	CalculateGridPositions(PosW, cascadeIndex, samplePos, curVoxel);
	if(cascadeIndex >= GI_CASCADES_COUNT - 1)
		return SampleSHBufferCascade(curVoxel, samplePos, shBuffer, gridBuffer, cascadeIndex);
	CalculateGridPositions(PosW, cascadeIndex + 1, nextCascadeSamplePos, nextCascadeVoxel);

	VPL curCascadeVPL = SampleSHBufferCascade(curVoxel, samplePos, shBuffer, gridBuffer, cascadeIndex);
	VPL nextCascadeVPL = SampleSHBufferCascade(nextCascadeVoxel, nextCascadeSamplePos, shBuffer, gridBuffer, cascadeIndex + 1);

	float3 offset = abs(CalculateOffset(PosW, cascadeIndex));
	offset /= GI_VOXEL_GRID_RESOLUTION / 2;
	offset -= 0.5f;
	offset = 2.0f * saturate(offset);

	VPL retVPL;
	retVPL.redSH = lerp(curCascadeVPL.redSH, nextCascadeVPL.redSH, offset.x);
	retVPL.greenSH = lerp(curCascadeVPL.greenSH, nextCascadeVPL.greenSH, offset.x);
	retVPL.blueSH = lerp(curCascadeVPL.blueSH, nextCascadeVPL.blueSH, offset.x);

	retVPL.redSH = lerp(retVPL.redSH, nextCascadeVPL.redSH, offset.y);
	retVPL.greenSH = lerp(retVPL.greenSH, nextCascadeVPL.greenSH, offset.y);
	retVPL.blueSH = lerp(retVPL.blueSH, nextCascadeVPL.blueSH, offset.y);

	retVPL.redSH = lerp(retVPL.redSH, nextCascadeVPL.redSH, offset.z);
	retVPL.greenSH = lerp(retVPL.greenSH, nextCascadeVPL.greenSH, offset.z);
	retVPL.blueSH = lerp(retVPL.blueSH, nextCascadeVPL.blueSH, offset.z);

	return retVPL;
}