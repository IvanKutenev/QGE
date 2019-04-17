#include "FBXLoader.h"

int SortCmp(const void* v1, const void* v2)
{
	MeshVertex* pv1 = (MeshVertex*)v1;
	MeshVertex* pv2 = (MeshVertex*)v2;
	if (pv1->Pos.x > pv2->Pos.x)
		return 1;
	else if (pv1->Pos.x < pv2->Pos.x)
		return -1;
	else
		return 0;
}

bool CmpVertices(MeshVertex* v1, MeshVertex* v2)
{
	return ((v1->Pos.x == v2->Pos.x &&v1->Pos.y == v2->Pos.y &&v1->Pos.z == v2->Pos.z)
		&&
		(v1->Normal.x == v2->Normal.x &&v1->Normal.y == v2->Normal.y&&v1->Normal.z == v2->Normal.z));
}


FBXLoader::FBXLoader(const char* Filename, const char* SceneName, BuffersOptimization bo) :mSdkManager(0), mIOSettings(0),
mScene(0), mRootNode(0), mNodeList(0)
{
	mBuffersOptimizization = bo;

	FbxImporter* mImporter = 0;

	mSdkManager = FbxManager::Create();

	mIOSettings = FbxIOSettings::Create(mSdkManager, IOSROOT);
	mSdkManager->SetIOSettings(mIOSettings);

	mImporter = FbxImporter::Create(mSdkManager, "");

	if (!mImporter->Initialize(Filename, -1, mSdkManager->GetIOSettings()))
	{
		std::string error = "Call to FbxImporter::Initialize() failed.\n";
		error += "Error returned: ";
		error += mImporter->GetStatus().GetErrorString();
		MessageBoxA(NULL, error.c_str(), NULL, MB_OK);
		error.clear();
		exit(-1);
	}

	mScene = FbxScene::Create(mSdkManager, SceneName);

	mImporter->Import(mScene);

	mImporter->Destroy();

	mRootNode = mScene->GetRootNode();
	
	Init();
}

FBXLoader::~FBXLoader()
{
	if (mRootNode != NULL)
		mRootNode->Destroy();
	if (mScene != NULL)
		mScene->Destroy();
	if (mIOSettings != NULL)
		mIOSettings->Destroy();
	if (mSdkManager != NULL)
		mSdkManager->Destroy();
	if (mNodeList.size() > 0)
		mNodeList.clear();
}


void FBXLoader::Init()
{
	GetNodesListByRootNode(mRootNode);
	GetMeshesListByRootNode(mRootNode);
	GetNodesBuffers();
}

void FBXLoader::GetNodesListByRootNode(FbxNode* pRootNode)
{
	if (pRootNode != NULL) {
		for (int j = 0; j < (int)pRootNode->GetChildCount(); ++j) {
			FbxAMatrix I;
			I.SetIdentity();
			Node newnode = { pRootNode->GetChild(j), I, std::vector<Mesh>(0) };
			mNodeList.push_back(newnode);
			GetNodesListByRootNode(pRootNode->GetChild(j));
		}
	}
}

void FBXLoader::GetMeshesListByRootNode(FbxNode* pRootNode)
{
	for (int i = 0; i < mNodeList.size(); ++i)
	{
		if ((mNodeList[i].mNode->GetNodeAttribute()) != NULL)
		{
			if (mNodeList[i].mNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh)
			{
				FbxMesh* nodeMesh = (FbxMesh*)mNodeList[i].mNode->GetNodeAttribute();
				FbxGeometryConverter mConverter(mSdkManager);
				nodeMesh = (FbxMesh*)mConverter.Triangulate(nodeMesh, true, false);
				if(!mConverter.SplitMeshPerMaterial(nodeMesh, true))
					exit(-1);
				else
				{
					for (int j = 0; j < mNodeList[i].mNode->GetNodeAttributeCount(); ++j)
					{
						Mesh msh;
						msh.mIndicies = std::vector<int>(0);
						msh.mVertices = std::vector<MeshVertex>(0);
						msh.mMeshMtl = Materials::Plastic_Cellulose;
						msh.mMesh = (FbxMesh*)mNodeList[i].mNode->GetNodeAttributeByIndex(j);
						msh.mTextures = std::vector<TexInfo>(0);
						mNodeList[i].mMeshes.push_back(msh);
						mNodeList[i].mMeshes[j].mMesh->GenerateNormals(RewriteNormals.first, RewriteNormals.second);
					}
				}
				
				mNodeList[i].mGlobalTransform = GetNodeGlobalTransform(mNodeList[i].mNode);
			}
		}
	}
}

