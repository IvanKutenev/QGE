#include "../Bin/GPU_Framework/SVOPipeline.h"

SvoPipeline::SvoPipeline(ID3D11Device* device, ID3D11DeviceContext* dc, ID3DX11Effect* buildSvoFx,
	ID3D11ShaderResourceView* normalDepthMapSRV, ID3D11ShaderResourceView* environmentMapSRV, FLOAT clientW, FLOAT clientH) :
	mBuildSvoFX(NULL),
	build_mfxSvoMinAveragingLevelIdx(NULL),
	build_mfxSVOViewProjMatrices(NULL),
	build_mfxSvoRootNodeCenterPosW(NULL),
	build_mfxVoxelsPoolSRV(NULL),
	build_mfxNodesPoolSRV(NULL),
	build_mfxNodesPoolUAV(NULL),
	build_mfxVoxelsPoolUAV(NULL),
	build_mfxVoxelsFlagsUAV(NULL),
	mSvoTraceRaysFX(NULL),
	trace_mfxSvoMinAveragingLevelIdx(NULL),
	trace_mfxSvoRayCastLevelIdx(NULL),
	trace_mfxClientW(NULL),
	trace_mfxClientH(NULL),
	trace_mfxReflectionMapMaskSampleLevel(NULL),
	trace_mfxConeTheta(NULL),
	trace_mfxEyePosW(NULL),
	trace_mfxInvProj(NULL),
	trace_mfxInvView(NULL),
	trace_mfxSvoRootNodeCenterPosW(NULL),
	trace_mfxNodesPoolUAV(NULL),
	trace_mfxRayCastedOutputUAV(NULL),
	trace_mfxReflectionMapUAV(NULL),
	trace_mfxReflectionMapMaskUAV(NULL),
	trace_mfxNodesPoolSRV(NULL),
	trace_mfxVoxelsPoolSRV(NULL),
	trace_mfxVoxelsFlagsSRV(NULL),
	trace_mfxNormalDepthMapSRV(NULL),
	trace_mfxEnvironmentMapSRV(NULL),
	trace_mfxReflectionMapSRV(NULL),
	mResetRootNodeTech(NULL),
	mCastRaysTech(NULL),
	mCastReflectionRaysTech(NULL),
	mTraceReflectionConesTech(NULL),
	mEnvironmentMapSRV(environmentMapSRV)
{
	md3dDevice = device;
	md3dImmediateContext = dc;
	mBuildSvoFX = buildSvoFx;

	OnSize(normalDepthMapSRV, clientW, clientH);
	
	BuildFX();
	BuildBuffers();
	BuildViewport();
	BuildInputLayout();
}

void SvoPipeline::OnSize(ID3D11ShaderResourceView* normalDepthMapSRV, FLOAT clientW, FLOAT clientH) {
	
	mNormalDepthMapSRV = normalDepthMapSRV;
	
	mClientW = clientW;
	mClientH = clientH;
	
	BuildViews();
};

void SvoPipeline::Update(Camera camera)
{
	mCamera = camera;
	BuildCamera();
}

void SvoPipeline::ResetNodesPool()
{
	trace_mfxNodesPoolUAV->SetUnorderedAccessView(mNodesPoolUAV);
	trace_mfxSvoRootNodeCenterPosW->SetFloatVector(reinterpret_cast<float*>(&mSvoRootNodeCenterPosW));

	D3DX11_TECHNIQUE_DESC techDesc;
	mResetRootNodeTech->GetDesc(&techDesc);

	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		mResetRootNodeTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
		md3dImmediateContext->Dispatch(ceil(NODES_COUNT / RESET_SVO_NUM_THREADS), 1, 1);
	}
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	md3dImmediateContext->CSSetShader(0, 0, 0);
}

void SvoPipeline::ResetVoxelsPool()
{
	UINT voxelEmptyFlag = VOXEL_EMPTY_FLAG;
	md3dImmediateContext->ClearUnorderedAccessViewUint(mVoxelsFlagsUAV, &voxelEmptyFlag);
	md3dImmediateContext->ClearUnorderedAccessViewUint(mVoxelsPoolUAV, &voxelEmptyFlag);
}

void SvoPipeline::Reset()
{
	ResetNodesPool();
	ResetVoxelsPool();
}

void SvoPipeline::Setup()
{
	build_mfxSvoMinAveragingLevelIdx->SetInt(SVO_MIN_AVG_LEVEL_IDX);
	build_mfxSVOViewProjMatrices->SetRawValue(mSVOViewProjMatrices, 0, sizeof(mSVOViewProjMatrices));
	build_mfxSvoRootNodeCenterPosW->SetFloatVector(reinterpret_cast<float*>(&mSvoRootNodeCenterPosW));
	build_mfxNodesPoolUAV->SetUnorderedAccessView(mNodesPoolUAV);
	build_mfxVoxelsPoolUAV->SetUnorderedAccessView(mVoxelsPoolUAV);
	build_mfxVoxelsFlagsUAV->SetUnorderedAccessView(mVoxelsFlagsUAV);
}

