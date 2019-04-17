#include "Terrain.h"

Terrain::Terrain(ID3D11Device* device, ID3D11DeviceContext* dc, const InitInfo& initInfo, ID3DX11Effect* mFX) : 
mTerrainMinLODdist(40.0f), mTerrainMaxLODdist(3000.0f), mTerrainMaxTessFactor(16.0f), mTerrainMinTessFactor(2.0f),
mTerrainHeightScale(0.2f), CellsPerPatch(256)
{
	mTerrainFX = mFX;

	mInfo = initInfo;

	mTerrainMaterial.Ambient = XMFLOAT4(1, 1, 1, 1);
	mTerrainMaterial.Diffuse = XMFLOAT4(1, 1, 1, 1);
	mTerrainMaterial.Specular = XMFLOAT4(1, 1, 1, 1);
	mTerrainMaterial.Reflect = XMFLOAT3(1.0, 0.0, -1);
	mTerrainMaterial = mTerrainMaterial * Color_lightgrey;

	mNumPatchVertRows = ((mInfo.HeightmapHeight - 1) / CellsPerPatch) + 1;
	mNumPatchVertCols = ((mInfo.HeightmapWidth - 1) / CellsPerPatch) + 1;

	mNumPatchVertices = mNumPatchVertRows*mNumPatchVertCols;
	mNumPatchQuadFaces = (mNumPatchVertRows - 1)*(mNumPatchVertCols - 1);

	LoadHeightmap();
	Smooth();
	CalcAllPatchBoundsY();

	BuildVB(device);
	BuildIB(device);
	BuildHeightmapSRV(device);
	BuildFX();
	BuildVertexInputLayout(device);

	std::vector<std::wstring> layerFilenames;
	layerFilenames.push_back(mInfo.LayerMapFilename0);
	layerFilenames.push_back(mInfo.LayerMapFilename1);
	layerFilenames.push_back(mInfo.LayerMapFilename2);
	layerFilenames.push_back(mInfo.LayerMapFilename3);
	layerFilenames.push_back(mInfo.LayerMapFilename4);
	mLayerMapArraySRV = d3dHelper::CreateTexture2DArraySRVW(device, dc, layerFilenames);

	std::vector<std::wstring> layerNormalFilenames;
	layerNormalFilenames.push_back(mInfo.LayerNormalMapFilename0);
	layerNormalFilenames.push_back(mInfo.LayerNormalMapFilename1);
	layerNormalFilenames.push_back(mInfo.LayerNormalMapFilename2);
	layerNormalFilenames.push_back(mInfo.LayerNormalMapFilename3);
	layerNormalFilenames.push_back(mInfo.LayerNormalMapFilename4);
	mLayerNormalMapArraySRV = d3dHelper::CreateTexture2DArraySRVW(device, dc, layerNormalFilenames);

	mBlendMapSRV = d3dHelper::CreateTexture2DSRVW(device, dc, mInfo.BlendMapFilename.c_str());

	XMStoreFloat4x4(&mTerrainWorld, XMMatrixIdentity());
}

float Terrain::GetWidth()const
{
	// Total terrain width.
	return (mInfo.HeightmapWidth - 1)*mInfo.CellSpacing;
}

float Terrain::GetDepth()const
{
	// Total terrain depth.
	return (mInfo.HeightmapHeight - 1)*mInfo.CellSpacing;
}

float Terrain::GetHeight(float x, float z)const
{
	// Transform from terrain local space to "cell" space.
	float c = (x + 0.5f*GetWidth()) / mInfo.CellSpacing;
	float d = (z - 0.5f*GetDepth()) / -mInfo.CellSpacing;

	// Get the row and column we are in.
	int row = (int)floorf(d);
	int col = (int)floorf(c);

	float A = mHeightmap[row*mInfo.HeightmapWidth + col];
	float B = mHeightmap[row*mInfo.HeightmapWidth + col + 1];
	float C = mHeightmap[(row + 1)*mInfo.HeightmapWidth + col];
	float D = mHeightmap[(row + 1)*mInfo.HeightmapWidth + col + 1];

	// Where we are relative to the cell.
	float s = c - (float)col;
	float t = d - (float)row;

	// If upper triangle ABC.
	if (s + t <= 1.0f)
	{
		float uy = B - A;
		float vy = C - A;
		return A + s*uy + t*vy;
	}
	else // lower triangle DCB.
	{
		float uy = C - D;
		float vy = B - D;
		return D + (1.0f - s)*uy + (1.0f - t)*vy;
	}
}