void FBXLoader::GetNodesBuffers()
{
	for (int i = 0; i < mNodeList.size(); ++i) {
		GetNodeVertices(&mNodeList[i]);
		GetNodeIndicies(&mNodeList[i]);
		GetNodeTextures(&mNodeList[i]);
		GetNodeMaterials(&mNodeList[i]);
	}
}

std::vector<std::pair<std::pair<std::wstring, std::wstring>, FbxAMatrix>> FBXLoader::GetTextureFilename(FbxProperty Property, int &TexIndex) //<Texture filename, UVSetName>
{
	int layeredTextureCount;

	std::wstring RootPath = L"Textures\\";

	std::vector<std::pair<std::pair<std::wstring, std::wstring>, FbxAMatrix>>Texs;

	layeredTextureCount = Property.GetSrcObjectCount<FbxLayeredTexture>();

	if (layeredTextureCount > 0)
	{
		for (int k = 0; k < layeredTextureCount; ++k)
		{
			FbxLayeredTexture* layeredTex = FbxCast<FbxLayeredTexture>(Property.GetSrcObject<FbxLayeredTexture>(k));
			int lCount = layeredTex->GetSrcObjectCount<FbxTexture>();

			for (int c = 0; c < lCount; ++c)
			{
				FbxTexture* Tex = FbxCast<FbxTexture>(layeredTex->GetSrcObject<FbxTexture>(k));
				FbxString uvSetNm = Tex->UVSet;
				std::string UVSetName = uvSetNm.Buffer();

				if (UVSetName == "default")
				{
					char str[100];
					sprintf_s(str, "%d", TexIndex);
					UVSetName = str;
				}

				FLOAT rotU = Tex->GetRotationU();
				FLOAT rotV = Tex->GetRotationV();
				FLOAT rotW = Tex->GetRotationW();

				FLOAT transU = Tex->GetTranslationU();
				FLOAT transV = Tex->GetTranslationV();

				FLOAT scaleU = Tex->GetScaleU();
				FLOAT scaleV = Tex->GetScaleV();

				FbxVector2 Pivot = FbxVector2(-0.5f, -0.5f);

				FbxAMatrix TexTransform;
				TexTransform.SetIdentity();

				TexTransform.MultT(FbxVector4(Pivot.mData[0], Pivot.mData[1], 0.0f, 1.0f));
				TexTransform.MultS(FbxVector4(scaleU, scaleV, 0.0f, 1.0f));
				TexTransform.MultR(FbxVector4(XMConvertToRadians(rotU), XMConvertToRadians(rotV), XMConvertToRadians(rotW), 1.0f));
				TexTransform.MultT(FbxVector4(-Pivot.mData[0], -Pivot.mData[1], 0.0, 1.0f));
				TexTransform.MultT(FbxVector4(transU, transV, 0.0, 1.0f));

				wchar_t* fname = { L"\0" };
				std::string tempStr = FbxCast<FbxFileTexture>(Tex)->GetFileName();
				if (tempStr == "")
					continue;
				FbxAnsiToWC(tempStr.c_str(), fname, nullptr);
				wchar_t* uvsname = { L"\0" };
				FbxAnsiToWC(UVSetName.c_str(), uvsname, nullptr);
				std::pair<std::pair<wchar_t*, wchar_t*>, FbxAMatrix>p; 
				p.first.first = fname;
				p.first.second = uvsname;
				p.second = TexTransform;
				Texs.push_back(p);

				TexIndex++;
			}
		}
	}
	else
	{
		int TextureCount = Property.GetSrcObjectCount<FbxTexture>();
		for (int k = 0; k < TextureCount; ++k)
		{
			FbxTexture* Tex = FbxCast<FbxTexture>(Property.GetSrcObject<FbxTexture>(k));
			FbxString uvSetNm = Tex->UVSet;
			std::string UVSetName = uvSetNm.Buffer();

			if (UVSetName == "default")
			{
				char str[100];
				sprintf_s(str, "%d", TexIndex);
				UVSetName = str;
			}

			FLOAT rotU = Tex->GetRotationU();
			FLOAT rotV = Tex->GetRotationV();
			FLOAT rotW = Tex->GetRotationW();

			FLOAT transU = Tex->GetTranslationU();
			FLOAT transV = Tex->GetTranslationV();

			FLOAT scaleU = Tex->GetScaleU();
			FLOAT scaleV = Tex->GetScaleV();

			FbxVector2 Pivot = FbxVector2(-0.5f, -0.5f);

			FbxAMatrix TexTransform;
			TexTransform.SetIdentity();

			TexTransform.MultT(FbxVector4(Pivot.mData[0], Pivot.mData[1], 0.0f, 1.0f));
			TexTransform.MultS(FbxVector4(scaleU, scaleV, 0.0f, 1.0f));
			TexTransform.MultR(FbxVector4(XMConvertToRadians(rotU), XMConvertToRadians(rotV), XMConvertToRadians(rotW), 1.0f));
			TexTransform.MultT(FbxVector4(-Pivot.mData[0], -Pivot.mData[1], 0.0, 1.0f));
			TexTransform.MultT(FbxVector4(transU, transV, 0.0, 1.0f));

			wchar_t* fname = { L"\0" };
			std::string tempStr = FbxCast<FbxFileTexture>(Tex)->GetFileName();
			if (tempStr == "")
				continue;
			FbxAnsiToWC(tempStr.c_str(), fname, nullptr);
			wchar_t* uvsname = { L"\0" };
			FbxAnsiToWC(UVSetName.c_str(), uvsname, nullptr);
			std::pair<std::pair<wchar_t*, wchar_t*>, FbxAMatrix>p; 
			p.first.first = fname;
			p.first.second = uvsname;
			p.second = TexTransform;
			Texs.push_back(p);

			TexIndex++;
		}
	}

	for (int i = 0; i < Texs.size(); ++i)
	{
		std::wstring temp = L"";
		if (Texs[i].first.first.size() > 0) {
			for (int k = Texs[i].first.first.size() - 1; Texs[i].first.first[k] != L'/' && Texs[i].first.first[k] != L'\\' && k >= 0; --k)
			{
				temp = Texs[i].first.first[k] + temp;
			}
			Texs[i].first.first = RootPath + temp;
		}
	}
	return Texs;
}