void SvoPipeline::CopyTree()
{
	md3dImmediateContext->CopyResource(mNodesPoolCopy, mNodesPool);
	D3D11_MAPPED_SUBRESOURCE debugMappedData, viewMappedData;
	md3dImmediateContext->Map(mNodesPoolCopy, 0, D3D11_MAP_READ, 0, &debugMappedData);
	md3dImmediateContext->Map(mNodesPoolDebugDraw, 0, D3D11_MAP_WRITE_DISCARD, 0, &viewMappedData);
	SvoNode *debug = reinterpret_cast<SvoNode*>(debugMappedData.pData);
	SvoDebugDrawVsIn *view = reinterpret_cast<SvoDebugDrawVsIn*>(viewMappedData.pData);
	mFilledNodesCount = 0;
	for (int j = 0; j < NODES_COUNT; ++j)
	{
		if (debug[j].svoLevelIdx != SVO_LEVELS_COUNT)
		{
			view[mFilledNodesCount].PosW = debug[j].PosW;
			view[mFilledNodesCount].svoLevelIdx = debug[j].svoLevelIdx;
			mFilledNodesCount++;
		}
	}
	md3dImmediateContext->Unmap(mNodesPoolDebugDraw, 0);
	md3dImmediateContext->Unmap(mNodesPoolCopy, 0);
}

void SvoPipeline::ViewTree(ID3D11RenderTargetView* rtv, ID3D11DepthStencilView* dsv, D3D11_VIEWPORT vp)
{
	CopyTree();

	md3dImmediateContext->OMSetRenderTargets(1, &rtv, dsv);
	md3dImmediateContext->RSSetViewports(1, &vp);

	md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	md3dImmediateContext->IASetInputLayout(mSvoDebugDrawInputLayout);

	const UINT stride = sizeof(SvoDebugDrawVsIn);
	const UINT offset = 0;
	md3dImmediateContext->IASetVertexBuffers(0, 1, &mNodesPoolDebugDraw, &stride, &offset);

	mfxViewProj->SetMatrix(reinterpret_cast<float*>(&mCamera.ViewProj()));

	D3DX11_TECHNIQUE_DESC techDesc;
	mSvoDebugDrawTech->GetDesc(&techDesc);

	for (int p = 0; p < techDesc.Passes; ++p)
	{
		mSvoDebugDrawTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
		md3dImmediateContext->Draw(mFilledNodesCount, 0);
	}
}

void SvoPipeline::SetTestParams()
{
	build_mfxVoxelsPoolSRV->SetResource(mVoxelsPoolSRV);
	build_mfxNodesPoolSRV->SetResource(mNodesPoolSRV);
	build_mfxSvoRootNodeCenterPosW->SetFloatVector(reinterpret_cast<float*>(&mSvoRootNodeCenterPosW));
}

void SvoPipeline::SvoCastRays()
{
	trace_mfxNodesPoolSRV->SetResource(mNodesPoolSRV);
	trace_mfxVoxelsPoolSRV->SetResource(mVoxelsPoolSRV);
	trace_mfxVoxelsFlagsSRV->SetResource(mVoxelsFlagsSRV);
	trace_mfxEnvironmentMapSRV->SetResource(mEnvironmentMapSRV);
	trace_mfxRayCastedOutputUAV->SetUnorderedAccessView(mRayCastedOutputUAV);
	trace_mfxSvoRootNodeCenterPosW->SetFloatVector(reinterpret_cast<float*>(&mSvoRootNodeCenterPosW));
	trace_mfxClientH->SetFloat(mClientH);
	trace_mfxClientW->SetFloat(mClientW);
	trace_mfxSvoRayCastLevelIdx->SetInt(RAY_CASTER_MAX_LEVEL_IDX);
	trace_mfxEyePosW->SetFloatVector(reinterpret_cast<float*>(&(mCamera.GetPosition())));
	trace_mfxInvProj->SetMatrix(reinterpret_cast<float*>(&XMMatrixInverse(&XMMatrixDeterminant(mCamera.Proj()), mCamera.Proj())));
	trace_mfxInvView->SetMatrix(reinterpret_cast<float*>(&XMMatrixInverse(&XMMatrixDeterminant(mCamera.View()), mCamera.View())));

	D3DX11_TECHNIQUE_DESC techDesc;
	mCastRaysTech->GetDesc(&techDesc);

	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		mCastRaysTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
		md3dImmediateContext->Dispatch(ceil(mClientW / RAY_CASTER_NUM_THREADS), ceil(mClientH / RAY_CASTER_NUM_THREADS), 1);
	}
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	md3dImmediateContext->CSSetShader(0, 0, 0);
}

