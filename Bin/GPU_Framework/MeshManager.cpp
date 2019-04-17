#include "MeshManager.h"

using namespace std;

MeshManager::MeshManager(ID3D11Device* device, ID3D11DeviceContext* dc, ID3DX11EffectTechnique* tech, int CubeMapSz, int planarRefrSz) : Meshes(0), MeshCount(0), CubeMapSize(CubeMapSz), PlanarRefractionTexSize(planarRefrSz)
{
	md3dDevice = device;
	m3dDeviceContext = dc;

	BuildVertexInputLayout(tech);
}

MeshManager::~MeshManager()
{
	for (int i = 0; i < MeshCount; ++i) {
		ReleaseCOM(Meshes[i].mVB);
		if (Meshes[i].mObjectDynamicCubeMapDSV != NULL && Meshes[i].mObjectDynamicCubeMapSRV != NULL) {
			ReleaseCOM(Meshes[i].mObjectDynamicCubeMapDSV);
			ReleaseCOM(Meshes[i].mObjectDynamicCubeMapSRV);
		}
		if (Meshes[i].drawType == DrawType::INDEXED_DRAW)
			ReleaseCOM(Meshes[i].mIB);
		if (Meshes[i].mUseTextures)
			ReleaseCOM(Meshes[i].mDiffuseMapArraySRV);
	}
	ReleaseCOM(md3dDevice);
	ReleaseCOM(m3dDeviceContext);
	ReleaseCOM(mInputLayout);
}

void MeshManager::BuildVertexInputLayout(ID3DX11EffectTechnique* tech)
{
	static const D3D11_INPUT_ELEMENT_DESC BasicDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	D3DX11_PASS_DESC passDesc;

	tech->GetPassByIndex(0)->GetDesc(&passDesc);

	HR(md3dDevice->CreateInputLayout(BasicDesc, 4, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize,
		&mInputLayout));
}

