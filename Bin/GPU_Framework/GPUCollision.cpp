#include "../Bin/GPU_Framework/GPUCollision.h"

GpuCollision::GpuCollision(ID3D11Device* device, ID3D11DeviceContext* dc, ID3DX11Effect* buildPPLLFX, MeshManager* pmeshmanager) : mBodiesCount(0), pMeshManager(pmeshmanager)
{
	mDevice = device;
	mDC = dc;
	mBuildBodiesPPLLFX = buildPPLLFX;

	BuildBuffers();
	BuildFX();
}

void GpuCollision::InitializeBodiesList(std::map<BodyIndex, std::vector<MeshInstanceIndex>>& BodiesMeshesIndices)
{
	mBodiesMeshesIndices = BodiesMeshesIndices;
	mBodiesCount = mBodiesMeshesIndices.size();
	for (int i = 0; i < mBodiesMeshesIndices.size(); ++i)
	{
		for (int j = 0; j < mBodiesMeshesIndices[i].size(); ++j)
		{
			static std::vector<InstanceIndex>nullvec(0);
			mMeshesBodiesIndices.insert({ mBodiesMeshesIndices[i][j], i });
			mMeshesInstancesIndices.insert({ mBodiesMeshesIndices[i][j].first, nullvec});
			mMeshesInstancesIndices[mBodiesMeshesIndices[i][j].first].push_back(mBodiesMeshesIndices[i][j].second);
		}
	}
}

void GpuCollision::InitializeBodiesParams(MacroParams* bodiesParams)
{
	for(int i = 0; i < mBodiesCount; ++i)
		mBodiesMacroParams[i] = bodiesParams[i];
}

void GpuCollision::BuildGrid(void)
{
	//clear grid 
	mfxBodiesCount->SetInt(mBodiesCount);
	mfxPreIterationParticlesGridUAV->SetUnorderedAccessView(mPreIterationParticlesGridUAV);
	for (int i = 0; i < PARTICLES_GRID_CASCADES_COUNT; ++i)
	{
		mfxProcessingCascadeIndex->SetInt(i);
		
		D3DX11_TECHNIQUE_DESC techDesc;
		mfxClearParticlesGridTech->GetDesc(&techDesc);
		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			mfxClearParticlesGridTech->GetPassByIndex(p)->Apply(0, mDC);

			mDC->Dispatch(CS_THREAD_GROUPS_NUMBER, CS_THREAD_GROUPS_NUMBER, CS_THREAD_GROUPS_NUMBER);
		}
	}
	mDC->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	mDC->CSSetShaderResources(0, 1, pNullSRV);
	mDC->CSSetShader(0, 0, 0);

	//populate grid
	mfxBodiesCount->SetInt(mBodiesCount);
	mfxPreIterationParticlesGridUAV->SetUnorderedAccessView(mPreIterationParticlesGridUAV);
	mfxGridCellSizes->SetRawValue(reinterpret_cast<float*>(&mGridCellSizes), 0, sizeof(mGridCellSizes));
	mfxGridCenterPosW->SetRawValue(reinterpret_cast<float*>(&mGridCenterPosW), 0, sizeof(mGridCenterPosW));
	for (int i = 0; i < PARTICLES_GRID_CASCADES_COUNT; ++i)
	{
		mfxProcessingCascadeIndex->SetInt(i);
		mfxBodiesPPLLSRV->SetResource(mBodiesPPLLsSRVs[i]);
		mfxHeadIndexBufferSRV->SetResource(mHeadIndexBuffersSRVs[i]);

		D3DX11_TECHNIQUE_DESC techDesc;
		mfxPopulateGridTech->GetDesc(&techDesc);
		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			mfxPopulateGridTech->GetPassByIndex(p)->Apply(0, mDC);

			mDC->Dispatch(CS_THREAD_GROUPS_NUMBER, CS_THREAD_GROUPS_NUMBER, CS_THREAD_GROUPS_NUMBER);
		}
	}
	mDC->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	mDC->CSSetShaderResources(0, 1, pNullSRV);
	mDC->CSSetShader(0, 0, 0);
}

