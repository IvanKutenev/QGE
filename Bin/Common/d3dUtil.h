#define _XM_NO_INTRINSICS_

#ifndef _D3DUTIL_H
#define _D3DUTIL_H

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#define _WINSOCKAPI_
#include <windowsx.h>
#include <windows.h>

#include <dxgi1_4.h>
#include <d3d11_4.h>
#include <d3d10_1.h>
#include <d2d1_3.h>
#include "d3dx11Effect.h"
#include <DirectXMath.h> 
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>
#include <cassert>
#include <ctime>
#include <algorithm>
#include <string>
#include <map>
#include <sstream>
#include <fstream>
#include <vector>
#include <utility>
#include <dwrite.h>
#include "MathHelper.h"
#include "LightHelper.h"
#include "DirectXTex.h"

using namespace DirectX;
using namespace PackedVector;

//---------------------------------------------------------------------------------------
// Simple d3d error checker for book demos.
//---------------------------------------------------------------------------------------

#if defined(DEBUG) | defined(_DEBUG)
	#ifndef HR
	#define HR(x)                                              \
	{                                                          \
		HRESULT hr = (x);                                      \
		if(FAILED(hr))                                         \
		{                                                      \
			DXTrace(__FILE__, (DWORD)__LINE__, hr, L#x, true); \
		}                                                      \
	}
	#endif

#else
	#ifndef HR
	#define HR(x) (x)
	#endif
#endif 


//---------------------------------------------------------------------------------------
// Convenience macro for releasing COM objects.
//---------------------------------------------------------------------------------------

#define ReleaseCOM(x) { if(x){ x->Release(); x = 0; } }

//---------------------------------------------------------------------------------------
// Convenience macro for deleting objects.
//---------------------------------------------------------------------------------------

#define SafeDelete(x) { delete x; x = 0; }

//---------------------------------------------------------------------------------------
// Utility classes.
//---------------------------------------------------------------------------------------

class d3dHelper
{
public:
	///<summary>
	/// 
	/// Does not work with compressed formats.
	///</summary>
	static ID3D11ShaderResourceView* CreateTexture2DArraySRVW(
		ID3D11Device* device, ID3D11DeviceContext* context,
		std::vector<std::wstring>filenames);

	static ID3D11ShaderResourceView* CreateTexture2DSRVW(
		ID3D11Device* device, ID3D11DeviceContext* context,
		std::wstring filenames);

	static ID3D11ShaderResourceView* CreateTexture2DArraySRVA(
		ID3D11Device* device, ID3D11DeviceContext* context,
		std::vector<std::string>filenames);

	static ID3D11ShaderResourceView* CreateTexture2DSRVA(
		ID3D11Device* device, ID3D11DeviceContext* context,
		std::string filename);

	static ID3D11ShaderResourceView* CreateRandomTexture1DSRV(ID3D11Device* device);

	static ID3DX11Effect* CreateEffectFromMemory(std::wstring filename, ID3D11Device* device);
};


// Order: left, right, bottom, top, near, far.
void ExtractFrustumPlanes(XMFLOAT4 planes[6], CXMMATRIX M);

#define XMGLOBALCONST extern CONST __declspec(selectany)
//   1. extern so there is only one copy of the variable, and not a separate
//      private copy in each .obj.
//   2. __declspec(selectany) so that the compiler does not complain about
//      multiple definitions in a .cpp file (it can pick anyone and discard 
//      the rest because they are constant--all the same).

namespace Colors
{
	XMGLOBALCONST XMVECTORF32 White     = {1.0f, 1.0f, 1.0f, 1.0f};
	XMGLOBALCONST XMVECTORF32 Black     = {0.0f, 0.0f, 0.0f, 1.0f};
	XMGLOBALCONST XMVECTORF32 Red       = {1.0f, 0.0f, 0.0f, 1.0f};
	XMGLOBALCONST XMVECTORF32 Green     = {0.0f, 1.0f, 0.0f, 1.0f};
	XMGLOBALCONST XMVECTORF32 Blue      = {0.0f, 0.0f, 1.0f, 1.0f};
	XMGLOBALCONST XMVECTORF32 Yellow    = {1.0f, 1.0f, 0.0f, 1.0f};
	XMGLOBALCONST XMVECTORF32 Cyan      = {0.0f, 1.0f, 1.0f, 1.0f};
	XMGLOBALCONST XMVECTORF32 Magenta   = {1.0f, 0.0f, 1.0f, 1.0f};

	XMGLOBALCONST XMVECTORF32 Silver    = {0.75f, 0.75f, 0.75f, 1.0f};
	XMGLOBALCONST XMVECTORF32 LightSteelBlue = {0.69f, 0.77f, 0.87f, 1.0f};

	XMGLOBALCONST XMVECTORF32 Inf = { 1000000.0f, 1000000.0f, 1000000.0f, 1.0f };
}

static const enum DrawPassType
{
	DRAW_PASS_TYPE_COLOR,
	DRAW_PASS_TYPE_NORMAL_DEPTH,
	DRAW_PASS_TYPE_AMBIENT,
	DRAW_PASS_TYPE_DIFFUSE,
	DRAW_PASS_TYPE_SPECULAR,
	DRAW_PASS_TYPE_DEPTH,
	DRAW_PASS_TYPE_POSITION_WS,
	DRAW_PASS_TYPE_COLOR_NO_REFLECTION,
	DRAW_PASS_TYPE_MATERIAL_SPECULAR,
	DRAW_PASS_TYPE_SHADOW_MAP_BUILD,
	DRAW_PASS_TYPE_VOXEL_GRID,
	DRAW_PASS_TYPE_PARTICLES_GRID,
	DRAW_PASS_TYPE_SVO
};

static const enum DrawShadowType
{
	DRAW_SHADOW_TYPE_DIRECTIONAL_LIGHT,
	DRAW_SHADOW_TYPE_SPOT_LIGHT,
	DRAW_SHADOW_TYPE_POINT_LIGHT,
	DRAW_SHADOW_TYPE_NO_SHADOW
};

static const enum DrawSceneType
{
	DRAW_SCENE_TO_PARTICLE_SYSTEM_REFRACTION_RT,
	DRAW_SCENE_TO_PARTICLE_SYSTEM_REFLECTION_RT,
	DRAW_SCENE_TO_WATER_REFRACTION_RT,
	DRAW_SCENE_TO_WATER_REFLECTION_RT,
	DRAW_SCENE_TO_MESH_PLANAR_REFRACTION_RT,
	DRAW_SCENE_TO_MESH_CUBIC_REFRACTION_RT,
	DRAW_SCENE_TO_MESH_CUBIC_REFLECTION_RT,
	DRAW_SCENE_TO_SPH_REFRACTION_RT,
	DRAW_SCENE_TO_SHADOWMAP_CASCADE_RT,
	DRAW_SCENE_TO_DEFAULT_RT
};

struct XMFLOAT2X4X4
{
	XMFLOAT4X4 Matrix_0;
	XMFLOAT4X4 Matrix_1;
};

struct XMFLOAT3X4X4
{
	XMFLOAT4X4 Matrix_0;
	XMFLOAT4X4 Matrix_1;
	XMFLOAT4X4 Matrix_2;
};

struct XMFLOAT2X3X3
{
	XMFLOAT3X3 Matrix_0;
	XMFLOAT3X3 Matrix_1;
};

struct XMFLOAT3X3X3
{
	XMFLOAT3X3 Matrix_0;
	XMFLOAT3X3 Matrix_1;
	XMFLOAT3X3 Matrix_2;
};

const XMFLOAT4 operator+(XMFLOAT4& left, XMFLOAT4& right);

const XMFLOAT4 operator-(XMFLOAT4& left, XMFLOAT4& right);

const XMFLOAT4 operator*(XMFLOAT4& left, XMFLOAT4& right);

const XMFLOAT3 operator+(XMFLOAT3& left, XMFLOAT3& right);

const XMFLOAT3 operator-(XMFLOAT3& left, XMFLOAT3& right);

const XMFLOAT3 operator*(XMFLOAT3& left, XMFLOAT3& right);

const XMFLOAT2 operator*(XMFLOAT2& left, XMFLOAT2& right);

inline void InitViewport(D3D11_VIEWPORT* vp, float toplx, float toply,
	float width, float height, float mind, float maxd)
{
	vp->TopLeftX = toplx;
	vp->TopLeftY = toply;
	vp->Width = width;
	vp->Height = height;
	vp->MinDepth = mind;
	vp->MaxDepth = maxd;
}

static ID3D11RenderTargetView* pNullRTV[1] = { NULL };
static ID3D11UnorderedAccessView* pNullUAV[1] = { NULL };
static ID3D11ShaderResourceView* pNullSRV[1] = { NULL };

#endif // D3DUTIL_H