void MeshManager::BuildDynamicCubeMapViews(int MeshIndex)
{
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = CubeMapSize;
	texDesc.Height = CubeMapSize;
	texDesc.MipLevels = 0;
	texDesc.ArraySize = 6;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS | D3D11_RESOURCE_MISC_TEXTURECUBE;

	ID3D11Texture2D* cubeTex = 0;
	HR(md3dDevice->CreateTexture2D(&texDesc, 0, &cubeTex));

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = texDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	rtvDesc.Texture2DArray.ArraySize = 1;
	rtvDesc.Texture2DArray.MipSlice = 0;

	for (int i = 0; i < 6; ++i)
	{
		rtvDesc.Texture2DArray.FirstArraySlice = i;
		HR(md3dDevice->CreateRenderTargetView(cubeTex, &rtvDesc, &Meshes[MeshIndex].mObjectDynamicCubeMapRTV[i]));
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = -1;

	HR(md3dDevice->CreateShaderResourceView(cubeTex, &srvDesc, &Meshes[MeshIndex].mObjectDynamicCubeMapSRV));

	ReleaseCOM(cubeTex);

	D3D11_TEXTURE2D_DESC depthTexDesc;
	depthTexDesc.Width = CubeMapSize;
	depthTexDesc.Height = CubeMapSize;
	depthTexDesc.MipLevels = 1;
	depthTexDesc.ArraySize = 1;
	depthTexDesc.SampleDesc.Count = 1;
	depthTexDesc.SampleDesc.Quality = 0;
	depthTexDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
	depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthTexDesc.CPUAccessFlags = 0;
	depthTexDesc.MiscFlags = 0;

	ID3D11Texture2D* depthTex = 0;
	HR(md3dDevice->CreateTexture2D(&depthTexDesc, 0, &depthTex));

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Format = depthTexDesc.Format;
	dsvDesc.Flags = 0;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	HR(md3dDevice->CreateDepthStencilView(depthTex, &dsvDesc, &Meshes[MeshIndex].mObjectDynamicCubeMapDSV));

	ReleaseCOM(depthTex);

	Meshes[MeshIndex].mCubeMapViewport.TopLeftX = 0.0f;
	Meshes[MeshIndex].mCubeMapViewport.TopLeftY = 0.0f;
	Meshes[MeshIndex].mCubeMapViewport.Width = (float)CubeMapSize;
	Meshes[MeshIndex].mCubeMapViewport.Height = (float)CubeMapSize;
	Meshes[MeshIndex].mCubeMapViewport.MinDepth = 0.0f;
	Meshes[MeshIndex].mCubeMapViewport.MaxDepth = 1.0f;
}

void MeshManager::BuildPlanarRefractionMapViews(int MeshIndex)
{
	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = PlanarRefractionTexSize;
	texDesc.Height = PlanarRefractionTexSize;
	texDesc.MipLevels = 0;
	texDesc.ArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	ID3D11Texture2D* Tex = 0;
	HR(md3dDevice->CreateTexture2D(&texDesc, 0, &Tex));

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = texDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2DArray.ArraySize = 1;
	rtvDesc.Texture2DArray.MipSlice = 0;

	HR(md3dDevice->CreateRenderTargetView(Tex, &rtvDesc, &Meshes[MeshIndex].mObjectPlanarRefractionMapRTV));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = -1;

	HR(md3dDevice->CreateShaderResourceView(Tex, &srvDesc, &Meshes[MeshIndex].mObjectPlanarRefractionMapSRV));

	ReleaseCOM(Tex);

	D3D11_TEXTURE2D_DESC depthTexDesc;
	depthTexDesc.Width = PlanarRefractionTexSize;
	depthTexDesc.Height = PlanarRefractionTexSize;
	depthTexDesc.MipLevels = 1;
	depthTexDesc.ArraySize = 1;
	depthTexDesc.SampleDesc.Count = 1;
	depthTexDesc.SampleDesc.Quality = 0;
	depthTexDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
	depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthTexDesc.CPUAccessFlags = 0;
	depthTexDesc.MiscFlags = 0;

	ID3D11Texture2D* depthTex = 0;
	HR(md3dDevice->CreateTexture2D(&depthTexDesc, 0, &depthTex));

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Format = depthTexDesc.Format;
	dsvDesc.Flags = 0;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	HR(md3dDevice->CreateDepthStencilView(depthTex, &dsvDesc, &Meshes[MeshIndex].mObjectPlanarRefractionMapDSV));

	ReleaseCOM(depthTex);

	Meshes[MeshIndex].mPlanarRefractionMapViewport.TopLeftX = 0.0f;
	Meshes[MeshIndex].mPlanarRefractionMapViewport.TopLeftY = 0.0f;
	Meshes[MeshIndex].mPlanarRefractionMapViewport.Width = (float)PlanarRefractionTexSize;
	Meshes[MeshIndex].mPlanarRefractionMapViewport.Height = (float)PlanarRefractionTexSize;
	Meshes[MeshIndex].mPlanarRefractionMapViewport.MinDepth = 0.0f;
	Meshes[MeshIndex].mPlanarRefractionMapViewport.MaxDepth = 1.0f;
}

void MeshManager::CreateFbxScene(D3D_PRIMITIVE_TOPOLOGY primitiveTopology, BuffersOptimization bo, DrawType drawType, XMMATRIX *world, ID3DX11EffectTechnique* ObjectTech, std::string SceneFilename, std::string SceneName,
								 bool UseCulling, bool UseTransparency, UINT InstancesCount, ReflectionType reflectionType, RefractionType refractionType)
{	
	pFbxLoader.push_back(NULL);
	pFbxLoader[pFbxLoader.size() - 1] = new FBXLoader(SceneFilename.c_str(), SceneName.c_str(), bo);

	for (int c = 0; c < pFbxLoader[pFbxLoader.size() - 1]->mNodeList.size(); ++c)
	{
		for (int f = 0; f < pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes.size(); ++f)
		{
			if (pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mVertices.size() > 0 &&
				pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mIndicies.size() > 0)
			{

				Object *TempObj = new Object[MeshCount];

				for (int i = 0; i < MeshCount; ++i)
					TempObj[i] = Meshes[i];

				delete[]  Meshes;

				Meshes = new Object[MeshCount + 1];

				for (int i = 0; i < MeshCount; ++i)
					Meshes[i] = TempObj[i];

				delete[]  TempObj;

				MeshCount++;

				Meshes[MeshCount - 1].mObjectDynamicCubeMapSRV = 0;
				Meshes[MeshCount - 1].mObjectDynamicCubeMapDSV = 0;
				if (reflectionType == ReflectionType::REFLECTION_TYPE_DYNAMIC_CUBE_MAP ||
					reflectionType == ReflectionType::REFLECTION_TYPE_DYNAMIC_CUBE_MAP_AND_SSLR ||
					refractionType == RefractionType::REFRACTION_TYPE_DYNAMIC_CUBE_MAP)
					BuildDynamicCubeMapViews(MeshCount - 1);

				Meshes[MeshCount - 1].mObjectPlanarRefractionMapDSV = 0;
				Meshes[MeshCount - 1].mObjectPlanarRefractionMapRTV = 0;
				Meshes[MeshCount - 1].mObjectPlanarRefractionMapSRV = 0;
				if (refractionType == RefractionType::REFRACTION_TYPE_PLANAR)
					BuildPlanarRefractionMapViews(MeshCount - 1);

				Meshes[MeshCount - 1].drawType = drawType;
				Meshes[MeshCount - 1].primitiveTopology = primitiveTopology;

				FbxNode* pNode = pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mNode;
				FbxAMatrix fWorld = pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mGlobalTransform;

				XMMATRIX xmWorld = XMLoadFloat4x4(&XMFLOAT4X4(fWorld.mData[0].mData[0], fWorld.mData[0].mData[1], fWorld.mData[0].mData[2], fWorld.mData[0].mData[3],
					fWorld.mData[1].mData[0], fWorld.mData[1].mData[1], fWorld.mData[1].mData[2], fWorld.mData[1].mData[3],
					fWorld.mData[2].mData[0], fWorld.mData[2].mData[1], fWorld.mData[2].mData[2], fWorld.mData[2].mData[3],
					fWorld.mData[3].mData[0], fWorld.mData[3].mData[1], fWorld.mData[3].mData[2], fWorld.mData[3].mData[3]));

				for(int i = 0; i < InstancesCount; ++i)
					XMStoreFloat4x4(&Meshes[MeshCount - 1].mWorld[i], xmWorld*world[i]);
				XMStoreFloat4x4(&Meshes[MeshCount - 1].mLocal, xmWorld);

				FbxAMatrix fTexTransform;

				fTexTransform.SetIdentity();
				XMStoreFloat4x4(&Meshes[MeshCount - 1].mTexTransform[0], I);

				for (int q = 0; q < pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mTextures.size(); ++q)
				{
					fTexTransform = pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mTextures[q].TexTransform;

					XMMATRIX xmTexTransform = XMLoadFloat4x4(&XMFLOAT4X4(fTexTransform.mData[0].mData[0], fTexTransform.mData[0].mData[1], fTexTransform.mData[0].mData[2], fTexTransform.mData[0].mData[3],
						fTexTransform.mData[1].mData[0], fTexTransform.mData[1].mData[1], fTexTransform.mData[1].mData[2], fTexTransform.mData[1].mData[3],
						fTexTransform.mData[2].mData[0], fTexTransform.mData[2].mData[1], fTexTransform.mData[2].mData[2], fTexTransform.mData[2].mData[3],
						fTexTransform.mData[3].mData[0], fTexTransform.mData[3].mData[1], fTexTransform.mData[3].mData[2], fTexTransform.mData[3].mData[3]));

					XMStoreFloat4x4(&Meshes[MeshCount - 1].mTexTransform[q], xmTexTransform);
				}

				std::vector<std::wstring>Filenames(0);
				for (int i = 0; i < pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mTextures.size(); ++i) {
					if (pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mTextures[i].TexFilename.size() != 0)
						Filenames.push_back(pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mTextures[i].TexFilename);
				}

				Meshes[MeshCount - 1].mUseTextures = false;
				Meshes[MeshCount - 1].mUseNormalMapping = false;
				Meshes[MeshCount - 1].mUseDisplacementMapping = false;
				Meshes[MeshCount - 1].mNormalMapSRV = 0;
				Meshes[MeshCount - 1].mReflectMapSRV = 0;
				Meshes[MeshCount - 1].mMaterialType = MaterialType::MTL_TYPE_DEFAULT;
				Meshes[MeshCount - 1].mInstancesCount = InstancesCount;

				int TexCount = 0;
				for (int i = 0; i < Filenames.size(); ++i)
				{
					if (pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mTextures[i].TexType == TEX_TYPE_DIFFUSE_MAP ||
						pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mTextures[i].TexType == TEX_TYPE_TRANSPARENT_MAP)
					{
						Meshes[MeshCount - 1].mExtDiffuseMapArraySRV[TexCount] = d3dHelper::CreateTexture2DSRVW(md3dDevice, m3dDeviceContext, Filenames[i]);
						Meshes[MeshCount - 1].mTexType[TexCount] = pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mTextures[i].TexType;
						Meshes[MeshCount - 1].mUseTextures = true;
						TexCount++;
					}
					else if (pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mTextures[i].TexType == TEX_TYPE_NORMAL_MAP)
					{
						Meshes[MeshCount - 1].mNormalMapSRV = d3dHelper::CreateTexture2DSRVW(md3dDevice, m3dDeviceContext, Filenames[i]);
						Meshes[MeshCount - 1].mUseNormalMapping = true;
						Meshes[MeshCount - 1].mUseDisplacementMapping = true;
					}
					else if (pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mTextures[i].TexType == TEX_TYPE_SHININESS_MAP)
					{
						Meshes[MeshCount - 1].mReflectMapSRV = d3dHelper::CreateTexture2DSRVW(md3dDevice, m3dDeviceContext, Filenames[i]);
						Meshes[MeshCount - 1].mMaterialType = MaterialType::MTL_TYPE_REFLECT_MAP;
					}
				}
				Meshes[MeshCount - 1].mDiffuseMapArraySRV = NULL;
				Meshes[MeshCount - 1].mTexArrayType = TexArrayType::DIFFERENT_FORMAT;

				Meshes[MeshCount - 1].mMeshType = MeshType::FBX_MESH;

				Meshes[MeshCount - 1].mTexCount = TexCount;
				Meshes[MeshCount - 1].mMaterial = pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mMeshMtl;
				Meshes[MeshCount - 1].mMaterial.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
				Meshes[MeshCount - 1].mUseTransparency = UseTransparency;
				Meshes[MeshCount - 1].mUseCulling = UseCulling;
				Meshes[MeshCount - 1].mReflectionType = reflectionType;
				Meshes[MeshCount - 1].mRefractionType = refractionType;
				Meshes[MeshCount - 1].mUseAdditiveBlend = false;
				Meshes[MeshCount - 1].mObjectDrawTech = ObjectTech;

				std::vector<MeshVertex>verts(0);
				std::vector<XMFLOAT3>vertspos(0);
				std::vector<int>inds(0);
				if ((pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mMesh) != NULL) {
					for (int i = 0; i < pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mIndicies.size(); ++i) {
						inds.push_back(pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mIndicies[i] + verts.size());
					}
					for (int i = 0; i < pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mVertices.size(); ++i) {
						MeshVertex newvert = { pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mVertices[i].Pos, pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mVertices[i].Normal,
							pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mVertices[i].TexCoord, pFbxLoader[pFbxLoader.size() - 1]->mNodeList[c].mMeshes[f].mVertices[i].Tangent };
						verts.push_back(newvert);
						vertspos.push_back(newvert.Pos);
					}
				}

				BoundingOrientedBox::CreateFromPoints(Meshes[MeshCount - 1].OBB, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));
				BoundingBox::CreateFromPoints(Meshes[MeshCount - 1].AABB, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));
				BoundingSphere::CreateFromPoints(Meshes[MeshCount - 1].BS, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));

				Meshes[MeshCount - 1].mVertexCount = verts.size();
				Meshes[MeshCount - 1].mIndexCount = inds.size();

				D3D11_BUFFER_DESC vbd;
				vbd.Usage = D3D11_USAGE_DYNAMIC;
				vbd.ByteWidth = sizeof(MeshVertex) * verts.size();
				vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
				vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				vbd.MiscFlags = 0;

				D3D11_SUBRESOURCE_DATA vinitData;
				vinitData.pSysMem = &verts[0];

				HR(md3dDevice->CreateBuffer(&vbd, &vinitData, &Meshes[MeshCount - 1].mVB));

				D3D11_BUFFER_DESC ibd;
				ibd.Usage = D3D11_USAGE_DYNAMIC;
				ibd.ByteWidth = sizeof(int) * inds.size();
				ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
				ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				ibd.MiscFlags = 0;
				ibd.StructureByteStride = 0;

				D3D11_SUBRESOURCE_DATA iinitData;
				iinitData.pSysMem = &inds[0];

				HR(md3dDevice->CreateBuffer(&ibd, &iinitData, &Meshes[MeshCount - 1].mIB));

				verts.clear();
				inds.clear();
			}
		}
	}
}

