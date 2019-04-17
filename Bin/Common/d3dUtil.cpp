//***************************************************************************************
// d3dUtil.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "d3dUtil.h"


ID3D11ShaderResourceView* d3dHelper::CreateTexture2DArraySRVW(
		ID3D11Device* device, ID3D11DeviceContext* context,
		std::vector<std::wstring>filenames)
{
	UINT size = filenames.size();

	std::vector<ID3D11Texture2D*> srcTex(size);
	for (UINT i = 0; i < size; ++i)
	{
		ScratchImage image;

		if (filenames[i].find(L".dds") != std::wstring::npos || filenames[i].find(L".DDS") != std::wstring::npos)
			LoadFromDDSFile(filenames[i].c_str(), DDS_FLAGS::DDS_FLAGS_NONE, nullptr, image);
		else if (filenames[i].find(L".tga") != std::wstring::npos || filenames[i].find(L".TGA") != std::wstring::npos)
			LoadFromTGAFile(filenames[i].c_str(), nullptr, image);
		else
			LoadFromWICFile(filenames[i].c_str(), WIC_FLAGS::WIC_FLAGS_NONE, nullptr, image);

		CreateTextureEx(device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ, 0, false, (ID3D11Resource**)&srcTex[i]);
	}

	//
	// Create the texture array.  Each element in the texture 
	// array has the same format/dimensions.
	//

	D3D11_TEXTURE2D_DESC texElementDesc;
	srcTex[0]->GetDesc(&texElementDesc);

	D3D11_TEXTURE2D_DESC texArrayDesc;
	texArrayDesc.Width              = texElementDesc.Width;
	texArrayDesc.Height             = texElementDesc.Height;
	texArrayDesc.MipLevels          = texElementDesc.MipLevels;
	texArrayDesc.ArraySize          = size;
	texArrayDesc.Format             = texElementDesc.Format;
	texArrayDesc.SampleDesc.Count   = 1;
	texArrayDesc.SampleDesc.Quality = 0;
	texArrayDesc.Usage              = D3D11_USAGE_DEFAULT;
	texArrayDesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
	texArrayDesc.CPUAccessFlags     = 0;
	texArrayDesc.MiscFlags          = 0;

	ID3D11Texture2D* texArray = 0;
	HR(device->CreateTexture2D( &texArrayDesc, 0, &texArray));

	//
	// Copy individual texture elements into texture array.
	//

	// for each texture element...
	for(UINT texElement = 0; texElement < size; ++texElement)
	{
		// for each mipmap level...
		for(UINT mipLevel = 0; mipLevel < texElementDesc.MipLevels; ++mipLevel)
		{
			D3D11_MAPPED_SUBRESOURCE mappedTex2D;
			HR(context->Map(srcTex[texElement], mipLevel, D3D11_MAP_READ, 0, &mappedTex2D));

			context->UpdateSubresource(texArray, 
				D3D11CalcSubresource(mipLevel, texElement, texElementDesc.MipLevels),
				0, mappedTex2D.pData, mappedTex2D.RowPitch, mappedTex2D.DepthPitch);

			context->Unmap(srcTex[texElement], mipLevel);
		}
	}	

	//
	// Create a resource view to the texture array.
	//
	
	D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
	viewDesc.Format = texArrayDesc.Format;
	viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	viewDesc.Texture2DArray.MostDetailedMip = 0;
	viewDesc.Texture2DArray.MipLevels = texArrayDesc.MipLevels;
	viewDesc.Texture2DArray.FirstArraySlice = 0;
	viewDesc.Texture2DArray.ArraySize = size;

	ID3D11ShaderResourceView* texArraySRV = 0;
	HR(device->CreateShaderResourceView(texArray, &viewDesc, &texArraySRV));

	//
	// Cleanup--we only need the resource view.
	//

	ReleaseCOM(texArray);

	for(UINT i = 0; i < size; ++i)
		ReleaseCOM(srcTex[i]);
	
	return texArraySRV;
}