XMMATRIX Terrain::GetWorld()const
{
	return XMLoadFloat4x4(&mTerrainWorld);
}

void Terrain::SetWorld(CXMMATRIX M)
{
	XMStoreFloat4x4(&mTerrainWorld, M);
}

void Terrain::Draw(ID3D11DeviceContext* dc, const Camera& camera,
	ID3D11RenderTargetView** renderTargets, ID3D11DepthStencilView* depthStencilView,
	DrawShadowType shadowType,
	int LightIndex,
	DrawPassType dst)
{
	ID3DX11EffectTechnique* tempTech = mTerrainTech;

	if (shadowType != DrawShadowType::DRAW_SHADOW_TYPE_NO_SHADOW)
		mTerrainTech = mBuildShadowMapTerrainTech;

	dc->OMSetRenderTargets(1, renderTargets, depthStencilView);

	if (dst == DrawPassType::DRAW_PASS_TYPE_NORMAL_DEPTH)
		mTerrainTech = mBuildNormalDepthMapTerrainTech;
	else if (dst == DrawPassType::DRAW_PASS_TYPE_AMBIENT)
		mTerrainTech = mBuildAmbientMapTerrainTech;
	else if (dst == DrawPassType::DRAW_PASS_TYPE_DIFFUSE)
		mTerrainTech = mBuildDiffuseMapTerrainTech;
	else if (dst == DrawPassType::DRAW_PASS_TYPE_DEPTH)
		mTerrainTech = mBuildDepthMapTerrainTech;
	else if (dst == DrawPassType::DRAW_PASS_TYPE_POSITION_WS)
		mTerrainTech = mBuildWSPositionMapTerrainTech;
	else if (dst == DrawPassType::DRAW_PASS_TYPE_VOXEL_GRID)
		mTerrainTech = mGenerateGridTerrainTech;
	else;

	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
	dc->IASetInputLayout(mTerrainInputLayout);

	const UINT stride = sizeof(TerrainVertex);
	const UINT offset = 0;
	dc->IASetVertexBuffers(0, 1, &mTerrainVB, &stride, &offset);
	dc->IASetIndexBuffer(mTerrainIB, DXGI_FORMAT_R16_UINT, 0);

	XMMATRIX viewProj = camera.ViewProj();
	XMMATRIX world = XMLoadFloat4x4(&mTerrainWorld);
	XMMATRIX worldInvTranspose = MathHelper::InverseTranspose(world);
	XMMATRIX worldInv = XMMatrixInverse(&XMMatrixDeterminant(world), world);
	XMMATRIX worldViewProj = world*viewProj;

	XMFLOAT4 worldPlanes[6];
	ExtractFrustumPlanes(worldPlanes, viewProj);

	mfxViewProj->SetMatrix(reinterpret_cast<const float*>(&viewProj));
	mfxWorld->SetMatrix(reinterpret_cast<const float*>(&mTerrainWorld));
	mfxWorldInvTranspose->SetMatrix(reinterpret_cast<const float*>(&worldInvTranspose));
	mfxInvWorld->SetMatrix(reinterpret_cast<const float*>(&worldInv));
	mfxView->SetMatrix(reinterpret_cast<const float*>(&(camera.View())));
	mfxProj->SetMatrix(reinterpret_cast<const float*>(&(camera.Proj())));
	mfxMaterial->SetRawValue(&mTerrainMaterial, 0, sizeof(Material));
	mfxHeightScale->SetFloat(mTerrainHeightScale);

	mfxMinDist->SetFloat(mTerrainMinLODdist);
	mfxMaxDist->SetFloat(mTerrainMaxLODdist);
	mfxMinTess->SetFloat(mTerrainMinTessFactor);
	mfxMaxTess->SetFloat(mTerrainMaxTessFactor);
	mfxTexelCellSpaceU->SetFloat(1.0f / mInfo.HeightmapWidth);
	mfxTexelCellSpaceV->SetFloat(1.0f / mInfo.HeightmapHeight);
	mfxWorldCellSpace->SetFloat(mInfo.CellSpacing);
	mfxWorldFrustrumPlanes->SetFloatVectorArray(reinterpret_cast<float*>(worldPlanes), 0, 6);

	mfxLayerMapArray->SetResource(mLayerMapArraySRV);
	mfxLayerNormalMapArray->SetResource(mLayerNormalMapArraySRV);
	mfxBlendMap->SetResource(mBlendMapSRV);
	mfxHeightMap->SetResource(mHeightMapSRV);

	D3DX11_TECHNIQUE_DESC terrainTechDesc;
	mTerrainTech->GetDesc(&terrainTechDesc);

	for (int i = 0; i < terrainTechDesc.Passes; ++i)
	{
		mTerrainTech->GetPassByIndex(i)->Apply(0, dc);
		dc->DrawIndexed(mNumPatchQuadFaces * 4, 0, 0);
	}

	dc->HSSetShader(0, 0, 0);
	dc->DSSetShader(0, 0, 0);

	mTerrainTech = tempTech;
}