void GpuCollision::DetectCollisions(void)
{
	mfxPreIterationParticlesGridSRV->SetResource(mPreIterationParticlesGridSRV);
	mfxBodiesMacroParametersBufferSRV->SetResource(mBodiesMacroParametersBufferSRV);
	mfxPostIterationParticlesGridUAV->SetUnorderedAccessView(mPostIterationParticlesGridUAV);
	mfxDetectBodiesGridCenterPosW->SetRawValue(reinterpret_cast<float*>(&mGridCenterPosW), 0, sizeof(mGridCenterPosW));
	mfxDetectBodiesGridCellSizes->SetRawValue(reinterpret_cast<float*>(&mGridCellSizes), 0, sizeof(mGridCellSizes));
	mfxDetectBodiesCount->SetInt(mBodiesCount);
	
	for (int i = 0; i < PARTICLES_GRID_CASCADES_COUNT; ++i)
	{
		mfxDetectProcessingCascadeIndex->SetInt(i);

		D3DX11_TECHNIQUE_DESC techDesc;
		mfxDetectCollisionsTech->GetDesc(&techDesc);
		for (UINT p = 0; p < techDesc.Passes; ++p)
		{
			mfxDetectCollisionsTech->GetPassByIndex(p)->Apply(0, mDC);
			mDC->Dispatch(CS_THREAD_GROUPS_NUMBER, CS_THREAD_GROUPS_NUMBER, CS_THREAD_GROUPS_NUMBER);
		}
	}

	mDC->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	mDC->CSSetShaderResources(0, 1, pNullSRV);
	mDC->CSSetShader(0, 0, 0);
}

std::vector<BodyIndex> GpuCollision::GetBodiesIndicesByMeshIndex(MeshIndex meshIndex)
{
	std::vector<BodyIndex>bodies;
	for (int i = 0; i < mMeshesInstancesIndices[meshIndex].size(); ++i)
	{
		bodies.push_back(mMeshesBodiesIndices[std::make_pair(meshIndex, mMeshesInstancesIndices[meshIndex][i])]);
	}
	return bodies;
};

void GpuCollision::UpdateBodiesMacroParams(float dt)
{
	//copy post-iteration buffer to cpu-readable buffer
	mDC->CopyResource(mPostIterationParticlesGridCopy, mPostIterationParticlesGrid);
	//map cpu-readable buffer
	D3D11_MAPPED_SUBRESOURCE mappedData;
	mDC->Map(mPostIterationParticlesGridCopy, 0, D3D11_MAP_READ, 0, &mappedData);
	MicroParams *v = reinterpret_cast<MicroParams*>(mappedData.pData);
	
	for (int cascadeIdx = 0; cascadeIdx < PARTICLES_GRID_CASCADES_COUNT; ++cascadeIdx)
	{
		for (int x = 0; x < PARTICLES_GRID_RESOLUTION; ++x)
		{
			for (int y = 0; y < PARTICLES_GRID_RESOLUTION; ++y)
			{
				for (int z = 0; z < PARTICLES_GRID_RESOLUTION; ++z)
				{
					if (cascadeIdx == 0 || (cascadeIdx > 0 &&
						abs(mGridCellSizes[cascadeIdx].x * (x - PARTICLES_GRID_RESOLUTION / 2.0f)) > mGridCellSizes[cascadeIdx - 1].x * PARTICLES_GRID_RESOLUTION / 2 ||
						abs(mGridCellSizes[cascadeIdx].y * (y - PARTICLES_GRID_RESOLUTION / 2.0f)) > mGridCellSizes[cascadeIdx - 1].y * PARTICLES_GRID_RESOLUTION / 2 ||
						abs(mGridCellSizes[cascadeIdx].z * (z - PARTICLES_GRID_RESOLUTION / 2.0f)) > mGridCellSizes[cascadeIdx - 1].z * PARTICLES_GRID_RESOLUTION / 2))
					{
						//compute grid index
						int gridIdx = cascadeIdx * PARTICLES_COUNT + z * PARTICLES_GRID_RESOLUTION * PARTICLES_GRID_RESOLUTION + x * PARTICLES_GRID_RESOLUTION + y;
						int bodyIdx = v[gridIdx].BodyIndex;
						if (bodyIdx != mBodiesCount && bodyIdx != 0)
						{
							mBodiesMacroParams[bodyIdx].Speed.x += v[gridIdx].LinearMomentum.x * dt;
							mBodiesMacroParams[bodyIdx].Speed.y += v[gridIdx].LinearMomentum.y * dt;
							mBodiesMacroParams[bodyIdx].Speed.z += v[gridIdx].LinearMomentum.z * dt;

							mBodiesMacroParams[bodyIdx].AngularSpeed.x += v[gridIdx].AngularMomentum.x * dt;
							mBodiesMacroParams[bodyIdx].AngularSpeed.y += v[gridIdx].AngularMomentum.y * dt;
							mBodiesMacroParams[bodyIdx].AngularSpeed.z += v[gridIdx].AngularMomentum.z * dt;
						}
					}
				}
			}
		}
	}
	//gravity
	for (int i = 1; i < mBodiesCount; ++i)
	{
		mBodiesMacroParams[i].Speed.y -= 9.8f * dt;
	}
	mDC->Unmap(mPostIterationParticlesGridCopy, 0);
}