ID3D11ShaderResourceView* d3dHelper::CreateTexture2DArraySRVA(
	ID3D11Device* device, ID3D11DeviceContext* context,
	std::vector<std::string>filenames)
{
	//
	// Load the texture elements individually from file.  These textures
	// won't be used by the GPU (0 bind flags), they are just used to 
	// load the image data from file.  We use the STAGING usage so the
	// CPU can read the resource.
	//

	UINT size = filenames.size();
	
	std::vector<ID3D11Texture2D*> srcTex(size);
	for (UINT i = 0; i < size; ++i)
	{
		ScratchImage image;

		wchar_t* filename = new wchar_t[filenames[i].size()];
		mbstowcs(filename, filenames[i].c_str(), filenames[i].size());

		if (filenames[i].find(".dds") != std::wstring::npos || filenames[i].find(".DDS") != std::wstring::npos)
			LoadFromDDSFile(filename, DDS_FLAGS::DDS_FLAGS_NONE, nullptr, image);
		else if (filenames[i].find(".tga") != std::wstring::npos || filenames[i].find(".TGA") != std::wstring::npos)
			LoadFromTGAFile(filename, nullptr, image);
		else
			LoadFromWICFile(filename, WIC_FLAGS::WIC_FLAGS_NONE, nullptr, image);

		CreateTextureEx(device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ, 0, false, (ID3D11Resource**)&srcTex[i]);
	}

	//
	// Create the texture array.  Each element in the texture 
	// array has the same format/dimensions.
	//

	D3D11_TEXTURE2D_DESC texElementDesc;
	srcTex[0]->GetDesc(&texElementDesc);

	D3D11_TEXTURE2D_DESC texArrayDesc;
	texArrayDesc.Width = texElementDesc.Width;
	texArrayDesc.Height = texElementDesc.Height;
	texArrayDesc.MipLevels = texElementDesc.MipLevels;
	texArrayDesc.ArraySize = size;
	texArrayDesc.Format = texElementDesc.Format;
	texArrayDesc.SampleDesc.Count = 1;
	texArrayDesc.SampleDesc.Quality = 0;
	texArrayDesc.Usage = D3D11_USAGE_DEFAULT;
	texArrayDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texArrayDesc.CPUAccessFlags = 0;
	texArrayDesc.MiscFlags = 0;

	ID3D11Texture2D* texArray = 0;
	HR(device->CreateTexture2D(&texArrayDesc, 0, &texArray));

	//
	// Copy individual texture elements into texture array.
	//

	// for each texture element...
	for (UINT texElement = 0; texElement < size; ++texElement)
	{
		// for each mipmap level...
		for (UINT mipLevel = 0; mipLevel < texElementDesc.MipLevels; ++mipLevel)
		{
			D3D11_MAPPED_SUBRESOURCE mappedTex2D;
			HR(context->Map(srcTex[texElement], mipLevel, D3D11_MAP_READ, 0, &mappedTex2D));

			context->UpdateSubresource(texArray,
				D3D11CalcSubresource(mipLevel, texElement, texElementDesc.MipLevels),
				0, mappedTex2D.pData, mappedTex2D.RowPitch, mappedTex2D.DepthPitch);

			context->Unmap(srcTex[texElement], mipLevel);
		}
	}

	//
	// Create a resource view to the texture array.
	//

	D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
	viewDesc.Format = texArrayDesc.Format;
	viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	viewDesc.Texture2DArray.MostDetailedMip = 0;
	viewDesc.Texture2DArray.MipLevels = texArrayDesc.MipLevels;
	viewDesc.Texture2DArray.FirstArraySlice = 0;
	viewDesc.Texture2DArray.ArraySize = size;

	ID3D11ShaderResourceView* texArraySRV = 0;
	HR(device->CreateShaderResourceView(texArray, &viewDesc, &texArraySRV));

	//
	// Cleanup--we only need the resource view.
	//

	ReleaseCOM(texArray);

	for (UINT i = 0; i < size; ++i)
		ReleaseCOM(srcTex[i]);

	return texArraySRV;
}