void FBXLoader::GetNodeTextures(Node* pNode)
{
	int TexIndex = 0;
	std::multimap<TextureType, TexInfo>Textures;

	//Load textures
	if (pNode->mMeshes.size() > 0 && pNode->mMeshes[0].mMesh)
	{
		int materialCount = pNode->mNode->GetSrcObjectCount<FbxSurfaceMaterial>();

		for (int index = 0; index < materialCount; ++index)
		{
			FbxSurfaceMaterial* pMaterial = (FbxSurfaceMaterial*)pNode->mNode->GetSrcObject<FbxSurfaceMaterial>(index);
			if (pMaterial != NULL)
			{
				FbxProperty Property;
				std::vector<std::pair<std::pair<std::wstring, std::wstring>, FbxAMatrix>>Filenames;
				for (int i = 0; i < pNode->mMeshes.size(); ++i) {
					if(pMaterial->IsConnectedDstObject(pNode->mMeshes[i].mMesh))
						MessageBox(NULL, NULL, NULL, MB_OK);
				}
				//Diffuse Textures
				Property = pMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse);
				Filenames = GetTextureFilename(Property, TexIndex);
				for (int i = 0; i < Filenames.size(); ++i) {
					TexInfo ti;
					ti.TexFilename = Filenames[i].first.first;
					ti.TexUVChannelName = Filenames[i].first.second;
					ti.TexTypeName = L"sDiffuse";
					ti.TexTransform = Filenames[i].second;
					ti.TexType = TextureType::TEX_TYPE_DIFFUSE_MAP;
					Textures.insert(std::pair<TextureType, TexInfo>(ti.TexType, ti));
				}

				//Specular power textures
				Property = pMaterial->FindProperty(FbxSurfaceMaterial::sShininess);
				Filenames = GetTextureFilename(Property, TexIndex);
				for (int i = 0; i < Filenames.size(); ++i) {
					TexInfo ti;
					ti.TexFilename = Filenames[i].first.first;
					ti.TexUVChannelName = Filenames[i].first.second;
					ti.TexTypeName = L"sShininess";
					ti.TexTransform = Filenames[i].second;
					ti.TexType = TextureType::TEX_TYPE_SHININESS_MAP;
					Textures.insert(std::pair<TextureType, TexInfo>(ti.TexType, ti));
				}

				Property = pMaterial->FindProperty(FbxSurfaceMaterial::sSpecularFactor);
				Filenames = GetTextureFilename(Property, TexIndex);
				for (int i = 0; i < Filenames.size(); ++i) {
					TexInfo ti;
					ti.TexFilename = Filenames[i].first.first;
					ti.TexUVChannelName = Filenames[i].first.second;
					ti.TexTypeName = L"sSpecularFactor";
					ti.TexTransform = Filenames[i].second;
					ti.TexType = TextureType::TEX_TYPE_SHININESS_MAP;
					Textures.insert(std::pair<TextureType, TexInfo>(ti.TexType, ti));
				}

				//Normal Map Textures
				Property = pMaterial->FindProperty(FbxSurfaceMaterial::sNormalMap);
				Filenames = GetTextureFilename(Property, TexIndex);
				for (int i = 0; i < Filenames.size(); ++i) {
					TexInfo ti;
					ti.TexFilename = Filenames[i].first.first;
					ti.TexUVChannelName = Filenames[i].first.second;
					ti.TexTypeName = L"sNormalMap";
					ti.TexTransform = Filenames[i].second;
					ti.TexType = TextureType::TEX_TYPE_NORMAL_MAP;
					Textures.insert(std::pair<TextureType, TexInfo>(ti.TexType, ti));
				}

				//Normal Map Textures
				Property = pMaterial->FindProperty(FbxSurfaceMaterial::sBump);
				Filenames = GetTextureFilename(Property, TexIndex);
				for (int i = 0; i < Filenames.size(); ++i) {
					TexInfo ti;
					ti.TexFilename = Filenames[i].first.first;
					ti.TexUVChannelName = Filenames[i].first.second;
					ti.TexTypeName = L"sBump";
					ti.TexTransform = Filenames[i].second;
					ti.TexType = TextureType::TEX_TYPE_NORMAL_MAP;
					Textures.insert(std::pair<TextureType, TexInfo>(ti.TexType, ti));
				}

				//Transparent Textures
				Property = pMaterial->FindProperty(FbxSurfaceMaterial::sTransparentColor);
				Filenames = GetTextureFilename(Property, TexIndex);
				for (int i = 0; i < Filenames.size(); ++i) {
					TexInfo ti;
					ti.TexFilename = Filenames[i].first.first;
					ti.TexUVChannelName = Filenames[i].first.second;
					ti.TexTypeName = L"sTransparentColor";
					ti.TexTransform = Filenames[i].second;
					ti.TexType = TextureType::TEX_TYPE_TRANSPARENT_MAP;
					Textures.insert(std::pair<TextureType, TexInfo>(ti.TexType, ti));
				}
			}
		}
		std::pair<std::multimap<TextureType, TexInfo>::iterator, std::multimap<TextureType, TexInfo>::iterator>Index;
		for (int i = 0; i < pNode->mMeshes.size(); ++i)
		{
			for (TextureType tt = TEX_TYPE_DIFFUSE_MAP; tt < TEX_TYPE_UNKNOWN; tt = (TextureType)(tt + 1))
			{
				Index = Textures.equal_range(tt);
				if (Textures.count(tt) == pNode->mMeshes.size() && pNode->mMeshes.size() > 1)
				{
					for (int l = 1; l <= i; ++l)
						Index.first++;
					pNode->mMeshes[i].mTextures.push_back(Index.first->second);
				}
				else
				{
					for (int j = 0; j < Textures.count(tt); ++j)
						pNode->mMeshes[i].mTextures.push_back((Index.first++)->second);
				}
			}
		}
	}
}