void GpuCollision::SetBodiesMacroParams(MacroParams *macroParams) {
	D3D11_MAPPED_SUBRESOURCE mappedData;
	mDC->Map(mBodiesMacroParametersCPUBuffer, 0, D3D11_MAP_WRITE, 0, &mappedData);
	MacroParams *v = reinterpret_cast<MacroParams*>(mappedData.pData);
	for (int i = 0; i < mBodiesCount; ++i)
		v[i] = macroParams[i];
	mDC->Unmap(mBodiesMacroParametersCPUBuffer, 0);
	mDC->CopyResource(mBodiesMacroParametersGPUBuffer, mBodiesMacroParametersCPUBuffer);
}

void GpuCollision::UpdateMeshesInstancesMatrices(FLOAT dt)
{
	for (int i = 0; i < mBodiesCount; ++i)
	{
		for (int j = 0; j < mBodiesMeshesIndices[i].size(); ++j)
		{
			//update quaternion
			XMVECTOR scale, rotQuat, trans;
			XMMatrixDecompose(&scale, &rotQuat, &trans, pMeshManager->GetMeshWorldMatrixXM(mBodiesMeshesIndices[i][j].first, mBodiesMeshesIndices[i][j].second));
			
			XMVECTOR angSpeed = XMLoadFloat4(&XMFLOAT4(mBodiesMacroParams[i].AngularSpeed.x, mBodiesMacroParams[i].AngularSpeed.y, mBodiesMacroParams[i].AngularSpeed.z, 0.0f));
			
			FLOAT rotAngle = XMVectorGetX(XMVector3Length(angSpeed * dt));
			XMVECTOR rotAxis = XMVector4Normalize(angSpeed);
			XMQuaternionRotationNormal(rotAxis, rotAngle);
			
			XMVECTOR deltaRotQuat = XMQuaternionRotationAxis(rotAxis, rotAngle); 
			rotQuat = XMQuaternionMultiply(deltaRotQuat, rotQuat);
			XMStoreFloat4(&mBodiesMacroParams[i].Quaternion, rotQuat);
			
			XMVECTOR velocity = XMLoadFloat3(&mBodiesMacroParams[i].Speed);
			trans += velocity * dt;
			mBodiesMacroParams[i].CMPosW = mBodiesMacroParams[i].CMPosW + XMFLOAT3(dt * mBodiesMacroParams[i].Speed.x, dt * mBodiesMacroParams[i].Speed.y, dt * mBodiesMacroParams[i].Speed.z);
			
			pMeshManager->SetMeshWorldMatrixXM(mBodiesMeshesIndices[i][j].first, mBodiesMeshesIndices[i][j].second, &(XMMatrixIdentity() * XMMatrixScalingFromVector(scale) * XMMatrixRotationQuaternion(rotQuat) * XMMatrixTranslationFromVector(trans)));
		}
	}
}

void GpuCollision::SetPerIterationData(void)
{
	mfxBodiesPPLLUAV->SetUnorderedAccessView(mBodiesPPLLsUAVs[mCurrentPPLLCascadeIndex]);
	mfxHeadIndexBufferUAV->SetUnorderedAccessView(mHeadIndexBuffersUAVs[mCurrentPPLLCascadeIndex]);
}