ID3D11ShaderResourceView* d3dHelper::CreateTexture2DSRVW(
	ID3D11Device* device, ID3D11DeviceContext* context,
	std::wstring filename)
{
	ScratchImage image;
	ScratchImage mipChain;
	ID3D11ShaderResourceView* SRV = nullptr;

	if (filename.find(L".dds") != std::wstring::npos || filename.find(L".DDS") != std::wstring::npos)
	{
		LoadFromDDSFile(filename.c_str(), DDS_FLAGS::DDS_FLAGS_NONE, nullptr, image);
		GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), TEX_FILTER_DEFAULT, 0, mipChain);
	}
	else if (filename.find(L".tga") != std::wstring::npos || filename.find(L".TGA") != std::wstring::npos)
	{
		LoadFromTGAFile(filename.c_str(), nullptr, image);
		GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), TEX_FILTER_DEFAULT, 0, mipChain);
	}
	else
	{
		LoadFromWICFile(filename.c_str(), WIC_FLAGS::WIC_FLAGS_NONE, nullptr, image);
		GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), TEX_FILTER_DEFAULT, 0, mipChain);
	}
	CreateShaderResourceView(device, mipChain.GetImages(), mipChain.GetImageCount(), mipChain.GetMetadata(), &SRV);
	return SRV;
}

ID3D11ShaderResourceView* d3dHelper::CreateTexture2DSRVA(
	ID3D11Device* device, ID3D11DeviceContext* context,
	std::string filename)
{
	ScratchImage image;
	ID3D11ShaderResourceView* SRV = nullptr;

	wchar_t* filenameA = new wchar_t[filename.size()];
	mbstowcs(filenameA, filename.c_str(), filename.size());

	if (filename.find(".dds") != std::wstring::npos || filename.find(".DDS") != std::wstring::npos)
		LoadFromDDSFile(filenameA, DDS_FLAGS::DDS_FLAGS_NONE, nullptr, image);
	else if (filename.find(".tga") != std::wstring::npos || filename.find(".TGA") != std::wstring::npos)
		LoadFromTGAFile(filenameA, nullptr, image);
	else
		LoadFromWICFile(filenameA, WIC_FLAGS::WIC_FLAGS_NONE, nullptr, image);

	CreateShaderResourceView(device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), &SRV);
	return SRV;
}

ID3D11ShaderResourceView* d3dHelper::CreateRandomTexture1DSRV(ID3D11Device* device)
{
	// 
	// Create the random data.
	//
	XMFLOAT4 randomValues[1024];

	for(int i = 0; i < 1024; ++i)
	{
		randomValues[i].x = MathHelper::RandF(-1.0f, 1.0f);
		randomValues[i].y = MathHelper::RandF(-1.0f, 1.0f);
		randomValues[i].z = MathHelper::RandF(-1.0f, 1.0f);
		randomValues[i].w = MathHelper::RandF(-1.0f, 1.0f);
	}

    D3D11_SUBRESOURCE_DATA initData;
    initData.pSysMem = randomValues;
	initData.SysMemPitch = 1024*sizeof(XMFLOAT4);
    initData.SysMemSlicePitch = 0;

	//
	// Create the texture.
	//
    D3D11_TEXTURE1D_DESC texDesc;
    texDesc.Width = 1024;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;
    texDesc.ArraySize = 1;

	ID3D11Texture1D* randomTex = 0;
    HR(device->CreateTexture1D(&texDesc, &initData, &randomTex));

	//
	// Create the resource view.
	//
    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
	viewDesc.Format = texDesc.Format;
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
    viewDesc.Texture1D.MipLevels = texDesc.MipLevels;
	viewDesc.Texture1D.MostDetailedMip = 0;
	
	ID3D11ShaderResourceView* randomTexSRV = 0;
    HR(device->CreateShaderResourceView(randomTex, &viewDesc, &randomTexSRV));

	ReleaseCOM(randomTex);

	return randomTexSRV;
}