void FBXLoader::GetNodeVertices(Node* pNode)
{
	for (int i = 0; i < pNode->mMeshes.size(); ++i)
		GetMeshVertices(pNode, i);
}

void FBXLoader::GetNodeMaterials(Node* pNode)
{
	for (int i = 0; i < pNode->mMeshes.size(); ++i)
		pNode->mMeshes[i].mMeshMtl = DefaultMaterial;

	for (int j = 0; j < pNode->mMeshes.size(); ++j)
	{
		int MtlCnt = pNode->mMeshes[j].mMesh->GetElementMaterialCount();
		for (int i = 0; i < MtlCnt; ++i)
		{
			const FbxGeometryElementMaterial* MtlElement = pNode->mMeshes[j].mMesh->GetElementMaterial(i);
			if (!MtlElement)
				continue;

			if (!MtlElement->GetMappingMode() == FbxGeometryElement::eAllSame &&
				!MtlElement->GetMappingMode() == FbxGeometryElement::eByPolygon)
				continue;

			if (MtlElement->GetMappingMode() == FbxGeometryElement::eAllSame)
			{
				FbxSurfaceMaterial* SurfaceMtl = pNode->mNode->GetMaterial(MtlElement->GetIndexArray().GetAt(0));
				FbxSurfacePhong* PhongMtl = FbxCast<FbxSurfacePhong>(SurfaceMtl);
				if (!PhongMtl)
				{
					continue;
				}
				else
				{
					FbxDouble3 Ambient = PhongMtl->Ambient.Get();
					FbxDouble3 Diffuse = PhongMtl->Diffuse.Get();
					FbxDouble3 Specular = PhongMtl->Specular.Get();
					FbxDouble SpecPower = PhongMtl->Shininess.Get();
					pNode->mMeshes[j].mMeshMtl.Ambient = XMFLOAT4(Ambient.mData[0], Ambient.mData[1], Ambient.mData[2], 1.0f);
					pNode->mMeshes[j].mMeshMtl.Diffuse = XMFLOAT4(Diffuse.mData[0], Diffuse.mData[1], Diffuse.mData[2], 1.0f);
					pNode->mMeshes[j].mMeshMtl.Specular = XMFLOAT4(Specular.mData[0], Specular.mData[1], Specular.mData[2], 1.0f);
					pNode->mMeshes[j].mMeshMtl.Reflect = XMFLOAT3(1.0f, sqrt(2.0f / (2.0f + SpecPower)), -1.0f);
				}
			}
			else if (MtlElement->GetMappingMode() == FbxGeometryElement::eByPolygon)
			{
				for (int p = 0; p < pNode->mMeshes[j].mMesh->GetPolygonCount(); ++p)
				{
					FbxSurfaceMaterial* SurfaceMtl = pNode->mNode->GetMaterial(MtlElement->GetIndexArray().GetAt(p));
					FbxSurfacePhong* PhongMtl = FbxCast<FbxSurfacePhong>(SurfaceMtl);
					if (!PhongMtl)
					{
						continue;
					}
					else
					{
						FbxDouble3 Ambient = PhongMtl->Ambient.Get();
						FbxDouble3 Diffuse = PhongMtl->Diffuse.Get();
						FbxDouble3 Specular = PhongMtl->Specular.Get();
						FbxDouble SpecPower = PhongMtl->Shininess.Get();
						pNode->mMeshes[j].mMeshMtl.Ambient = XMFLOAT4(Ambient.mData[0], Ambient.mData[1], Ambient.mData[2], 1.0f);
						pNode->mMeshes[j].mMeshMtl.Diffuse = XMFLOAT4(Diffuse.mData[0], Diffuse.mData[1], Diffuse.mData[2], 1.0f);
						pNode->mMeshes[j].mMeshMtl.Specular = XMFLOAT4(Specular.mData[0], Specular.mData[1], Specular.mData[2], 1.0f);
						pNode->mMeshes[j].mMeshMtl.Reflect = XMFLOAT3(1.0f, sqrt(2.0f / (2.0f + SpecPower)), -1.0f);
					}
				}
			}
		}
	}
}