void SvoPipeline::SvoCastReflectionRays()
{
	trace_mfxNodesPoolSRV->SetResource(mNodesPoolSRV);
	trace_mfxVoxelsPoolSRV->SetResource(mVoxelsPoolSRV);
	trace_mfxVoxelsFlagsSRV->SetResource(mVoxelsFlagsSRV);
	trace_mfxReflectionMapUAV->SetUnorderedAccessView(mReflectionMapUAV);
	trace_mfxReflectionMapMaskUAV->SetUnorderedAccessView(mReflectionMapMaskUAV);
	trace_mfxReflectionMapMaskSampleLevel->SetInt(max(REFLECTION_MAP_MASK_SAMPLE_LEVEL, 1));
	trace_mfxSvoRootNodeCenterPosW->SetFloatVector(reinterpret_cast<float*>(&mSvoRootNodeCenterPosW));
	trace_mfxSvoMinAveragingLevelIdx->SetInt(SVO_MIN_AVG_LEVEL_IDX);
	trace_mfxClientH->SetFloat(mClientH);
	trace_mfxClientW->SetFloat(mClientW);
	trace_mfxSvoRayCastLevelIdx->SetInt(RAY_CASTER_MAX_LEVEL_IDX);
	trace_mfxEyePosW->SetFloatVector(reinterpret_cast<float*>(&(mCamera.GetPosition())));
	trace_mfxInvProj->SetMatrix(reinterpret_cast<float*>(&XMMatrixInverse(&XMMatrixDeterminant(mCamera.Proj()), mCamera.Proj())));
	trace_mfxInvView->SetMatrix(reinterpret_cast<float*>(&XMMatrixInverse(&XMMatrixDeterminant(mCamera.View()), mCamera.View())));
	trace_mfxNormalDepthMapSRV->SetResource(mNormalDepthMapSRV);
	
	D3DX11_TECHNIQUE_DESC techDesc;
	mCastReflectionRaysTech->GetDesc(&techDesc);

	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		mCastReflectionRaysTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
		md3dImmediateContext->Dispatch(ceil(mClientW / RAY_CASTER_NUM_THREADS), ceil(mClientH / RAY_CASTER_NUM_THREADS), 1);
	}
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	md3dImmediateContext->CSSetShader(0, 0, 0);
}

void SvoPipeline::SvoTraceReflectionCones()
{
	md3dImmediateContext->CopyResource(mRayCastedOutputCopy, mRayCastedOutput);
	
	trace_mfxNodesPoolSRV->SetResource(mNodesPoolSRV);
	trace_mfxVoxelsPoolSRV->SetResource(mVoxelsPoolSRV);
	trace_mfxVoxelsFlagsSRV->SetResource(mVoxelsFlagsSRV);
	trace_mfxRayCastedOutputUAV->SetUnorderedAccessView(mRayCastedOutputUAV);
	trace_mfxReflectionMapSRV->SetResource(mRayCastedOutputCopySRV);
	trace_mfxEnvironmentMapSRV->SetResource(mEnvironmentMapSRV);
	trace_mfxSvoRootNodeCenterPosW->SetFloatVector(reinterpret_cast<float*>(&mSvoRootNodeCenterPosW));
	trace_mfxSvoMinAveragingLevelIdx->SetInt(SVO_MIN_AVG_LEVEL_IDX);
	trace_mfxClientH->SetFloat(mClientH);
	trace_mfxClientW->SetFloat(mClientW);
	trace_mfxSvoRayCastLevelIdx->SetInt(RAY_CASTER_MAX_LEVEL_IDX);
	trace_mfxEyePosW->SetFloatVector(reinterpret_cast<float*>(&(mCamera.GetPosition())));
	trace_mfxInvProj->SetMatrix(reinterpret_cast<float*>(&XMMatrixInverse(&XMMatrixDeterminant(mCamera.Proj()), mCamera.Proj())));
	trace_mfxInvView->SetMatrix(reinterpret_cast<float*>(&XMMatrixInverse(&XMMatrixDeterminant(mCamera.View()), mCamera.View())));
	trace_mfxNormalDepthMapSRV->SetResource(mNormalDepthMapSRV);
	trace_mfxReflectionMapMaskSampleLevel->SetInt(max(REFLECTION_MAP_MASK_SAMPLE_LEVEL, 1));
	trace_mfxConeTheta->SetFloat(CONE_THETA);
	trace_mfxMaxStepMultiplier->SetFloat(MAX_STEP_MULTIPLIER);
	trace_mfxMinStepMultiplier->SetFloat(MIN_STEP_MULTIPLIER);
	trace_mfxWeightMultiplier->SetFloat(WEIGHT_MULTIPLIER);
	trace_mfxAlphaClampValue->SetFloat(ALPHA_CLAMP_VALUE);
	trace_mfxAlphaMultiplier->SetFloat(ALPHA_MULTIPLIER);
	trace_mfxEnvironmentMapMipsCount->SetFloat(ENV_MAP_MIP_LEVELS_COUNT);
	trace_mfxMaxIterCount->SetInt(MAX_ITER_COUNT);

	D3DX11_TECHNIQUE_DESC techDesc;
	mTraceReflectionConesTech->GetDesc(&techDesc);

	for (UINT p = 0; p < techDesc.Passes; ++p)
	{
		mTraceReflectionConesTech->GetPassByIndex(p)->Apply(0, md3dImmediateContext);
		md3dImmediateContext->Dispatch(ceil(mClientW / RAY_CASTER_NUM_THREADS), ceil(mClientH / RAY_CASTER_NUM_THREADS), 1);
	}
	md3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNullUAV, 0);
	md3dImmediateContext->CSSetShaderResources(0, 1, pNullSRV);
	md3dImmediateContext->CSSetShader(0, 0, 0);
}