void Terrain::LoadHeightmap()
{
	// A height for each vertex
	std::vector<unsigned char> in(mInfo.HeightmapWidth * mInfo.HeightmapHeight);

	// Open the file.
	std::ifstream inFile;
	inFile.open(mInfo.HeightMapFilename.c_str(), std::ios_base::binary);

	if (inFile)
	{
		// Read the RAW bytes.
		inFile.read((char*)&in[0], (std::streamsize)in.size());

		// Done with file.
		inFile.close();
	}

	// Copy the array data into a float array and scale it.
	mHeightmap.resize(mInfo.HeightmapHeight * mInfo.HeightmapWidth, 0);
	for (int i = 0; i < mInfo.HeightmapHeight * mInfo.HeightmapWidth; ++i)
	{
		mHeightmap[i] = (in[i] / 255.0f)*mInfo.HeightScale;
	}
}

void Terrain::Smooth()
{
	std::vector<float> dest(mHeightmap.size());

	for (int i = 0; i < mInfo.HeightmapHeight; ++i)
	{
		for (int j = 0; j < mInfo.HeightmapWidth; ++j)
		{
			dest[i*mInfo.HeightmapWidth + j] = Average(i, j);
		}
	}

	// Replace the old heightmap with the filtered one.
	mHeightmap = dest;
}

bool Terrain::InBounds(int i, int j)
{
	// True if ij are valid indices; false otherwise.
	return
		i >= 0 && i < (int)mInfo.HeightmapHeight &&
		j >= 0 && j < (int)mInfo.HeightmapWidth;
}

float Terrain::Average(int i, int j)
{
	float avg = 0.0f;
	float num = 0.0f;

	for (int m = i - 1; m <= i + 1; ++m)
	{
		for (int n = j - 1; n <= j + 1; ++n)
		{
			if (InBounds(m, n))
			{
				avg += mHeightmap[m*mInfo.HeightmapWidth + n];
				num += 1.0f;
			}
		}
	}

	return avg / num;
}

void Terrain::CalcAllPatchBoundsY()
{
	mPatchBoundsY.resize(mNumPatchQuadFaces);

	// For each patch
	for (int i = 0; i < mNumPatchVertRows - 1; ++i)
	{
		for (int j = 0; j < mNumPatchVertCols - 1; ++j)
		{
			CalcPatchBoundsY(i, j);
		}
	}
}