void MeshManager::CreateBox(D3D_PRIMITIVE_TOPOLOGY primitiveTopology, DrawType drawType, XMMATRIX *TexTransform, XMMATRIX *world, ID3DX11EffectTechnique* ObjectTech,
	std::vector<std::wstring>&TexFilenames, std::vector<std::wstring>&NormalMapFilenames, int TexCount, bool UseTextures, bool UseTransparecy, bool UseAdditiveBlend, bool UseCulling, bool mUseNormalMapping, bool UseDisplacementMapping, UINT InstancesCount, ReflectionType reflectionType,
	RefractionType refractionType, Material material, float width, float height, float depth)
{
	Object *TempObj = new Object[MeshCount];

	for (int i = 0; i < MeshCount; ++i)
		TempObj[i] = Meshes[i];

	delete[]  Meshes;

	Meshes = new Object[MeshCount + 1];

	for (int i = 0; i < MeshCount; ++i)
		Meshes[i] = TempObj[i];

	delete[] TempObj;

	MeshCount++;

	Meshes[MeshCount - 1].mObjectDynamicCubeMapSRV = 0;
	Meshes[MeshCount - 1].mObjectDynamicCubeMapDSV = 0;
	if (reflectionType == ReflectionType::REFLECTION_TYPE_DYNAMIC_CUBE_MAP ||
		reflectionType == ReflectionType::REFLECTION_TYPE_DYNAMIC_CUBE_MAP_AND_SSLR ||
		refractionType == RefractionType::REFRACTION_TYPE_DYNAMIC_CUBE_MAP)
		BuildDynamicCubeMapViews(MeshCount - 1);

	Meshes[MeshCount - 1].mObjectPlanarRefractionMapDSV = 0;
	Meshes[MeshCount - 1].mObjectPlanarRefractionMapRTV = 0;
	Meshes[MeshCount - 1].mObjectPlanarRefractionMapSRV = 0;
	if (refractionType == RefractionType::REFRACTION_TYPE_PLANAR)
		BuildPlanarRefractionMapViews(MeshCount - 1);

	Meshes[MeshCount - 1].drawType = drawType;
	Meshes[MeshCount - 1].primitiveTopology = primitiveTopology;

	XMStoreFloat4x4(&Meshes[MeshCount - 1].mTexTransform[0], TexTransform[0]);
	for(int i = 0; i < InstancesCount; ++i)
		XMStoreFloat4x4(&Meshes[MeshCount - 1].mWorld[i], world[i]);
	XMStoreFloat4x4(&Meshes[MeshCount - 1].mLocal, I);

	Meshes[MeshCount - 1].mTexCount = TexCount;
	Meshes[MeshCount - 1].mUseTextures = UseTextures;
	Meshes[MeshCount - 1].mMaterial = material;
	Meshes[MeshCount - 1].mUseTransparency = UseTransparecy;
	Meshes[MeshCount - 1].mUseAdditiveBlend = UseAdditiveBlend;
	Meshes[MeshCount - 1].mUseCulling = UseCulling;
	Meshes[MeshCount - 1].mReflectionType = reflectionType;
	Meshes[MeshCount - 1].mRefractionType = refractionType;
	Meshes[MeshCount - 1].mObjectDrawTech = ObjectTech;
	Meshes[MeshCount - 1].mUseDisplacementMapping = UseDisplacementMapping;
	Meshes[MeshCount - 1].mUseNormalMapping = mUseNormalMapping;
	Meshes[MeshCount - 1].mInstancesCount = InstancesCount;
	Meshes[MeshCount - 1].mReflectMapSRV = 0;
	Meshes[MeshCount - 1].mMaterialType = MaterialType::MTL_TYPE_DEFAULT;

	if (UseTextures)
		Meshes[MeshCount - 1].mDiffuseMapArraySRV = d3dHelper::CreateTexture2DArraySRVW(md3dDevice, m3dDeviceContext, TexFilenames);

	for (int i = 1; i < TexFilenames.size(); ++i)
	{
		Meshes[MeshCount - 1].mExtDiffuseMapArraySRV[i] = NULL;
	}
	Meshes[MeshCount - 1].mTexArrayType = TexArrayType::EQUAL_FORMAT;

	for (int i = 0; i < TexFilenames.size(); ++i)
	{
		Meshes[MeshCount - 1].mTexType[i] = TextureType::TEX_TYPE_DIFFUSE_MAP;
	}

	Meshes[MeshCount - 1].mMeshType = MeshType::PROCEDURAL_MESH;

	if (mUseNormalMapping)
		Meshes[MeshCount - 1].mNormalMapSRV = d3dHelper::CreateTexture2DSRVW(md3dDevice, m3dDeviceContext, NormalMapFilenames[0].c_str());
	else
		Meshes[MeshCount - 1].mNormalMapSRV = 0;

	GeometryGenerator GeoGen;
	GeometryGenerator::MeshData Box;

	GeoGen.CreateBox(width, height, depth, Box);

	std::vector<MeshVertex>vertices(Box.Vertices.size());
	std::vector<XMFLOAT3>vertspos(Box.Vertices.size());

	for (int i = 0; i < Box.Vertices.size(); ++i)
	{
		vertices[i].Pos = Box.Vertices[i].Position;
		vertices[i].Normal = Box.Vertices[i].Normal;
		vertices[i].TexCoord = Box.Vertices[i].TexC;
		vertices[i].Tangent = Box.Vertices[i].TangentU;
		vertspos[i] = vertices[i].Pos;
	}

	BoundingOrientedBox::CreateFromPoints(Meshes[MeshCount - 1].OBB, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));
	BoundingBox::CreateFromPoints(Meshes[MeshCount - 1].AABB, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));
	BoundingSphere::CreateFromPoints(Meshes[MeshCount - 1].BS, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));

	Meshes[MeshCount - 1].mIndexCount = Box.Indices.size();
	Meshes[MeshCount - 1].mVertexCount = Box.Vertices.size();

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_DYNAMIC;
	vbd.ByteWidth = sizeof(MeshVertex) * Box.Vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vbd.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &vertices[0];

	HR(md3dDevice->CreateBuffer(&vbd, &vinitData, &Meshes[MeshCount - 1].mVB));

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_DYNAMIC;
	ibd.ByteWidth = sizeof(int) * Box.Indices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &Box.Indices[0];

	HR(md3dDevice->CreateBuffer(&ibd, &iinitData, &Meshes[MeshCount - 1].mIB));
}