ID3DX11Effect* d3dHelper::CreateEffectFromMemory(std::wstring filename, ID3D11Device* device)
{
	ID3DX11Effect* fx;

	std::ifstream fin(filename, std::ios::binary);

	fin.seekg(0, std::ios_base::end);
	int size = (int)fin.tellg();
	fin.seekg(0, std::ios_base::beg);
	std::vector<char> compiledShader(size);

	fin.read(&compiledShader[0], size);
	fin.close();

	HR(D3DX11CreateEffectFromMemory(&compiledShader[0], size,
		0, device, &fx));

	return fx;
}

void ExtractFrustumPlanes(XMFLOAT4 planes[6], CXMMATRIX M)
{
	//
	// Left
	//
	planes[0].x = M(0,3) + M(0,0);
	planes[0].y = M(1,3) + M(1,0);
	planes[0].z = M(2,3) + M(2,0);
	planes[0].w = M(3,3) + M(3,0);

	//
	// Right
	//
	planes[1].x = M(0,3) - M(0,0);
	planes[1].y = M(1,3) - M(1,0);
	planes[1].z = M(2,3) - M(2,0);
	planes[1].w = M(3,3) - M(3,0);

	//
	// Bottom
	//
	planes[2].x = M(0,3) + M(0,1);
	planes[2].y = M(1,3) + M(1,1);
	planes[2].z = M(2,3) + M(2,1);
	planes[2].w = M(3,3) + M(3,1);

	//
	// Top
	//
	planes[3].x = M(0,3) - M(0,1);
	planes[3].y = M(1,3) - M(1,1);
	planes[3].z = M(2,3) - M(2,1);
	planes[3].w = M(3,3) - M(3,1);

	//
	// Near
	//
	planes[4].x = M(0,2);
	planes[4].y = M(1,2);
	planes[4].z = M(2,2);
	planes[4].w = M(3,2);

	//
	// Far
	//
	planes[5].x = M(0,3) - M(0,2);
	planes[5].y = M(1,3) - M(1,2);
	planes[5].z = M(2,3) - M(2,2);
	planes[5].w = M(3,3) - M(3,2);

	// Normalize the plane equations.
	for(int i = 0; i < 6; ++i)
	{
		XMVECTOR v = XMPlaneNormalize(XMLoadFloat4(&planes[i]));
		XMStoreFloat4(&planes[i], v);
	}
}

const XMFLOAT4 operator+(XMFLOAT4& left, XMFLOAT4& right)
{
	return XMFLOAT4(left.x + right.x, left.y + right.y, left.z + right.z, left.w + right.w);
};

const XMFLOAT4 operator-(XMFLOAT4& left, XMFLOAT4& right)
{
	return XMFLOAT4(left.x - right.x, left.y - right.y, left.z - right.z, left.w - right.w);
};

const XMFLOAT4 operator*(XMFLOAT4& left, XMFLOAT4& right)
{
	return XMFLOAT4(left.x * right.x, left.y * right.y, left.z * right.z, left.w * right.w);
};

const XMFLOAT3 operator+(XMFLOAT3& left, XMFLOAT3& right)
{
	return XMFLOAT3(left.x + right.x, left.y + right.y, left.z + right.z);
};

const XMFLOAT3 operator-(XMFLOAT3& left, XMFLOAT3& right)
{
	return XMFLOAT3(left.x - right.x, left.y - right.y, left.z - right.z);
};

const XMFLOAT3 operator*(XMFLOAT3& left, XMFLOAT3& right)
{
	return XMFLOAT3(left.x * right.x, left.y * right.y, left.z * right.z);
};

const XMFLOAT2 operator*(XMFLOAT2& left, XMFLOAT2& right)
{
	return XMFLOAT2(left.x * right.x, left.y * right.y);
};