void SvoPipeline::BuildFX()
{
	//SVO building effect variables
	build_mfxSVOViewProjMatrices = mBuildSvoFX->GetVariableByName("gSVOViewProj");
	build_mfxSvoRootNodeCenterPosW = mBuildSvoFX->GetVariableByName("gSvoRootNodeCenterPosW")->AsVector();
	build_mfxNodesPoolUAV = mBuildSvoFX->GetVariableByName("gNodesPoolRW")->AsUnorderedAccessView();
	build_mfxVoxelsPoolUAV = mBuildSvoFX->GetVariableByName("gVoxelsPoolRW")->AsUnorderedAccessView();
	build_mfxVoxelsFlagsUAV = mBuildSvoFX->GetVariableByName("gVoxelsFlagsRW")->AsUnorderedAccessView();
	build_mfxNodesPoolSRV = mBuildSvoFX->GetVariableByName("gNodesPoolRO")->AsShaderResource();
	build_mfxVoxelsPoolSRV = mBuildSvoFX->GetVariableByName("gVoxelsPoolRO")->AsShaderResource();
	build_mfxSvoMinAveragingLevelIdx = mBuildSvoFX->GetVariableByName("gSvoMinAveragingLevelIdx")->AsScalar();

	//SVO tracing effect variables
	mSvoTraceRaysFX = d3dHelper::CreateEffectFromMemory(L"FX/SVOTraceRays.cso", md3dDevice);
	trace_mfxClientW = mSvoTraceRaysFX->GetVariableByName("gClientW")->AsScalar();
	trace_mfxClientH = mSvoTraceRaysFX->GetVariableByName("gClientH")->AsScalar();
	trace_mfxConeTheta = mSvoTraceRaysFX->GetVariableByName("gConeTheta")->AsScalar();
	trace_mfxAlphaClampValue = mSvoTraceRaysFX->GetVariableByName("gAlphaClampValue")->AsScalar();
	trace_mfxMaxStepMultiplier = mSvoTraceRaysFX->GetVariableByName("gMaxStepMultiplier")->AsScalar();
	trace_mfxMinStepMultiplier = mSvoTraceRaysFX->GetVariableByName("gMinStepMultiplier")->AsScalar();
	trace_mfxWeightMultiplier = mSvoTraceRaysFX->GetVariableByName("gWeightMultiplier")->AsScalar();
	trace_mfxAlphaMultiplier = mSvoTraceRaysFX->GetVariableByName("gAlphaMultiplier")->AsScalar();
	trace_mfxEnvironmentMapMipsCount = mSvoTraceRaysFX->GetVariableByName("gEnvironmentMapMipsCount")->AsScalar();
	trace_mfxMaxIterCount = mSvoTraceRaysFX->GetVariableByName("gMaxIterCount")->AsScalar();
	trace_mfxReflectionMapMaskSampleLevel = mSvoTraceRaysFX->GetVariableByName("gReflectionMapMaskSampleLevel")->AsScalar();
	trace_mfxSvoMinAveragingLevelIdx = mSvoTraceRaysFX->GetVariableByName("gSvoMinAveragingLevelIdx")->AsScalar();
	trace_mfxSvoRayCastLevelIdx = mSvoTraceRaysFX->GetVariableByName("gSvoRayCastLevelIdx")->AsScalar();
	trace_mfxEyePosW = mSvoTraceRaysFX->GetVariableByName("gEyePosW")->AsVector();
	trace_mfxInvProj = mSvoTraceRaysFX->GetVariableByName("gInvProj")->AsMatrix();
	trace_mfxInvView = mSvoTraceRaysFX->GetVariableByName("gInvView")->AsMatrix();
	trace_mfxSvoRootNodeCenterPosW = mSvoTraceRaysFX->GetVariableByName("gSvoRootNodeCenterPosW")->AsVector();
	trace_mfxNodesPoolUAV = mSvoTraceRaysFX->GetVariableByName("gNodesPoolRW")->AsUnorderedAccessView();
	trace_mfxRayCastedOutputUAV = mSvoTraceRaysFX->GetVariableByName("gRayCastedOutputRW")->AsUnorderedAccessView();
	trace_mfxReflectionMapMaskUAV = mSvoTraceRaysFX->GetVariableByName("gReflectionMapMaskRW")->AsUnorderedAccessView();
	trace_mfxReflectionMapUAV = mSvoTraceRaysFX->GetVariableByName("gReflectionMapRW")->AsUnorderedAccessView();
	trace_mfxNodesPoolSRV = mSvoTraceRaysFX->GetVariableByName("gNodesPoolRO")->AsShaderResource();
	trace_mfxVoxelsPoolSRV = mSvoTraceRaysFX->GetVariableByName("gVoxelsPoolRO")->AsShaderResource();
	trace_mfxVoxelsFlagsSRV = mSvoTraceRaysFX->GetVariableByName("gVoxelsFlagsRO")->AsShaderResource();
	trace_mfxNormalDepthMapSRV = mSvoTraceRaysFX->GetVariableByName("gNormalDepthMap")->AsShaderResource();
	trace_mfxReflectionMapSRV = mSvoTraceRaysFX->GetVariableByName("gReflectionMapRO")->AsShaderResource();
	trace_mfxEnvironmentMapSRV = mSvoTraceRaysFX->GetVariableByName("gEnvironmentMap")->AsShaderResource();
	mResetRootNodeTech = mSvoTraceRaysFX->GetTechniqueByName("ResetRootNodeTech");
	mCastRaysTech = mSvoTraceRaysFX->GetTechniqueByName("CastRaysTech");
	mCastReflectionRaysTech = mSvoTraceRaysFX->GetTechniqueByName("CastReflectionRaysTech");
	mTraceReflectionConesTech = mSvoTraceRaysFX->GetTechniqueByName("TraceReflectionConesTech");

	//SVO debugging visualization effect variables
	mSvoDebugDrawTech = mBuildSvoFX->GetTechniqueByName("SvoDebugDrawTech");
	mfxViewProj = mBuildSvoFX->GetVariableByName("gViewProj")->AsMatrix();
}