void MeshManager::CreateSphere(D3D_PRIMITIVE_TOPOLOGY primitiveTopology, DrawType drawType, XMMATRIX *TexTransform, XMMATRIX *world, ID3DX11EffectTechnique* ObjectTech,
	std::vector<std::wstring>&TexFilenames, std::vector<std::wstring>&NormalMapFilenames, int TexCount, bool UseTextures, bool UseTransparecy, bool UseAdditiveBlend, bool UseCulling, bool mUseNormalMapping, bool UseDisplacementMapping, UINT InstancesCount, ReflectionType reflectionType,
	RefractionType refractionType, Material material, float radius, int sliceCount, int stackCount)
{
	Object *TempObj = new Object[MeshCount];

	for (int i = 0; i < MeshCount; ++i)
		TempObj[i] = Meshes[i];

	delete[]  Meshes;

	Meshes = new Object[MeshCount + 1];

	for (int i = 0; i < MeshCount; ++i)
		Meshes[i] = TempObj[i];

	delete[] TempObj;

	MeshCount++;

	Meshes[MeshCount - 1].mObjectDynamicCubeMapSRV = 0;
	Meshes[MeshCount - 1].mObjectDynamicCubeMapDSV = 0;
	if (reflectionType == ReflectionType::REFLECTION_TYPE_DYNAMIC_CUBE_MAP ||
		reflectionType == ReflectionType::REFLECTION_TYPE_DYNAMIC_CUBE_MAP_AND_SSLR ||
		refractionType == RefractionType::REFRACTION_TYPE_DYNAMIC_CUBE_MAP)
		BuildDynamicCubeMapViews(MeshCount - 1);

	Meshes[MeshCount - 1].mObjectPlanarRefractionMapDSV = 0;
	Meshes[MeshCount - 1].mObjectPlanarRefractionMapRTV = 0;
	Meshes[MeshCount - 1].mObjectPlanarRefractionMapSRV = 0;
	if (refractionType == RefractionType::REFRACTION_TYPE_PLANAR)
		BuildPlanarRefractionMapViews(MeshCount - 1);

	Meshes[MeshCount - 1].drawType = drawType;
	Meshes[MeshCount - 1].primitiveTopology = primitiveTopology;

	XMStoreFloat4x4(&Meshes[MeshCount - 1].mTexTransform[0], TexTransform[0]);
	for (int i = 0; i < InstancesCount; ++i)
		XMStoreFloat4x4(&Meshes[MeshCount - 1].mWorld[i], world[i]);
	XMStoreFloat4x4(&Meshes[MeshCount - 1].mLocal, I);

	Meshes[MeshCount - 1].mTexCount = TexCount;
	Meshes[MeshCount - 1].mUseTextures = UseTextures;
	Meshes[MeshCount - 1].mMaterial = material;
	Meshes[MeshCount - 1].mUseTransparency = UseTransparecy;
	Meshes[MeshCount - 1].mUseAdditiveBlend = UseAdditiveBlend;
	Meshes[MeshCount - 1].mUseCulling = UseCulling;
	Meshes[MeshCount - 1].mReflectionType = reflectionType;
	Meshes[MeshCount - 1].mRefractionType = refractionType;
	Meshes[MeshCount - 1].mObjectDrawTech = ObjectTech;
	Meshes[MeshCount - 1].mUseDisplacementMapping = UseDisplacementMapping;
	Meshes[MeshCount - 1].mUseNormalMapping = mUseNormalMapping;
	Meshes[MeshCount - 1].mInstancesCount = InstancesCount;
	Meshes[MeshCount - 1].mReflectMapSRV = 0;
	Meshes[MeshCount - 1].mMaterialType = MaterialType::MTL_TYPE_DEFAULT;

	if(UseTextures)
		Meshes[MeshCount - 1].mDiffuseMapArraySRV = d3dHelper::CreateTexture2DArraySRVW(md3dDevice, m3dDeviceContext, TexFilenames);

	for (int i = 1; i < TexFilenames.size(); ++i)
	{
		Meshes[MeshCount - 1].mExtDiffuseMapArraySRV[i] = NULL;
	}
	Meshes[MeshCount - 1].mTexArrayType = TexArrayType::EQUAL_FORMAT;

	for (int i = 0; i < TexFilenames.size(); ++i)
	{
		Meshes[MeshCount - 1].mTexType[i] = TextureType::TEX_TYPE_DIFFUSE_MAP;
	}

	Meshes[MeshCount - 1].mMeshType = MeshType::PROCEDURAL_MESH;

	if (mUseNormalMapping)
		Meshes[MeshCount - 1].mNormalMapSRV = d3dHelper::CreateTexture2DSRVW(md3dDevice, m3dDeviceContext, NormalMapFilenames[0].c_str());
	else
		Meshes[MeshCount - 1].mNormalMapSRV = 0;

	GeometryGenerator GeoGen;
	GeometryGenerator::MeshData Sphere;

	GeoGen.CreateSphere(radius, sliceCount, stackCount, Sphere);

	std::vector<MeshVertex>vertices(Sphere.Vertices.size());
	std::vector<XMFLOAT3>vertspos(Sphere.Vertices.size());

	for (int i = 0; i < Sphere.Vertices.size(); ++i)
	{
		vertices[i].Pos = Sphere.Vertices[i].Position;
		vertices[i].Normal = Sphere.Vertices[i].Normal;
		vertices[i].TexCoord = Sphere.Vertices[i].TexC;
		vertices[i].Tangent = Sphere.Vertices[i].TangentU;
		vertspos[i] = vertices[i].Pos;
	}

	BoundingOrientedBox::CreateFromPoints(Meshes[MeshCount - 1].OBB, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));
	BoundingBox::CreateFromPoints(Meshes[MeshCount - 1].AABB, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));
	BoundingSphere::CreateFromPoints(Meshes[MeshCount - 1].BS, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));

	Meshes[MeshCount - 1].mIndexCount = Sphere.Indices.size();
	Meshes[MeshCount - 1].mVertexCount = Sphere.Vertices.size();

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_DYNAMIC;
	vbd.ByteWidth = sizeof(MeshVertex) * Sphere.Vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vbd.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &vertices[0];

	HR(md3dDevice->CreateBuffer(&vbd, &vinitData, &Meshes[MeshCount - 1].mVB));

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_DYNAMIC;
	ibd.ByteWidth = sizeof(int) * Sphere.Indices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &Sphere.Indices[0];

	HR(md3dDevice->CreateBuffer(&ibd, &iinitData, &Meshes[MeshCount - 1].mIB));
}

