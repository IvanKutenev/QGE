#ifndef _RENDER_STATES_H_
#define _RENDER_STATES_H_

#include "../Common/d3dUtil.h"

class RenderStates
{
public:
	RenderStates(ID3D11Device* device);
	~RenderStates();

	static void InitAll(ID3D11Device* device);
	static void DestroyAll();

	// Rasterizer states
	static ID3D11RasterizerState* WireframeRS;
	static ID3D11RasterizerState* NoCullRS;
	static ID3D11RasterizerState* CullClockwiseRS;
	 
	// Blend states
	static ID3D11BlendState* AlphaToCoverageBS;
	static ID3D11BlendState* TransparentBS;
	static ID3D11BlendState* NoRenderTargetWritesBS;
	static ID3D11BlendState* AdditiveBlendBS;
	static ID3D11BlendState* OitBS;
	static ID3D11BlendState* VolumetricBS;

	// Depth/stencil states
	static ID3D11DepthStencilState* MarkMirrorDSS;
	static ID3D11DepthStencilState* DrawReflectionDSS;
	static ID3D11DepthStencilState* NoDoubleBlendDSS;
	static ID3D11DepthStencilState* NoDepthDSS;
	static ID3D11DepthStencilState* NoDepthWritesDSS;
	static ID3D11DepthStencilState* NoDepthStencilTestDSS;
};
#endif