void Terrain::CalcPatchBoundsY(int i, int j)
{
	int x0 = j*CellsPerPatch;
	int x1 = (j + 1)*CellsPerPatch;

	int y0 = i*CellsPerPatch;
	int y1 = (i + 1)*CellsPerPatch;

	float minY = +MathHelper::Infinity;
	float maxY = -MathHelper::Infinity;
	for (int y = y0; y <= y1; ++y)
	{
		for (int x = x0; x <= x1; ++x)
		{
			int k = y*mInfo.HeightmapWidth + x;
			minY = MathHelper::Min(minY, mHeightmap[k]);
			maxY = MathHelper::Max(maxY, mHeightmap[k]);
		}
	}

	int patchID = i*(mNumPatchVertCols - 1) + j;
	mPatchBoundsY[patchID] = XMFLOAT2(minY, maxY);
}

void Terrain::BuildVB(ID3D11Device* device)
{
	std::vector<TerrainVertex> patchVertices(mNumPatchVertRows*mNumPatchVertCols);

	float halfWidth = 0.5f*GetWidth();
	float halfDepth = 0.5f*GetDepth();

	float patchWidth = GetWidth() / (mNumPatchVertCols - 1);
	float patchDepth = GetDepth() / (mNumPatchVertRows - 1);
	float du = 1.0f / (mNumPatchVertCols - 1);
	float dv = 1.0f / (mNumPatchVertRows - 1);

	for (int i = 0; i < mNumPatchVertRows; ++i)
	{
		float z = halfDepth - i*patchDepth;
		for (int j = 0; j < mNumPatchVertCols; ++j)
		{
			float x = -halfWidth + j*patchWidth;

			patchVertices[i*mNumPatchVertCols + j].Pos = XMFLOAT3(x, 0.0f, z);

			// Stretch texture over grid.
			patchVertices[i*mNumPatchVertCols + j].Tex.x = j*du;
			patchVertices[i*mNumPatchVertCols + j].Tex.y = i*dv;
		}
	}

	// Store axis-aligned bounding box y-bounds in upper-left patch corner.
	for (int i = 0; i < mNumPatchVertRows - 1; ++i)
	{
		for (int j = 0; j < mNumPatchVertCols - 1; ++j)
		{
			int patchID = i*(mNumPatchVertCols - 1) + j;
			patchVertices[i*mNumPatchVertCols + j].BoundsY = mPatchBoundsY[patchID];
		}
	}

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(TerrainVertex) * patchVertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &patchVertices[0];
	HR(device->CreateBuffer(&vbd, &vinitData, &mTerrainVB));
}

void Terrain::BuildIB(ID3D11Device* device)
{
	std::vector<USHORT> indices(mNumPatchQuadFaces * 4); // 4 indices per quad face

														 // Iterate over each quad and compute indices.
	int k = 0;
	for (int i = 0; i < mNumPatchVertRows - 1; ++i)
	{
		for (int j = 0; j < mNumPatchVertCols - 1; ++j)
		{
			// Top row of 2x2 quad patch
			indices[k] = i*mNumPatchVertCols + j;
			indices[k + 1] = i*mNumPatchVertCols + j + 1;

			// Bottom row of 2x2 quad patch
			indices[k + 2] = (i + 1)*mNumPatchVertCols + j;
			indices[k + 3] = (i + 1)*mNumPatchVertCols + j + 1;

			k += 4; // next quad
		}
	}

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(USHORT)* indices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &indices[0];
	HR(device->CreateBuffer(&ibd, &iinitData, &mTerrainIB));
}

void Terrain::BuildHeightmapSRV(ID3D11Device* device)
{
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = mInfo.HeightmapWidth;
	texDesc.Height = mInfo.HeightmapHeight;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R16_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	// HALF is defined in xnamath.h, for storing 16-bit float.
	std::vector<HALF> hmap(mHeightmap.size());
	std::transform(mHeightmap.begin(), mHeightmap.end(), hmap.begin(), XMConvertFloatToHalf);

	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = &hmap[0];
	data.SysMemPitch = mInfo.HeightmapWidth*sizeof(HALF);
	data.SysMemSlicePitch = 0;

	ID3D11Texture2D* hmapTex = 0;
	HR(device->CreateTexture2D(&texDesc, &data, &hmapTex));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	HR(device->CreateShaderResourceView(hmapTex, &srvDesc, &mHeightMapSRV));

	// SRV saves reference.
	ReleaseCOM(hmapTex);
}