void MeshManager::CreateGrid(D3D_PRIMITIVE_TOPOLOGY primitiveTopology, DrawType drawType, XMMATRIX *TexTransform, XMMATRIX *world, ID3DX11EffectTechnique* ObjectTech,
	std::vector<std::wstring>&TexFilenames, std::vector<std::wstring>&NormalMapFilenames, int TexCount, bool UseTextures, bool UseTransparecy, bool UseAdditiveBlend, bool UseCulling, bool mUseNormalMapping, bool UseDisplacementMapping, UINT InstancesCount, ReflectionType reflectionType,
	RefractionType refractionType, Material material, float width, float depth, int m, int n)
{
	Object *TempObj = new Object[MeshCount];

	for (int i = 0; i < MeshCount; ++i)
		TempObj[i] = Meshes[i];

	delete[]  Meshes;

	Meshes = new Object[MeshCount + 1];

	for (int i = 0; i < MeshCount; ++i)
		Meshes[i] = TempObj[i];

	delete[] TempObj;

	MeshCount++;

	Meshes[MeshCount - 1].mObjectDynamicCubeMapSRV = 0;
	Meshes[MeshCount - 1].mObjectDynamicCubeMapDSV = 0;
	if (reflectionType == ReflectionType::REFLECTION_TYPE_DYNAMIC_CUBE_MAP ||
		reflectionType == ReflectionType::REFLECTION_TYPE_DYNAMIC_CUBE_MAP_AND_SSLR ||
		refractionType == RefractionType::REFRACTION_TYPE_DYNAMIC_CUBE_MAP)
		BuildDynamicCubeMapViews(MeshCount - 1);

	Meshes[MeshCount - 1].mObjectPlanarRefractionMapDSV = 0;
	Meshes[MeshCount - 1].mObjectPlanarRefractionMapRTV = 0;
	Meshes[MeshCount - 1].mObjectPlanarRefractionMapSRV = 0;
	if(refractionType == RefractionType::REFRACTION_TYPE_PLANAR)
		BuildPlanarRefractionMapViews(MeshCount - 1);

	Meshes[MeshCount - 1].drawType = drawType;
	Meshes[MeshCount - 1].primitiveTopology = primitiveTopology;

	for (int y = 0; y < TexCount; ++y)
		XMStoreFloat4x4(&Meshes[MeshCount - 1].mTexTransform[y], TexTransform[y]);

	for (int i = 0; i < InstancesCount; ++i)
		XMStoreFloat4x4(&Meshes[MeshCount - 1].mWorld[i], world[i]);
	XMStoreFloat4x4(&Meshes[MeshCount - 1].mLocal, I);

	Meshes[MeshCount - 1].mTexCount = TexCount;
	Meshes[MeshCount - 1].mUseTextures = UseTextures;
	Meshes[MeshCount - 1].mMaterial = material;
	Meshes[MeshCount - 1].mUseTransparency = UseTransparecy;
	Meshes[MeshCount - 1].mUseAdditiveBlend = UseAdditiveBlend;
	Meshes[MeshCount - 1].mUseCulling = UseCulling;
	Meshes[MeshCount - 1].mReflectionType = reflectionType;
	Meshes[MeshCount - 1].mRefractionType = refractionType;
	Meshes[MeshCount - 1].mObjectDrawTech = ObjectTech;
	Meshes[MeshCount - 1].mUseDisplacementMapping = UseDisplacementMapping;
	Meshes[MeshCount - 1].mUseNormalMapping = mUseNormalMapping;
	Meshes[MeshCount - 1].mInstancesCount = InstancesCount;
	Meshes[MeshCount - 1].mReflectMapSRV = 0;
	Meshes[MeshCount - 1].mMaterialType = MaterialType::MTL_TYPE_DEFAULT;

	if (UseTextures)
		Meshes[MeshCount - 1].mDiffuseMapArraySRV = d3dHelper::CreateTexture2DArraySRVW(md3dDevice, m3dDeviceContext, TexFilenames);

	for (int i = 1; i < TexFilenames.size(); ++i)
	{
		Meshes[MeshCount - 1].mExtDiffuseMapArraySRV[i] = NULL;
	}
	Meshes[MeshCount - 1].mTexArrayType = TexArrayType::EQUAL_FORMAT;

	for (int i = 0; i < TexFilenames.size(); ++i)
	{
		Meshes[MeshCount - 1].mTexType[i] = TextureType::TEX_TYPE_DIFFUSE_MAP;
	}

	Meshes[MeshCount - 1].mMeshType = MeshType::PROCEDURAL_MESH;

	if (mUseNormalMapping)
		Meshes[MeshCount - 1].mNormalMapSRV = d3dHelper::CreateTexture2DSRVW(md3dDevice, m3dDeviceContext, NormalMapFilenames[0].c_str());
	else
		Meshes[MeshCount - 1].mNormalMapSRV = 0;

	GeometryGenerator GeoGen;
	GeometryGenerator::MeshData Grid;

	GeoGen.CreateGrid(width, depth, m, n, Grid);

	std::vector<MeshVertex>vertices(Grid.Vertices.size());
	std::vector<XMFLOAT3>vertspos(Grid.Vertices.size());

	for (int i = 0; i < Grid.Vertices.size(); ++i)
	{
		vertices[i].Pos = Grid.Vertices[i].Position;
		vertices[i].Normal = Grid.Vertices[i].Normal;
		vertices[i].TexCoord = Grid.Vertices[i].TexC;
		vertices[i].Tangent = Grid.Vertices[i].TangentU;
		vertspos[i] = vertices[i].Pos;
	}

	BoundingOrientedBox::CreateFromPoints(Meshes[MeshCount - 1].OBB, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));
	BoundingBox::CreateFromPoints(Meshes[MeshCount - 1].AABB, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));
	BoundingSphere::CreateFromPoints(Meshes[MeshCount - 1].BS, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));

	Meshes[MeshCount - 1].mIndexCount = Grid.Indices.size();
	Meshes[MeshCount - 1].mVertexCount = Grid.Vertices.size();

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_DYNAMIC;
	vbd.ByteWidth = sizeof(MeshVertex) * Grid.Vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vbd.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &vertices[0];

	HR(md3dDevice->CreateBuffer(&vbd, &vinitData, &Meshes[MeshCount - 1].mVB));

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_DYNAMIC;
	ibd.ByteWidth = sizeof(int) * Grid.Indices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &Grid.Indices[0];

	HR(md3dDevice->CreateBuffer(&ibd, &iinitData, &Meshes[MeshCount - 1].mIB));
}