void FBXLoader::GetMeshVertices(Node* pNode, int MeshIndex)
{
	if (pNode->mMeshes.size() > 0 && pNode->mMeshes[MeshIndex].mMesh)
	{

		FbxStringList UVSetNameList;
		pNode->mMeshes[MeshIndex].mMesh->GetUVSetNames(UVSetNameList);
		std::vector<FbxArray<FbxVector2>>UVs(pNode->mMeshes[MeshIndex].mMesh->GetPolygonCount()*pNode->mMeshes[MeshIndex].mMesh->GetPolygonSize(0));

		for (int UVSetIndex = 0; UVSetIndex < UVSetNameList.GetCount(); UVSetIndex++)
		{
			const char* UVSetName = UVSetNameList.GetStringAt(UVSetIndex);
			const FbxGeometryElementUV* UVElement = pNode->mMeshes[MeshIndex].mMesh->GetElementUV(UVSetName);

			if (!UVElement)
				continue;

			if (UVElement->GetMappingMode() != FbxGeometryElement::eByPolygonVertex &&
				UVElement->GetMappingMode() != FbxGeometryElement::eByControlPoint)
				continue;

			const bool UseIndex = UVElement->GetReferenceMode() != FbxGeometryElement::eDirect;
			const int IndexCount = UseIndex ? UVElement->GetIndexArray().GetCount() : 0;

			if (UVElement->GetMappingMode() == FbxGeometryElement::eByControlPoint)
			{
				for (int PolyIndex = 0; PolyIndex < pNode->mMeshes[MeshIndex].mMesh->GetPolygonCount(); ++PolyIndex)
				{
					for (int VertIndex = 0; VertIndex < pNode->mMeshes[MeshIndex].mMesh->GetPolygonSize(PolyIndex); ++VertIndex)
					{
						int PolyVertIndex = pNode->mMeshes[MeshIndex].mMesh->GetPolygonVertex(PolyIndex, VertIndex);
						int UVIndex = UseIndex ? UVElement->GetIndexArray().GetAt(PolyVertIndex) : PolyVertIndex;
						UVs[PolyIndex*pNode->mMeshes[MeshIndex].mMesh->GetPolygonSize(PolyIndex) + VertIndex].Add(UVElement->GetDirectArray().GetAt(UVIndex));
					}
				}
			}
			else if (UVElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
			{
				int PolyIndexCounter = 0;
				for (int PolyIndex = 0; PolyIndex < pNode->mMeshes[MeshIndex].mMesh->GetPolygonCount(); ++PolyIndex)
				{
					for (int VertIndex = 0; VertIndex < pNode->mMeshes[MeshIndex].mMesh->GetPolygonSize(PolyIndex); ++VertIndex)
					{
						if (PolyIndexCounter < IndexCount)
						{
							int UVIndex = UseIndex ? UVElement->GetIndexArray().GetAt(PolyIndexCounter) : PolyIndexCounter;
							UVs[PolyIndex*pNode->mMeshes[MeshIndex].mMesh->GetPolygonSize(PolyIndex) + VertIndex].Add(UVElement->GetDirectArray().GetAt(UVIndex));
							PolyIndexCounter++;
						}
					}
				}
			}
		}

		FbxVector4* mControlPoints = pNode->mMeshes[MeshIndex].mMesh->GetControlPoints();
		for (int j = 0; j < (int)pNode->mMeshes[MeshIndex].mMesh->GetPolygonCount(); ++j)
		{
			for (int k = 0; k < (int)pNode->mMeshes[MeshIndex].mMesh->GetPolygonSize(j); ++k)
			{
				FbxVector4 normal;
				pNode->mMeshes[MeshIndex].mMesh->GetPolygonVertexNormal(j, k, normal);
				MeshVertex newvert = { XMFLOAT3(
					static_cast<float>(mControlPoints[pNode->mMeshes[MeshIndex].mMesh->GetPolygonVertex(j, k)].mData[0]),
					static_cast<float>(mControlPoints[pNode->mMeshes[MeshIndex].mMesh->GetPolygonVertex(j, k)].mData[1]),
					static_cast<float>(-mControlPoints[pNode->mMeshes[MeshIndex].mMesh->GetPolygonVertex(j, k)].mData[2])
					),
					XMFLOAT3(
						static_cast<float>(normal.mData[0]),
						static_cast<float>(normal.mData[1]),
						static_cast<float>(-normal.mData[2])
						),
					XMFLOAT2(
						static_cast<float>(UVs[j*pNode->mMeshes[MeshIndex].mMesh->GetPolygonSize(j) + k].GetCount()<1 ? NON_MAPPED_TEXC : UVs[j*pNode->mMeshes[MeshIndex].mMesh->GetPolygonSize(j) + k][0].mData[0]),
						static_cast<float>(UVs[j*pNode->mMeshes[MeshIndex].mMesh->GetPolygonSize(j) + k].GetCount()<1 ? NON_MAPPED_TEXC : 1 - UVs[j*pNode->mMeshes[MeshIndex].mMesh->GetPolygonSize(j) + k][0].mData[1])
						),
					XMFLOAT3(0.0f, 0.0f, 0.0f)
				};
				pNode->mMeshes[MeshIndex].mVertices.push_back(newvert);
				switch (mBuffersOptimizization)
				{
					case BUFFERS_OPTIMIZATION_DISABLE:
						pNode->mMeshes[MeshIndex].mIndicies.push_back(j * 3 + k);
						break;
					case BUFFERS_OPTIMIZATION_ENABLE:
						break;
					default:
						break;
				}
			}
			MeshVertex TempVert = pNode->mMeshes[MeshIndex].mVertices[pNode->mMeshes[MeshIndex].mVertices.size() - 1];
			pNode->mMeshes[MeshIndex].mVertices[pNode->mMeshes[MeshIndex].mVertices.size() - 1] = pNode->mMeshes[MeshIndex].mVertices[pNode->mMeshes[MeshIndex].mVertices.size() - 2];
			pNode->mMeshes[MeshIndex].mVertices[pNode->mMeshes[MeshIndex].mVertices.size() - 2] = TempVert;
		}
		switch (mBuffersOptimizization)
		{
			case BUFFERS_OPTIMIZATION_DISABLE:
				break;
			case BUFFERS_OPTIMIZATION_ENABLE:
				qsort(&pNode->mMeshes[MeshIndex].mVertices[0], pNode->mMeshes[MeshIndex].mVertices.size(), sizeof(MeshVertex), SortCmp);
				for (int i = 0; i + 1 < pNode->mMeshes[MeshIndex].mVertices.size(); ++i)
				{
					if (CmpVertices(&pNode->mMeshes[MeshIndex].mVertices[i], &pNode->mMeshes[MeshIndex].mVertices[i + 1]))
					{
						pNode->mMeshes[MeshIndex].mVertices.erase(pNode->mMeshes[MeshIndex].mVertices.begin() + i + 1);
						i--;
					}
				}
				break;
			default:
				break;
		}
	}
}

void FBXLoader::GetNodeIndicies(Node* pNode)
{
	for (int i = 0; i < pNode->mMeshes.size(); ++i)
		GetMeshIndicies(pNode, i);
}

void FBXLoader::GetMeshIndicies(Node* pNode, int MeshIndex)
{
	switch (mBuffersOptimizization)
	{
		case BUFFERS_OPTIMIZATION_DISABLE:
			break;
		case BUFFERS_OPTIMIZATION_ENABLE:
			if (pNode->mMeshes.size() > 0 && pNode->mMeshes[MeshIndex].mMesh)
			{
				std::vector<MeshVertex>verts(0);
				FbxVector4* mControlPoints = pNode->mMeshes[MeshIndex].mMesh->GetControlPoints();
				for (int j = 0; j < (int)pNode->mMeshes[MeshIndex].mMesh->GetPolygonCount(); ++j)
				{
					for (int k = 0; k < (int)pNode->mMeshes[MeshIndex].mMesh->GetPolygonSize(j); ++k)
					{
						pNode->mMeshes[MeshIndex].mIndicies.push_back(0);
						FbxVector4 normal;
						pNode->mMeshes[MeshIndex].mMesh->GetPolygonVertexNormal(j, k, normal);
						MeshVertex newvert = { XMFLOAT3(
							static_cast<float>(mControlPoints[pNode->mMeshes[MeshIndex].mMesh->GetPolygonVertex(j, k)].mData[0]),
							static_cast<float>(mControlPoints[pNode->mMeshes[MeshIndex].mMesh->GetPolygonVertex(j, k)].mData[1]),
							static_cast<float>(-mControlPoints[pNode->mMeshes[MeshIndex].mMesh->GetPolygonVertex(j, k)].mData[2])
							),
							XMFLOAT3(
								static_cast<float>(normal.mData[0]),
								static_cast<float>(normal.mData[1]),
								static_cast<float>(-normal.mData[2])
								),
							XMFLOAT2(0.0f, 0.0f),
							XMFLOAT3(0.0f, 0.0f, 0.0f)
						};
						verts.push_back(newvert);
					}
					MeshVertex TempVert = verts[verts.size() - 1];
					verts[verts.size() - 1] = verts[verts.size() - 2];
					verts[verts.size() - 2] = TempVert;
				}
				for (int i = 0; i < verts.size(); ++i)
				{
					for (int j = 0; j < pNode->mMeshes[MeshIndex].mVertices.size(); ++j)
					{
						if (CmpVertices(&verts[i], &pNode->mMeshes[MeshIndex].mVertices[j]))
						{
							pNode->mMeshes[MeshIndex].mIndicies[i] = j;
						}
					}
				}
				if (verts.size() > 0)
					verts.clear();
			}
			break;
		default:
			break;
	}
}

FbxAMatrix FBXLoader::GetNodeGlobalTransform(FbxNode* pNode)
{
	FbxAMatrix mTranlationM, mScalingM, mScalingPivotM, mScalingOffsetM, mRotationOffsetM,
		mRotationPivotM, mPreRotationM, mRotationM, mPostRotationM, mTransform;

	FbxAMatrix mParentTransform, mGlobalTranslationM, mGlobalRotationM;

	if (!pNode)
	{
		mTransform.SetIdentity();
		return mTransform;
	}

	FbxVector4 lTranslation = pNode->LclTranslation.Get();
	lTranslation[2] = -lTranslation[2];
	mTranlationM.SetT(lTranslation);

	FbxVector4 mRotationV = pNode->LclRotation.Get();
	mRotationV[0] = -mRotationV[0];
	mRotationV[1] = -mRotationV[1];
	FbxVector4 mPreRotationV = pNode->PreRotation.Get();
	FbxVector4 mPostRotationV = pNode->PostRotation.Get();
	mRotationM.SetR(mRotationV);
	mPreRotationM.SetR(mPreRotationV);
	mPostRotationM.SetR(mPostRotationV);

	FbxVector4 mScalingV = pNode->LclScaling.Get();
	mScalingM.SetS(mScalingV);

	FbxVector4 mScalingOffsetV = pNode->ScalingOffset.Get();
	FbxVector4 mScalingPivotV = pNode->ScalingPivot.Get();
	FbxVector4 mRotationOffsetV = pNode->RotationOffset.Get();
	FbxVector4 mRotationPivotV = pNode->RotationPivot.Get();
	mScalingOffsetM.SetT(mScalingOffsetV);
	mScalingPivotM.SetT(mScalingPivotV);
	mRotationOffsetM.SetT(mRotationOffsetV);
	mRotationPivotM.SetT(mRotationPivotV);

	FbxNode* pParentNode = pNode->GetParent();
	if (pParentNode)
	{
		mParentTransform = GetNodeGlobalTransform(pParentNode);
	}
	else
	{
		mParentTransform.SetIdentity();
	}

	FbxAMatrix mLocalRotationM, mParentGlobalRotationM;
	FbxVector4 mParentGlobalRotationV = mParentTransform.GetR();
	mParentGlobalRotationM.SetR(mParentGlobalRotationV);
	mLocalRotationM = mPreRotationM * mRotationM * mPostRotationM;

	FbxAMatrix mLocalScalingM, mParentGlobalScalingM, mParentGlobalRotationScalingM, mParentTranslationM;
	FbxVector4 lParentGT = mParentTransform.GetT();
	mParentTranslationM.SetT(lParentGT);
	mParentGlobalRotationScalingM = mParentTranslationM.Inverse() * mParentTransform;
	mParentGlobalScalingM = mParentGlobalRotationM.Inverse() * mParentGlobalRotationScalingM;
	mLocalScalingM = mScalingM;

	FbxTransform::EInheritType mInheritType = pNode->InheritType.Get();
	if (mInheritType == FbxTransform::eInheritRrSs)
	{
		mGlobalRotationM = mParentGlobalRotationM * mLocalRotationM * mParentGlobalScalingM * mLocalScalingM;
	}
	else if (mInheritType == FbxTransform::eInheritRSrs)
	{
		mGlobalRotationM = mParentGlobalRotationM * mParentGlobalScalingM * mLocalRotationM * mLocalScalingM;
	}
	else if (mInheritType == FbxTransform::eInheritRrs)
	{
		FbxAMatrix mParentLocalScalingM;
		FbxVector4 mParentLocalScalingV = pParentNode->LclScaling.Get();
		mParentLocalScalingM.SetS(mParentLocalScalingV);

		FbxAMatrix mParentGlobalScalingM_noLocal = mParentGlobalScalingM * mParentLocalScalingM.Inverse();
		mGlobalRotationM = mParentGlobalRotationM * mLocalRotationM * mParentGlobalScalingM_noLocal * mLocalScalingM;
	}
	else
	{
		;
	}

	mTransform = mTranlationM * mRotationOffsetM * mRotationPivotM * mPreRotationM * mRotationM * mPostRotationM * mRotationPivotM.Inverse()
		* mScalingOffsetM * mScalingPivotM * mScalingM * mScalingPivotM.Inverse();

	FbxVector4 mLocalTWithAllPivotAndOffsetInfo = mTransform.GetT();

	FbxVector4 mGlobalTranslation = mParentTransform.MultT(mLocalTWithAllPivotAndOffsetInfo);
	mGlobalTranslationM.SetT(mGlobalTranslation);

	mTransform = mGlobalTranslationM * mGlobalRotationM;

	return mTransform;
}