void SvoPipeline::BuildBuffers()
{
	///Utility 64x64 render target view//////////////////////////////////////////////////////////////////////////////////////
	D3D11_TEXTURE2D_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(texDesc));
	texDesc.Width = SVO_BUILD_RT_SZ;
	texDesc.Height = SVO_BUILD_RT_SZ;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	ID3D11Texture2D* Tex = 0;
	HR(md3dDevice->CreateTexture2D(&texDesc, 0, &Tex));
	HR(md3dDevice->CreateRenderTargetView(Tex, 0, &m64x64RT));
	ReleaseCOM(Tex);
	///Nodes pool buffer views///////////////////////////////////////////////////////////////////////////////////////////////
	D3D11_BUFFER_DESC nodespooldesc;
	ZeroMemory(&nodespooldesc, sizeof(nodespooldesc));
	nodespooldesc.Usage = D3D11_USAGE_DEFAULT;
	nodespooldesc.ByteWidth = sizeof(SvoNode) * NODES_COUNT;
	nodespooldesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	nodespooldesc.CPUAccessFlags = 0;
	nodespooldesc.StructureByteStride = sizeof(SvoNode);
	nodespooldesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	HR(md3dDevice->CreateBuffer(&nodespooldesc, NULL, &mNodesPool));

	D3D11_UNORDERED_ACCESS_VIEW_DESC nodespooluavdesc;
	ZeroMemory(&nodespooluavdesc, sizeof(nodespooluavdesc));
	nodespooluavdesc.Format = DXGI_FORMAT_UNKNOWN;
	nodespooluavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	nodespooluavdesc.Buffer.FirstElement = 0;
	nodespooluavdesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
	nodespooluavdesc.Buffer.NumElements = NODES_COUNT;
	HR(md3dDevice->CreateUnorderedAccessView(mNodesPool, &nodespooluavdesc, &mNodesPoolUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC nodespoolsrvdesc;
	ZeroMemory(&nodespoolsrvdesc, sizeof(nodespoolsrvdesc));
	nodespoolsrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	nodespoolsrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	nodespoolsrvdesc.Buffer.ElementWidth = NODES_COUNT;
	HR(md3dDevice->CreateShaderResourceView(mNodesPool, &nodespoolsrvdesc, &mNodesPoolSRV));
	///Nodes pool cpu-accessible copy for debug///////////////////////////////////////////////////////////////////////////
	/*D3D11_BUFFER_DESC nodespoolcopydesc;
	nodespoolcopydesc.Usage = D3D11_USAGE_STAGING;
	nodespoolcopydesc.ByteWidth = sizeof(SvoNode) * NODES_COUNT;
	nodespoolcopydesc.BindFlags = 0;
	nodespoolcopydesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	nodespoolcopydesc.StructureByteStride = sizeof(SvoNode);
	nodespoolcopydesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	HR(md3dDevice->CreateBuffer(&nodespoolcopydesc, NULL, &mNodesPoolCopy));
	///Nodes pool copy for debug visualization////////////////////////////////////////////////////////////////////////////
	D3D11_BUFFER_DESC nodespoolviewdesc;
	nodespoolviewdesc.Usage = D3D11_USAGE_DYNAMIC;
	nodespoolviewdesc.ByteWidth = sizeof(SvoDebugDrawVsIn) * NODES_COUNT;
	nodespoolviewdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	nodespoolviewdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	nodespoolviewdesc.StructureByteStride = sizeof(SvoDebugDrawVsIn);
	nodespoolviewdesc.MiscFlags = 0;
	HR(md3dDevice->CreateBuffer(&nodespoolviewdesc, NULL, &mNodesPoolDebugDraw));*/
	///Voxels flags buffer views///////////////////////////////////////////////////////////////////////////////////////
	D3D11_BUFFER_DESC voxelsflagsdesc;
	ZeroMemory(&voxelsflagsdesc, sizeof(voxelsflagsdesc));
	voxelsflagsdesc.Usage = D3D11_USAGE_DEFAULT;
	voxelsflagsdesc.ByteWidth = sizeof(UINT) * VOXELS_COUNT;
	voxelsflagsdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	voxelsflagsdesc.CPUAccessFlags = 0;
	voxelsflagsdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	voxelsflagsdesc.StructureByteStride = sizeof(UINT);
	HR(md3dDevice->CreateBuffer(&voxelsflagsdesc, NULL, &mVoxelsFlags));

	D3D11_UNORDERED_ACCESS_VIEW_DESC voxelsflagsuavdesc;
	ZeroMemory(&voxelsflagsuavdesc, sizeof(voxelsflagsuavdesc));
	voxelsflagsuavdesc.Format = DXGI_FORMAT_UNKNOWN;
	voxelsflagsuavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	voxelsflagsuavdesc.Buffer.FirstElement = 0;
	voxelsflagsuavdesc.Buffer.Flags = 0;
	voxelsflagsuavdesc.Buffer.NumElements = VOXELS_COUNT;
	HR(md3dDevice->CreateUnorderedAccessView(mVoxelsFlags, &voxelsflagsuavdesc, &mVoxelsFlagsUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC voxelsflagssrvdesc;
	ZeroMemory(&voxelsflagssrvdesc, sizeof(voxelsflagssrvdesc));
	voxelsflagssrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	voxelsflagssrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	voxelsflagssrvdesc.Buffer.ElementWidth = VOXELS_COUNT;
	HR(md3dDevice->CreateShaderResourceView(mVoxelsFlags, &voxelsflagssrvdesc, &mVoxelsFlagsSRV));
	///Voxels pool buffer views////////////////////////////////////////////////////////////////////////////////////////
	D3D11_BUFFER_DESC voxelspooldesc;
	ZeroMemory(&voxelspooldesc, sizeof(voxelspooldesc));
	voxelspooldesc.Usage = D3D11_USAGE_DEFAULT;
	voxelspooldesc.ByteWidth = sizeof(SvoVoxel) * VOXELS_COUNT;
	voxelspooldesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	voxelspooldesc.CPUAccessFlags = 0;
	voxelspooldesc.StructureByteStride = sizeof(SvoVoxel);
	voxelspooldesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	HR(md3dDevice->CreateBuffer(&voxelspooldesc, NULL, &mVoxelsPool));

	D3D11_UNORDERED_ACCESS_VIEW_DESC voxelspooluavdesc;
	ZeroMemory(&voxelspooluavdesc, sizeof(voxelspooluavdesc));
	voxelspooluavdesc.Format = DXGI_FORMAT_UNKNOWN;
	voxelspooluavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	voxelspooluavdesc.Buffer.FirstElement = 0;
	voxelspooluavdesc.Buffer.Flags = 0;
	voxelspooluavdesc.Buffer.NumElements = VOXELS_COUNT;
	HR(md3dDevice->CreateUnorderedAccessView(mVoxelsPool, &voxelspooluavdesc, &mVoxelsPoolUAV));

	D3D11_SHADER_RESOURCE_VIEW_DESC voxelspoolsrvdesc;
	ZeroMemory(&voxelspoolsrvdesc, sizeof(voxelspoolsrvdesc));
	voxelspoolsrvdesc.Format = DXGI_FORMAT_UNKNOWN;
	voxelspoolsrvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	voxelspoolsrvdesc.Buffer.ElementWidth = VOXELS_COUNT;
	HR(md3dDevice->CreateShaderResourceView(mVoxelsPool, &voxelspoolsrvdesc, &mVoxelsPoolSRV));
}

void SvoPipeline::BuildViews()
{
	D3D11_TEXTURE2D_DESC rayCastOutTexDesc;
	ZeroMemory(&rayCastOutTexDesc, sizeof(rayCastOutTexDesc));
	rayCastOutTexDesc.Width = mClientW;
	rayCastOutTexDesc.Height = mClientH;
	rayCastOutTexDesc.MipLevels = max(REFLECTION_MAP_MASK_SAMPLE_LEVEL, 1) + 1;
	rayCastOutTexDesc.ArraySize = 1;
	rayCastOutTexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	rayCastOutTexDesc.SampleDesc.Count = 1;
	rayCastOutTexDesc.SampleDesc.Quality = 0;
	rayCastOutTexDesc.Usage = D3D11_USAGE_DEFAULT;
	rayCastOutTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	rayCastOutTexDesc.CPUAccessFlags = 0;
	rayCastOutTexDesc.MiscFlags = 0;

	HR(md3dDevice->CreateTexture2D(&rayCastOutTexDesc, 0, &mRayCastedOutput));
	HR(md3dDevice->CreateShaderResourceView(mRayCastedOutput, 0, &mRayCastedOutputSRV));
	HR(md3dDevice->CreateUnorderedAccessView(mRayCastedOutput, 0, &mRayCastedOutputUAV));
	
	D3D11_UNORDERED_ACCESS_VIEW_DESC reflectionMapMaskUavDesc;
	ZeroMemory(&reflectionMapMaskUavDesc, sizeof(reflectionMapMaskUavDesc));
	reflectionMapMaskUavDesc.Format = DXGI_FORMAT_UNKNOWN;
	reflectionMapMaskUavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	reflectionMapMaskUavDesc.Texture2D.MipSlice = max(REFLECTION_MAP_MASK_SAMPLE_LEVEL, 1);

	HR(md3dDevice->CreateUnorderedAccessView(mRayCastedOutput, &reflectionMapMaskUavDesc, &mReflectionMapMaskUAV));
	
	D3D11_UNORDERED_ACCESS_VIEW_DESC reflectionMapUavDesc;
	ZeroMemory(&reflectionMapUavDesc, sizeof(reflectionMapUavDesc));
	reflectionMapUavDesc.Format = DXGI_FORMAT_UNKNOWN;
	reflectionMapUavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	reflectionMapUavDesc.Texture2D.MipSlice = 0;

	HR(md3dDevice->CreateUnorderedAccessView(mRayCastedOutput, &reflectionMapUavDesc, &mReflectionMapUAV));

	D3D11_TEXTURE2D_DESC rayCastOutCopyTexDesc;
	ZeroMemory(&rayCastOutCopyTexDesc, sizeof(rayCastOutCopyTexDesc));
	rayCastOutCopyTexDesc.Width = mClientW;
	rayCastOutCopyTexDesc.Height = mClientH;
	rayCastOutCopyTexDesc.MipLevels = max(REFLECTION_MAP_MASK_SAMPLE_LEVEL, 1) + 1;
	rayCastOutCopyTexDesc.ArraySize = 1;
	rayCastOutCopyTexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	rayCastOutCopyTexDesc.SampleDesc.Count = 1;
	rayCastOutCopyTexDesc.SampleDesc.Quality = 0;
	rayCastOutCopyTexDesc.Usage = D3D11_USAGE_DEFAULT;
	rayCastOutCopyTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	rayCastOutCopyTexDesc.CPUAccessFlags = 0;
	rayCastOutCopyTexDesc.MiscFlags = 0;

	HR(md3dDevice->CreateTexture2D(&rayCastOutCopyTexDesc, 0, &mRayCastedOutputCopy));
	HR(md3dDevice->CreateShaderResourceView(mRayCastedOutputCopy, 0, &mRayCastedOutputCopySRV));

	D3D11_SHADER_RESOURCE_VIEW_DESC reflectionMapMaskSrvDesc;
	ZeroMemory(&reflectionMapMaskSrvDesc, sizeof(reflectionMapMaskSrvDesc));
	reflectionMapMaskSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	reflectionMapMaskSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	reflectionMapMaskSrvDesc.Texture2D.MipLevels = 1;
	reflectionMapMaskSrvDesc.Texture2D.MostDetailedMip = max(REFLECTION_MAP_MASK_SAMPLE_LEVEL, 1);
	HR(md3dDevice->CreateShaderResourceView(mRayCastedOutputCopy, &reflectionMapMaskSrvDesc, &mReflectionMapMaskSRV));

	D3D11_SHADER_RESOURCE_VIEW_DESC reflectionMapSrvDesc;
	ZeroMemory(&reflectionMapSrvDesc, sizeof(reflectionMapSrvDesc));
	reflectionMapSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	reflectionMapSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	reflectionMapSrvDesc.Texture2D.MipLevels = 1;
	reflectionMapSrvDesc.Texture2D.MostDetailedMip = 0;
	HR(md3dDevice->CreateShaderResourceView(mRayCastedOutputCopy, &reflectionMapSrvDesc, &mReflectionMapSRV));
}

void SvoPipeline::BuildCamera()
{
	//back-to-front
	XMVECTOR eyePosBF = mCamera.GetPositionXM() + XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f) * (SVO_ROOT_NODE_SZ / 2.0 + mCamera.GetNearZ());
	//right-to-left
	XMVECTOR eyePosRL = mCamera.GetPositionXM() + XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f) * (SVO_ROOT_NODE_SZ / 2.0 + mCamera.GetNearZ());
	//top-to-bottom
	XMVECTOR eyePosTB = mCamera.GetPositionXM() + XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f) * (SVO_ROOT_NODE_SZ / 2.0 + mCamera.GetNearZ());
	//front-to-back
	XMVECTOR eyePosFB = mCamera.GetPositionXM() + XMVectorSet(0.0f, 0.0f, -1.0f, 1.0f) * (SVO_ROOT_NODE_SZ / 2.0 + mCamera.GetNearZ());
	//left-to-right
	XMVECTOR eyePosLR = mCamera.GetPositionXM() + XMVectorSet(-1.0f, 0.0f, 0.0f, 1.0f) * (SVO_ROOT_NODE_SZ / 2.0 + mCamera.GetNearZ());
	//bottom-to-top
	XMVECTOR eyePosBT = mCamera.GetPositionXM() + XMVectorSet(0.0f, -1.0f, 0.0f, 1.0f) * (SVO_ROOT_NODE_SZ / 2.0 + mCamera.GetNearZ());

	XMMATRIX ViewBF = XMMatrixLookAtLH(eyePosBF, mCamera.GetPositionXM(), XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
	XMMATRIX ViewRL = XMMatrixLookAtLH(eyePosRL, mCamera.GetPositionXM(), XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
	XMMATRIX ViewTB = XMMatrixLookAtLH(eyePosTB, mCamera.GetPositionXM(), XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f));
	XMMATRIX ViewFB = XMMatrixLookAtLH(eyePosFB, mCamera.GetPositionXM(), XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
	XMMATRIX ViewLR = XMMatrixLookAtLH(eyePosLR, mCamera.GetPositionXM(), XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
	XMMATRIX ViewBT = XMMatrixLookAtLH(eyePosBT, mCamera.GetPositionXM(), XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f));

	XMMATRIX P = XMMatrixOrthographicLH(SVO_ROOT_NODE_SZ, SVO_ROOT_NODE_SZ, mCamera.GetNearZ(), mCamera.GetNearZ() + SVO_ROOT_NODE_SZ);

	mSVOViewProjMatrices[0] = ViewBF * P;
	mSVOViewProjMatrices[1] = ViewRL * P;
	mSVOViewProjMatrices[2] = ViewTB * P;
	mSVOViewProjMatrices[3] = ViewFB * P;
	mSVOViewProjMatrices[4] = ViewLR * P;
	mSVOViewProjMatrices[5] = ViewBT * P;

	mSvoRootNodeCenterPosW = mCamera.GetPosition();
}

void SvoPipeline::BuildViewport()
{
	InitViewport(&m64x64RTViewport, 0.0f, 0.0f, SVO_BUILD_RT_SZ, SVO_BUILD_RT_SZ, 0.0f, 1.0f);
}

void SvoPipeline::BuildInputLayout()
{
	D3DX11_PASS_DESC passDesc;
	static const D3D11_INPUT_ELEMENT_DESC SvoNodeDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "LEVEL_INDEX", 0, DXGI_FORMAT_R32_SINT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	mSvoDebugDrawTech->GetPassByIndex(0)->GetDesc(&passDesc);
	HR(md3dDevice->CreateInputLayout(SvoNodeDesc, 2, passDesc.pIAInputSignature,
		passDesc.IAInputSignatureSize, &(mSvoDebugDrawInputLayout)));
}