void MeshManager::CreateCylinder(D3D_PRIMITIVE_TOPOLOGY primitiveTopology, DrawType drawType, XMMATRIX *TexTransform, XMMATRIX *world, ID3DX11EffectTechnique* ObjectTech,
	std::vector<std::wstring>&TexFilenames, std::vector<std::wstring>&NormalMapFilenames, int TexCount, bool UseTextures, bool UseTransparecy, bool UseAdditiveBlend, bool UseCulling, bool mUseNormalMapping, bool UseDisplacementMapping, UINT InstancesCount, ReflectionType reflectionType,
	RefractionType refractionType, Material material, float bottomRadius, float topRadius, float height,
	int sliceCount, int stackCount, bool BuildCaps)
{
	Object *TempObj = new Object[MeshCount];

	for (int i = 0; i < MeshCount; ++i)
		TempObj[i] = Meshes[i];

	delete[]  Meshes;

	Meshes = new Object[MeshCount + 1];

	for (int i = 0; i < MeshCount; ++i)
		Meshes[i] = TempObj[i];

	delete[] TempObj;

	MeshCount++;

	Meshes[MeshCount - 1].mObjectDynamicCubeMapSRV = 0;
	Meshes[MeshCount - 1].mObjectDynamicCubeMapDSV = 0;
	if (reflectionType == ReflectionType::REFLECTION_TYPE_DYNAMIC_CUBE_MAP ||
		reflectionType == ReflectionType::REFLECTION_TYPE_DYNAMIC_CUBE_MAP_AND_SSLR ||
		refractionType == RefractionType::REFRACTION_TYPE_DYNAMIC_CUBE_MAP)
		BuildDynamicCubeMapViews(MeshCount - 1);

	Meshes[MeshCount - 1].mObjectPlanarRefractionMapDSV = 0;
	Meshes[MeshCount - 1].mObjectPlanarRefractionMapRTV = 0;
	Meshes[MeshCount - 1].mObjectPlanarRefractionMapSRV = 0;
	if (refractionType == RefractionType::REFRACTION_TYPE_PLANAR)
		BuildPlanarRefractionMapViews(MeshCount - 1);

	Meshes[MeshCount - 1].drawType = drawType;
	Meshes[MeshCount - 1].primitiveTopology = primitiveTopology;

	XMStoreFloat4x4(&Meshes[MeshCount - 1].mTexTransform[0], TexTransform[0]);
	for (int i = 0; i < InstancesCount; ++i)
		XMStoreFloat4x4(&Meshes[MeshCount - 1].mWorld[i], world[i]);
	XMStoreFloat4x4(&Meshes[MeshCount - 1].mLocal, I);

	Meshes[MeshCount - 1].mTexCount = TexCount;
	Meshes[MeshCount - 1].mUseTextures = UseTextures;
	Meshes[MeshCount - 1].mMaterial = material;
	Meshes[MeshCount - 1].mUseTransparency = UseTransparecy;
	Meshes[MeshCount - 1].mUseAdditiveBlend = UseAdditiveBlend;
	Meshes[MeshCount - 1].mUseCulling = UseCulling;
	Meshes[MeshCount - 1].mReflectionType = reflectionType;
	Meshes[MeshCount - 1].mRefractionType = refractionType;
	Meshes[MeshCount - 1].mObjectDrawTech = ObjectTech;
	Meshes[MeshCount - 1].mUseDisplacementMapping = UseDisplacementMapping;
	Meshes[MeshCount - 1].mUseNormalMapping = mUseNormalMapping;
	Meshes[MeshCount - 1].mInstancesCount = InstancesCount;
	Meshes[MeshCount - 1].mReflectMapSRV = 0;
	Meshes[MeshCount - 1].mMaterialType = MaterialType::MTL_TYPE_DEFAULT;

	if (UseTextures)
		Meshes[MeshCount - 1].mDiffuseMapArraySRV = d3dHelper::CreateTexture2DArraySRVW(md3dDevice, m3dDeviceContext, TexFilenames);

	for (int i = 1; i < TexFilenames.size(); ++i)
	{
		Meshes[MeshCount - 1].mExtDiffuseMapArraySRV[i] = NULL;
	}
	Meshes[MeshCount - 1].mTexArrayType = TexArrayType::EQUAL_FORMAT;

	for (int i = 0; i < TexFilenames.size(); ++i)
	{
		Meshes[MeshCount - 1].mTexType[i] = TextureType::TEX_TYPE_DIFFUSE_MAP;
	}

	Meshes[MeshCount - 1].mMeshType = MeshType::PROCEDURAL_MESH;

	if (mUseNormalMapping)
		Meshes[MeshCount - 1].mNormalMapSRV = d3dHelper::CreateTexture2DSRVW(md3dDevice, m3dDeviceContext, NormalMapFilenames[0].c_str());
	else
		Meshes[MeshCount - 1].mNormalMapSRV = 0;

	GeometryGenerator GeoGen;
	GeometryGenerator::MeshData Cylinder;

	GeoGen.CreateCylinder(bottomRadius, topRadius, height, sliceCount, stackCount, Cylinder, BuildCaps);

	std::vector<MeshVertex>vertices(Cylinder.Vertices.size());
	std::vector<XMFLOAT3>vertspos(Cylinder.Vertices.size());

	for (int i = 0; i < Cylinder.Vertices.size(); ++i)
	{
		vertices[i].Pos = Cylinder.Vertices[i].Position;
		vertices[i].Normal = Cylinder.Vertices[i].Normal;
		vertices[i].TexCoord = Cylinder.Vertices[i].TexC;
		vertices[i].Tangent = Cylinder.Vertices[i].TangentU;
		vertspos[i] = vertices[i].Pos;
	}

	BoundingOrientedBox::CreateFromPoints(Meshes[MeshCount - 1].OBB, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));
	BoundingBox::CreateFromPoints(Meshes[MeshCount - 1].AABB, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));
	BoundingSphere::CreateFromPoints(Meshes[MeshCount - 1].BS, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));

	Meshes[MeshCount - 1].mIndexCount = Cylinder.Indices.size();
	Meshes[MeshCount - 1].mVertexCount = Cylinder.Vertices.size();

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_DYNAMIC;
	vbd.ByteWidth = sizeof(MeshVertex) * Cylinder.Vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vbd.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &vertices[0];

	HR(md3dDevice->CreateBuffer(&vbd, &vinitData, &Meshes[MeshCount - 1].mVB));

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_DYNAMIC;
	ibd.ByteWidth = sizeof(int) * Cylinder.Indices.size();
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &Cylinder.Indices[0];

	HR(md3dDevice->CreateBuffer(&ibd, &iinitData, &Meshes[MeshCount - 1].mIB));
}