void Terrain::BuildFX()
{
	mTerrainTech = mTerrainFX->GetTechniqueByName("TerrainTech");
	mBuildShadowMapTerrainTech = mTerrainFX->GetTechniqueByName("BuildShadowMapTerrainTech");
	mBuildNormalDepthMapTerrainTech = mTerrainFX->GetTechniqueByName("BuildNormalDepthMapTerrainTech");
	mBuildAmbientMapTerrainTech = mTerrainFX->GetTechniqueByName("BuildAmbientMapTerrainTech");
	mBuildDiffuseMapTerrainTech = mTerrainFX->GetTechniqueByName("BuildDiffuseMapTerrainTech");
	mBuildDepthMapTerrainTech = mTerrainFX->GetTechniqueByName("BuildDepthMapTerrainTech");
	mBuildWSPositionMapTerrainTech = mTerrainFX->GetTechniqueByName("BuildWSPositionMapTerrainTech");
	mGenerateGridTerrainTech = mTerrainFX->GetTechniqueByName("GenerateGridTerrainTech");
	mfxViewProj = mTerrainFX->GetVariableByName("gViewProj")->AsMatrix();
	mfxProj = mTerrainFX->GetVariableByName("gProj")->AsMatrix();
	mfxView = mTerrainFX->GetVariableByName("gView")->AsMatrix();
	mfxWorld = mTerrainFX->GetVariableByName("gWorld")->AsMatrix();
	mfxInvWorld = mTerrainFX->GetVariableByName("gInvWorld")->AsMatrix();
	mfxWorldInvTranspose = mTerrainFX->GetVariableByName("gWorldInvTranspose")->AsMatrix();
	mfxHeightScale = mTerrainFX->GetVariableByName("gHeightScale")->AsScalar();
	mfxMinDist = mTerrainFX->GetVariableByName("gMinDist")->AsScalar();
	mfxMaxDist = mTerrainFX->GetVariableByName("gMaxDist")->AsScalar();
	mfxMinTess = mTerrainFX->GetVariableByName("gMinTess")->AsScalar();
	mfxMaxTess = mTerrainFX->GetVariableByName("gMaxTess")->AsScalar();
	mfxTexelCellSpaceU = mTerrainFX->GetVariableByName("gTexelCellSpaceU")->AsScalar();
	mfxTexelCellSpaceV = mTerrainFX->GetVariableByName("gTexelCellSpaceV")->AsScalar();
	mfxWorldCellSpace = mTerrainFX->GetVariableByName("gWorldCellSpace")->AsScalar();
	mfxWorldFrustrumPlanes = mTerrainFX->GetVariableByName("gWorldFrustrumPlanes")->AsVector();
	mfxLayerMapArray = mTerrainFX->GetVariableByName("gLayerMapArray")->AsShaderResource();
	mfxLayerNormalMapArray = mTerrainFX->GetVariableByName("gLayerNormalMapArray")->AsShaderResource();
	mfxBlendMap = mTerrainFX->GetVariableByName("gBlendMap")->AsShaderResource();
	mfxHeightMap = mTerrainFX->GetVariableByName("gHeightMap")->AsShaderResource();
	mfxMaterial = mTerrainFX->GetVariableByName("gMaterial");
}

void Terrain::BuildVertexInputLayout(ID3D11Device* device)
{
	static const D3D11_INPUT_ELEMENT_DESC TerrainDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	D3DX11_PASS_DESC passDesc;

	mTerrainTech->GetPassByIndex(0)->GetDesc(&passDesc);

	HR(device->CreateInputLayout(TerrainDesc, 3, passDesc.pIAInputSignature,
		passDesc.IAInputSignatureSize, &mTerrainInputLayout));
}