void GpuCollision::SetPerMeshData(MeshIndex meshIdx)
{
	if (mMeshesInstancesIndices.find(meshIdx) != mMeshesInstancesIndices.end())
	{
		mfxBodyIndex->SetRawValue(GetBodiesIndicesByMeshIndex(meshIdx).data(), 0, sizeof(BodyIndex) * GetBodiesIndicesByMeshIndex(meshIdx).size());
	}
}

void GpuCollision::Update(Camera& cam)
{
	mCamera = cam;
	BuildCamera();
	BuildViewport();
}

void GpuCollision::Setup(void)
{
	UINT CleanVal = mBodiesCount;
	for (int i = 0; i < PARTICLES_GRID_CASCADES_COUNT; ++i)
		mDC->ClearUnorderedAccessViewUint(mHeadIndexBuffersUAVs[i], &CleanVal);
}

void GpuCollision::Compute(FLOAT dt)
{
	SetBodiesMacroParams(mBodiesMacroParams);

	BuildGrid();
	DetectCollisions();
	
	UpdateBodiesMacroParams(dt);
	UpdateMeshesInstancesMatrices(dt);
}

void GpuCollision::BuildBuffers(void)
{
	//PPLL render target
	D3D11_TEXTURE2D_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(texDesc));
	texDesc.Width = 128;
	texDesc.Height = 128;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	ID3D11Texture2D* Tex = 0;
	HR(mDevice->CreateTexture2D(&texDesc, 0, &Tex));
	HR(mDevice->CreateRenderTargetView(Tex, 0, &mOffscreenRTV));
	ReleaseCOM(Tex);

	//bodies list buffer
	D3D11_TEXTURE2D_DESC headTexDesc;
	headTexDesc.Width = PARTICLES_GRID_RESOLUTION;
	headTexDesc.Height = PARTICLES_GRID_RESOLUTION;
	headTexDesc.MipLevels = 1;
	headTexDesc.ArraySize = 1;
	headTexDesc.Format = DXGI_FORMAT_R32_UINT;
	headTexDesc.SampleDesc.Count = 1;
	headTexDesc.SampleDesc.Quality = 0;
	headTexDesc.Usage = D3D11_USAGE_DEFAULT;
	headTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	headTexDesc.CPUAccessFlags = 0;
	headTexDesc.MiscFlags = 0;

	for (int i = 0; i < PARTICLES_GRID_CASCADES_COUNT; ++i)
	{
		HR(mDevice->CreateTexture2D(&headTexDesc, 0, &mHeadIndexBuffers[i]));
		HR(mDevice->CreateShaderResourceView(mHeadIndexBuffers[i], 0, &mHeadIndexBuffersSRVs[i]));
		HR(mDevice->CreateUnorderedAccessView(mHeadIndexBuffers[i], 0, &mHeadIndexBuffersUAVs[i]));
	}

	D3D11_BUFFER_DESC fraglistdesc;
	ZeroMemory(&fraglistdesc, sizeof(fraglistdesc));
	fraglistdesc.Usage = D3D11_USAGE_DEFAULT;
	fraglistdesc.ByteWidth = sizeof(BodiesListNode) * PARTICLES_GRID_RESOLUTION * PARTICLES_GRID_RESOLUTION * PPLL_LAYERS_COUNT;
	fraglistdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	fraglistdesc.CPUAccessFlags = 0;
	fraglistdesc.StructureByteStride = sizeof(BodiesListNode);
	fraglistdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	for (int i = 0; i < PARTICLES_GRID_CASCADES_COUNT; ++i)
	{
		HR(mDevice->CreateBuffer(&fraglistdesc, NULL, &mBodiesPPLLs[i]));
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC fraglistuavdesc;
	ZeroMemory(&fraglistuavdesc, sizeof(fraglistuavdesc));
	fraglistuavdesc.Format = DXGI_FORMAT_UNKNOWN;
	fraglistuavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	fraglistuavdesc.Buffer.FirstElement = 0;
	fraglistuavdesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
	fraglistuavdesc.Buffer.NumElements = PARTICLES_GRID_RESOLUTION * PARTICLES_GRID_RESOLUTION * PPLL_LAYERS_COUNT;
	for (int i = 0; i < PARTICLES_GRID_CASCADES_COUNT; ++i)
	{
		HR(mDevice->CreateUnorderedAccessView(mBodiesPPLLs[i], &fraglistuavdesc, &mBodiesPPLLsUAVs[i]));
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC fraglistsrvdesc;
	ZeroMemory(&fraglistsrvdesc, sizeof(fraglistsrvdesc));
	fraglistsrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	fraglistsrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	fraglistsrvdesc.Buffer.ElementWidth = PARTICLES_GRID_RESOLUTION * PARTICLES_GRID_RESOLUTION * PPLL_LAYERS_COUNT;
	for (int i = 0; i < PARTICLES_GRID_CASCADES_COUNT; ++i)
	{
		HR(mDevice->CreateShaderResourceView(mBodiesPPLLs[i], &fraglistsrvdesc, &mBodiesPPLLsSRVs[i]));
	}

	//bodies macro parameters
	D3D11_BUFFER_DESC macroparamscpubufdesc;
	macroparamscpubufdesc.Usage = D3D11_USAGE_STAGING;
	macroparamscpubufdesc.ByteWidth = sizeof(MacroParams) * MAX_BODIES_COUNT;
	macroparamscpubufdesc.BindFlags = 0;
	macroparamscpubufdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	macroparamscpubufdesc.StructureByteStride = sizeof(MacroParams);
	macroparamscpubufdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	D3D11_SUBRESOURCE_DATA initData;
	initData.pSysMem = mBodiesMacroParams;
	HR(mDevice->CreateBuffer(&macroparamscpubufdesc, &initData, &mBodiesMacroParametersCPUBuffer));

	D3D11_BUFFER_DESC macroparamsgpubufdesc;
	macroparamsgpubufdesc.Usage = D3D11_USAGE_DEFAULT;
	macroparamsgpubufdesc.ByteWidth = sizeof(MacroParams) * MAX_BODIES_COUNT;
	macroparamsgpubufdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	macroparamsgpubufdesc.CPUAccessFlags = 0;
	macroparamsgpubufdesc.StructureByteStride = sizeof(MacroParams);
	macroparamsgpubufdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HR(mDevice->CreateBuffer(&macroparamsgpubufdesc, 0, &mBodiesMacroParametersGPUBuffer));

	D3D11_SHADER_RESOURCE_VIEW_DESC macroparamsbufsrvdesc;
	ZeroMemory(&macroparamsbufsrvdesc, sizeof(macroparamsbufsrvdesc));
	macroparamsbufsrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	macroparamsbufsrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	macroparamsbufsrvdesc.Buffer.ElementWidth = MAX_BODIES_COUNT;
	HR(mDevice->CreateShaderResourceView(mBodiesMacroParametersGPUBuffer, &macroparamsbufsrvdesc, &mBodiesMacroParametersBufferSRV));

	D3D11_UNORDERED_ACCESS_VIEW_DESC macroparamsbufuavdesc;
	ZeroMemory(&macroparamsbufuavdesc, sizeof(macroparamsbufuavdesc));
	macroparamsbufuavdesc.Format = DXGI_FORMAT_UNKNOWN;
	macroparamsbufuavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	macroparamsbufuavdesc.Buffer.FirstElement = 0;
	macroparamsbufuavdesc.Buffer.Flags = 0;
	macroparamsbufuavdesc.Buffer.NumElements = MAX_BODIES_COUNT;
	HR(mDevice->CreateUnorderedAccessView(mBodiesMacroParametersGPUBuffer, &macroparamsbufuavdesc, &mBodiesMacroParametersBufferUAV));

	//particles grid
	D3D11_BUFFER_DESC gridbufdesc;
	ZeroMemory(&gridbufdesc, sizeof(gridbufdesc));
	gridbufdesc.Usage = D3D11_USAGE_DEFAULT;
	gridbufdesc.ByteWidth = sizeof(MicroParams) * PARTICLES_COUNT * PARTICLES_GRID_CASCADES_COUNT;
	gridbufdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	gridbufdesc.CPUAccessFlags = 0;
	gridbufdesc.StructureByteStride = sizeof(MicroParams);
	gridbufdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	HR(mDevice->CreateBuffer(&gridbufdesc, NULL, &mPreIterationParticlesGrid));
	HR(mDevice->CreateBuffer(&gridbufdesc, NULL, &mPostIterationParticlesGrid));

	D3D11_BUFFER_DESC gridbufcopydesc;
	ZeroMemory(&gridbufdesc, sizeof(gridbufdesc));
	gridbufcopydesc.Usage = D3D11_USAGE_STAGING;
	gridbufcopydesc.ByteWidth = sizeof(MicroParams) * PARTICLES_COUNT * PARTICLES_GRID_CASCADES_COUNT;
	gridbufcopydesc.BindFlags = 0;
	gridbufcopydesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	gridbufcopydesc.StructureByteStride = sizeof(MicroParams);
	gridbufcopydesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	HR(mDevice->CreateBuffer(&gridbufcopydesc, NULL, &mPostIterationParticlesGridCopy));

	D3D11_UNORDERED_ACCESS_VIEW_DESC gridbufferuavdesc;
	ZeroMemory(&gridbufferuavdesc, sizeof(gridbufferuavdesc));
	gridbufferuavdesc.Format = DXGI_FORMAT_UNKNOWN;
	gridbufferuavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	gridbufferuavdesc.Buffer.FirstElement = 0;
	gridbufferuavdesc.Buffer.Flags = 0;
	gridbufferuavdesc.Buffer.NumElements = PARTICLES_COUNT * PARTICLES_GRID_CASCADES_COUNT;

	HR(mDevice->CreateUnorderedAccessView(mPreIterationParticlesGrid, &gridbufferuavdesc, &mPreIterationParticlesGridUAV));
	HR(mDevice->CreateUnorderedAccessView(mPostIterationParticlesGrid, &gridbufferuavdesc, &mPostIterationParticlesGridUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC gridbuffersrvdesc;
	ZeroMemory(&gridbuffersrvdesc, sizeof(gridbuffersrvdesc));
	gridbuffersrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	gridbuffersrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	gridbuffersrvdesc.Buffer.ElementWidth = PARTICLES_COUNT * PARTICLES_GRID_CASCADES_COUNT;
	HR(mDevice->CreateShaderResourceView(mPreIterationParticlesGrid, &gridbuffersrvdesc, &mPreIterationParticlesGridSRV));
	HR(mDevice->CreateShaderResourceView(mPostIterationParticlesGrid, &gridbuffersrvdesc, &mPostIterationParticlesGridSRV));
}

void GpuCollision::SetTestParams()
{
	testBodiesCount->SetInt(mBodiesCount);
	testParticlesGrid->SetResource(mPreIterationParticlesGridSRV);
	testCellSizes->SetRawValue(reinterpret_cast<float*>(&mGridCellSizes), 0, sizeof(mGridCellSizes));
	testCenterPosW->SetRawValue(reinterpret_cast<float*>(&mGridCenterPosW), 0, sizeof(mGridCenterPosW));
}

void GpuCollision::BuildFX(void)
{
	//test
	testBodiesCount = mBuildBodiesPPLLFX->GetVariableByName("gBodiesCount")->AsScalar();
	testCenterPosW = mBuildBodiesPPLLFX->GetVariableByName("gBodiesGridCenterPosW");
	testCellSizes = mBuildBodiesPPLLFX->GetVariableByName("gBodiesGridCellSizes");
	testParticlesGrid = mBuildBodiesPPLLFX->GetVariableByName("gPreIterationParticlesGridRO")->AsShaderResource();
	//ppll
	mfxBodiesPPLLUAV = mBuildBodiesPPLLFX->GetVariableByName("gBodiesListBufferRW")->AsUnorderedAccessView();
	mfxHeadIndexBufferUAV = mBuildBodiesPPLLFX->GetVariableByName("gHeadIndexBufferRW")->AsUnorderedAccessView();
	mfxBodyIndex = mBuildBodiesPPLLFX->GetVariableByName("gBodyIndex");
	//build grid
	mGridFX = d3dHelper::CreateEffectFromMemory(L"FX/GPUCollisionBuildGrid.cso", mDevice);
	mfxClearParticlesGridTech = mGridFX->GetTechniqueByName("ClearParticlesGridTech");
	mfxPopulateGridTech = mGridFX->GetTechniqueByName("PopulateGridTech");
	mfxBodiesPPLLSRV = mGridFX->GetVariableByName("gBodiesListBufferRO")->AsShaderResource();
	mfxHeadIndexBufferSRV = mGridFX->GetVariableByName("gHeadIndexBufferRO")->AsShaderResource();
	mfxBodiesMacroParametersBufferSRV = mGridFX->GetVariableByName("gBodiesMacroParametersBufferRO")->AsShaderResource();
	mfxPreIterationParticlesGridUAV = mGridFX->GetVariableByName("gPreIterationParticlesGridRW")->AsUnorderedAccessView();
	mfxBodiesCount = mGridFX->GetVariableByName("gBodiesCount")->AsScalar();
	mfxProcessingCascadeIndex = mGridFX->GetVariableByName("gProcessingCascadeIndex")->AsScalar();
	mfxGridCenterPosW = mGridFX->GetVariableByName("gBodiesGridCenterPosW");
	mfxGridCellSizes = mGridFX->GetVariableByName("gBodiesGridCellSizes");
	//detect collisions
	mDetectCollisionsFX = d3dHelper::CreateEffectFromMemory(L"FX/GPUCollisionDetect.cso", mDevice);
	mfxDetectCollisionsTech = mDetectCollisionsFX->GetTechniqueByName("DetectCollisionsTech");
	mfxPreIterationParticlesGridSRV = mDetectCollisionsFX->GetVariableByName("gPreIterationParticlesGridRO")->AsShaderResource();
	mfxBodiesMacroParametersBufferSRV = mDetectCollisionsFX->GetVariableByName("gBodiesMacroParametersBufferRO")->AsShaderResource();
	mfxPostIterationParticlesGridUAV = mDetectCollisionsFX->GetVariableByName("gPostIterationParticlesGridRW")->AsUnorderedAccessView();
	mfxDetectBodiesGridCenterPosW = mDetectCollisionsFX->GetVariableByName("gBodiesGridCenterPosW");
	mfxDetectBodiesGridCellSizes = mDetectCollisionsFX->GetVariableByName("gBodiesGridCellSizes");
	mfxDetectProcessingCascadeIndex = mDetectCollisionsFX->GetVariableByName("gProcessingCascadeIndex")->AsScalar();
	mfxDetectBodiesCount = mDetectCollisionsFX->GetVariableByName("gBodiesCount")->AsScalar();
}

void GpuCollision::CalculateCellSizes()
{
	for (int i = 1; i <= PARTICLES_GRID_CASCADES_COUNT; ++i)
	{
		float u = UPDATE_DISTANCE * (float)i / (float)PARTICLES_GRID_CASCADES_COUNT;
		FLOAT mCellSize = u / PARTICLES_GRID_RESOLUTION;
		mGridCellSizes[i - 1] = XMFLOAT4(mCellSize, mCellSize, mCellSize, mCellSize);
	}
}

void GpuCollision::BuildCamera(void)
{
	CalculateCellSizes();

	for (int i = 0; i < PARTICLES_GRID_CASCADES_COUNT; ++i)
	{
		FLOAT mCellSize = mGridCellSizes[i].x;

		XMVECTOR eyePos = mCamera.GetPositionXM() + XMVectorSet(0.0f, 0.0f, (PARTICLES_GRID_RESOLUTION / 2.0f * mCellSize), 0.0f);
		XMMATRIX View = XMMatrixLookAtLH(eyePos, mCamera.GetPositionXM(), XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f));
		XMMATRIX Proj = XMMatrixOrthographicLH(PARTICLES_GRID_RESOLUTION * mCellSize, PARTICLES_GRID_RESOLUTION * mCellSize, 0.0f, mCellSize * PARTICLES_GRID_RESOLUTION);

		XMStoreFloat4(&mGridCenterPosW[i], mCamera.GetPositionXM());

		XMFLOAT4X4 view, proj;
		XMStoreFloat4x4(&view, View);
		XMStoreFloat4x4(&proj, Proj);
		XMFLOAT3 eyePosF3;
		XMStoreFloat3(&eyePosF3, eyePos);
		mCascadeCamera[i].SetPosition(eyePosF3);
		mCascadeCamera[i].SetProj(proj);
		mCascadeCamera[i].SetView(view);
	}
}

void GpuCollision::BuildViewport(void)
{
	InitViewport(&mOffscreenRTViewport, 0.0f, 0.0f, 128, 128, 0.0f, 1.0f);
}