void MeshManager::CreateMeshFromFile(D3D_PRIMITIVE_TOPOLOGY primitiveTopology, DrawType drawType, XMMATRIX *world, ID3DX11EffectTechnique* ObjectTech, bool UseTransparecy, bool UseAdditiveBlend, bool UseCulling, UINT InstancesCount,
	ReflectionType reflectionType, RefractionType refractionType, Material material, std::wstring MeshFilename)
{
	Object *TempObj = new Object[MeshCount];

	for (int i = 0; i < MeshCount; ++i)
		TempObj[i] = Meshes[i];

	delete[]  Meshes;

	Meshes = new Object[MeshCount + 1];

	for (int i = 0; i < MeshCount; ++i)
		Meshes[i] = TempObj[i];

	delete[] TempObj;

	MeshCount++;

	Meshes[MeshCount - 1].mObjectDynamicCubeMapSRV = 0;
	Meshes[MeshCount - 1].mObjectDynamicCubeMapDSV = 0;
	if (reflectionType == ReflectionType::REFLECTION_TYPE_DYNAMIC_CUBE_MAP ||
		reflectionType == ReflectionType::REFLECTION_TYPE_DYNAMIC_CUBE_MAP_AND_SSLR ||
		refractionType == RefractionType::REFRACTION_TYPE_DYNAMIC_CUBE_MAP)
		BuildDynamicCubeMapViews(MeshCount - 1);

	Meshes[MeshCount - 1].mObjectPlanarRefractionMapDSV = 0;
	Meshes[MeshCount - 1].mObjectPlanarRefractionMapRTV = 0;
	Meshes[MeshCount - 1].mObjectPlanarRefractionMapSRV = 0;
	if (refractionType == RefractionType::REFRACTION_TYPE_PLANAR)
		BuildPlanarRefractionMapViews(MeshCount - 1);

	Meshes[MeshCount - 1].drawType = drawType;
	Meshes[MeshCount - 1].primitiveTopology = primitiveTopology;

	XMStoreFloat4x4(&Meshes[MeshCount - 1].mTexTransform[0], XMMatrixIdentity());
	for (int i = 0; i < InstancesCount; ++i)
		XMStoreFloat4x4(&Meshes[MeshCount - 1].mWorld[i], world[i]);
	XMStoreFloat4x4(&Meshes[MeshCount - 1].mLocal, I);

	Meshes[MeshCount - 1].mTexCount = 0;
	Meshes[MeshCount - 1].mUseTextures = false;
	Meshes[MeshCount - 1].mMaterial = material;
	Meshes[MeshCount - 1].mUseTransparency = UseTransparecy;
	Meshes[MeshCount - 1].mUseAdditiveBlend = UseAdditiveBlend;
	Meshes[MeshCount - 1].mUseCulling = UseCulling;
	Meshes[MeshCount - 1].mReflectionType = reflectionType;
	Meshes[MeshCount - 1].mRefractionType = refractionType;
	Meshes[MeshCount - 1].mObjectDrawTech = ObjectTech;
	Meshes[MeshCount - 1].mUseDisplacementMapping = false;
	Meshes[MeshCount - 1].mUseNormalMapping = false;
	Meshes[MeshCount - 1].mInstancesCount = InstancesCount;
	Meshes[MeshCount - 1].mReflectMapSRV = 0;
	Meshes[MeshCount - 1].mMaterialType = MaterialType::MTL_TYPE_DEFAULT;

	Meshes[MeshCount - 1].mNormalMapSRV = 0;

	Meshes[MeshCount - 1].mMeshType = MeshType::CUSTOM_FORMAT_MESH;

	FILE *Mesh;

	int totalMeshVertexCount;
	int totalMeshIndexCount;

	wchar_t str[4000];

	_wfopen_s(&Mesh, MeshFilename.c_str(), L"r");

	fgetws(str, 3000, Mesh);
	swscanf_s(str, L"VertexCount: %d", &totalMeshVertexCount);
	fgetws(str, 3000, Mesh);
	swscanf_s(str, L"TriangleCount: %d", &totalMeshIndexCount);
	totalMeshIndexCount *= 3;

	std::vector<MeshVertex> MeshVertices(totalMeshVertexCount);
	std::vector<int> MeshIndices(totalMeshIndexCount);
	std::vector<XMFLOAT3>vertspos(totalMeshVertexCount);

	fgetws(str, 3000, Mesh);
	fgetws(str, 3000, Mesh);
	for (int i = 0; i < totalMeshVertexCount; ++i)
	{
		fgetws(str, 3000, Mesh);
		swscanf_s(str, L"\t%f %f %f %f %f %f", &MeshVertices[i].Pos.x, &MeshVertices[i].Pos.y, &MeshVertices[i].Pos.z,
			&MeshVertices[i].Normal.x, &MeshVertices[i].Normal.y, &MeshVertices[i].Normal.z);
		MeshVertices[i].Tangent = XMFLOAT3(0, 0, 0);
		vertspos[i] = MeshVertices[i].Pos;
	}
	for (int i = 0; i < totalMeshIndexCount; i += 3)
	{
		fgetws(str, 3000, Mesh);
		swscanf_s(str, L"\t%d %d %d", &MeshIndices[i], &MeshIndices[i + 1], &MeshIndices[i + 2]);
	}


	BoundingOrientedBox::CreateFromPoints(Meshes[MeshCount - 1].OBB, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));
	BoundingBox::CreateFromPoints(Meshes[MeshCount - 1].AABB, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));
	BoundingSphere::CreateFromPoints(Meshes[MeshCount - 1].BS, vertspos.size(), &vertspos[0], sizeof(XMFLOAT3));

	Meshes[MeshCount - 1].mIndexCount = totalMeshIndexCount;
	Meshes[MeshCount - 1].mVertexCount = totalMeshVertexCount;

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_DYNAMIC;
	vbd.ByteWidth = sizeof(MeshVertex) * totalMeshVertexCount;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vbd.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &MeshVertices[0];

	HR(md3dDevice->CreateBuffer(&vbd, &vinitData, &Meshes[MeshCount - 1].mVB));

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_DYNAMIC;
	ibd.ByteWidth = sizeof(int) * totalMeshIndexCount;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &MeshIndices[0];

	HR(md3dDevice->CreateBuffer(&ibd, &iinitData, &Meshes[MeshCount - 1].mIB));
}

void MeshManager::CreateBrokenLineFromFile(D3D_PRIMITIVE_TOPOLOGY primitiveTopology, DrawType drawType, XMMATRIX *world, ID3DX11EffectTechnique* ObjectTech, bool UseTransparecy, bool UseAdditiveBlend, bool UseCulling, UINT InstancesCount, Material material)//<--Example constructor function, which reads 2D-signal from file and outputs it. Works with only specific files!
{
	LPWSTR FileName = NULL;

	Object *TempObj = new Object[MeshCount];

	for (int i = 0; i < MeshCount; ++i)
		TempObj[i] = Meshes[i];

	delete[]  Meshes;

	Meshes = new Object[MeshCount + 1];

	for (int i = 0; i < MeshCount; ++i)
		Meshes[i] = TempObj[i];

	delete[] TempObj;

	MeshCount++;

	Meshes[MeshCount - 1].mObjectDynamicCubeMapSRV = 0;
	Meshes[MeshCount - 1].mObjectDynamicCubeMapDSV = 0;

	Meshes[MeshCount - 1].drawType = drawType;
	Meshes[MeshCount - 1].primitiveTopology = primitiveTopology;

	XMStoreFloat4x4(&Meshes[MeshCount - 1].mTexTransform[0], XMMatrixIdentity());
	for (int i = 0; i < InstancesCount; ++i)
		XMStoreFloat4x4(&Meshes[MeshCount - 1].mWorld[i], world[i]);
	XMStoreFloat4x4(&Meshes[MeshCount - 1].mLocal, I);

	Meshes[MeshCount - 1].mTexCount = 0;
	Meshes[MeshCount - 1].mUseTextures = false;
	Meshes[MeshCount - 1].mMaterial = material;
	Meshes[MeshCount - 1].mUseTransparency = UseTransparecy;
	Meshes[MeshCount - 1].mUseAdditiveBlend = UseAdditiveBlend;
	Meshes[MeshCount - 1].mUseCulling = UseCulling;
	Meshes[MeshCount - 1].mReflectionType = ReflectionType::REFLECTION_TYPE_NONE;
	Meshes[MeshCount - 1].mRefractionType = RefractionType::REFRACTION_TYPE_NONE;
	Meshes[MeshCount - 1].mObjectDrawTech = ObjectTech;
	Meshes[MeshCount - 1].mUseDisplacementMapping = false;
	Meshes[MeshCount - 1].mUseNormalMapping = false;
	Meshes[MeshCount - 1].mInstancesCount = InstancesCount;
	Meshes[MeshCount - 1].mReflectMapSRV = 0;
	Meshes[MeshCount - 1].mMaterialType = MaterialType::MTL_TYPE_DEFAULT;

	Meshes[MeshCount - 1].mNormalMapSRV = 0;

	Meshes[MeshCount - 1].mMeshType = MeshType::CUSTOM_FORMAT_MESH;

	int totalMeshVertexCount = 0;

	wchar_t str[4000];

	OPENFILENAMEW ofn;
	wchar_t szFile[260];

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = szFile;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(szFile);
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	FILE *Mesh;

	if (GetOpenFileNameW(&ofn) == TRUE) {
		_wfopen_s(&Mesh, ofn.lpstrFile, L"r");

		XMFLOAT3 Scale(30000000, 10, 1);
		XMFLOAT3 Offset(0, 0, 0);

		std::vector<MeshVertex> MeshVertices;

		for (int i = 0; i < 20; ++i)
			fgetws(str, 300, Mesh);

		float First;

		for (int i = 0; !feof(Mesh); ++i)
		{
			MeshVertex newVertex;
			newVertex.Pos = XMFLOAT3(0, 0, 0);
			newVertex.Normal = XMFLOAT3(0, 0, 0);
			newVertex.TexCoord = XMFLOAT2(0, 0);
			MeshVertices.push_back(newVertex);

			fwscanf_s(Mesh, L"%f,%f", &MeshVertices[i].Pos.x, &MeshVertices[i].Pos.y);
			if (i == 0)
				First = MeshVertices[i].Pos.x;

			MeshVertices[i].Pos.x -= First;

			MeshVertices[i].Pos.z = 0;
			MeshVertices[i].Pos.x *= Scale.x;
			MeshVertices[i].Pos.y *= Scale.y;
			MeshVertices[i].Pos.z *= Scale.z;
			MeshVertices[i].Pos.x += Offset.x;
			MeshVertices[i].Pos.y += Offset.y;
			MeshVertices[i].Pos.z = Offset.z;
			MeshVertices[i].Normal.x = 0;
			MeshVertices[i].Normal.y = 0;
			MeshVertices[i].Normal.z = 1;
			MeshVertices[i].Tangent = XMFLOAT3(0, 0, 0);

			totalMeshVertexCount++;
		}

		MeshVertices.pop_back();
		totalMeshVertexCount--;
		Meshes[MeshCount - 1].mVertexCount = totalMeshVertexCount;

		D3D11_BUFFER_DESC vbd;
		vbd.Usage = D3D11_USAGE_DYNAMIC;
		vbd.ByteWidth = sizeof(MeshVertex)* totalMeshVertexCount;
		vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		vbd.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA vinitData;
		vinitData.pSysMem = &MeshVertices[0];

		HR(md3dDevice->CreateBuffer(&vbd, &vinitData, &Meshes[MeshCount - 1].mVB));

		fclose(Mesh);
	}
	else
	{
		MessageBoxW(mhMainWnd, L"GetOpenFileName failed!", L" ", MB_OK);
		ExitProcess(0);
	}
}

//////////////////////Primitive-managing functions/////////////////////////////////////////////////

void MeshManager::GetMeshVertexBuffer(int MeshIndex, MeshVertex *DestBuffer) const {
	D3D11_MAPPED_SUBRESOURCE mappedData;
	m3dDeviceContext->Map(Meshes[MeshIndex].mVB, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mappedData);

	MeshVertex *v = reinterpret_cast<MeshVertex*>(mappedData.pData);

	for (int i = 0; i < Meshes[MeshIndex].mVertexCount; ++i)
	{
		DestBuffer[i] = v[i];
	}

	m3dDeviceContext->Unmap(Meshes[MeshIndex].mVB, 0);
}

void MeshManager::SetMeshVertexBuffer(int MeshIndex, MeshVertex *FromBuffer) {
	D3D11_MAPPED_SUBRESOURCE mappedData;
	m3dDeviceContext->Map(Meshes[MeshIndex].mVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData);

	MeshVertex *v = reinterpret_cast<MeshVertex*>(mappedData.pData);

	for (int i = 0; i < Meshes[MeshIndex].mVertexCount; ++i)
		v[i] = FromBuffer[i];

	m3dDeviceContext->Unmap(Meshes[MeshIndex].mVB, 0);
}

void MeshManager::GetMeshIndexBuffer(int MeshIndex, int *DestBuffer) const {
	D3D11_MAPPED_SUBRESOURCE mappedData;
	m3dDeviceContext->Map(Meshes[MeshIndex].mIB, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mappedData);

	int *inds = reinterpret_cast<int*>(mappedData.pData);

	for (int i = 0; i < Meshes[MeshIndex].mIndexCount; ++i)
	{
		DestBuffer[i] = inds[i];
	}

	m3dDeviceContext->Unmap(Meshes[MeshIndex].mIB, 0);
}

void MeshManager::SetMeshIndexBuffer(int MeshIndex, int *FromBuffer) {
	D3D11_MAPPED_SUBRESOURCE mappedData;
	m3dDeviceContext->Map(Meshes[MeshIndex].mIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData);

	int *inds = reinterpret_cast<int*>(mappedData.pData);

	for (int i = 0; i < Meshes[MeshIndex].mIndexCount; ++i)
		inds[i] = FromBuffer[i];

	m3dDeviceContext->Unmap(Meshes[MeshIndex].mIB, 0);
}

void MeshManager::SetMeshWorldMatrixXM(int MeshIndex, int InstanceIndex, XMMATRIX *world) {
	XMStoreFloat4x4(&(Meshes[MeshIndex].mWorld[InstanceIndex]), *world);
}
XMMATRIX MeshManager::GetMeshWorldMatrixXM(int MeshIndex, int InstanceIndex) const {
	return XMLoadFloat4x4(&(Meshes[MeshIndex].mWorld[InstanceIndex]));
}

void MeshManager::SetMeshLocalMatrixXM(int MeshIndex, XMMATRIX *loc) {
	XMStoreFloat4x4(&(Meshes[MeshIndex].mLocal), *loc);
}
XMMATRIX MeshManager::GetMeshLocalMatrixXM(int MeshIndex) const {
	return XMLoadFloat4x4(&(Meshes[MeshIndex].mLocal));
}