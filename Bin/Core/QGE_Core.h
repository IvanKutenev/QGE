#ifndef _QGE_CORE_H_
#define _QGE_CORE_H_

#include "d3dUtil.h"
#include "GameTimer.h"
#include "../Bin/GUI/GUI.h"

class QGE_Core
{
private:
	bool InitD2D_D3D101_DWrite(IDXGIAdapter1 *Adapter);
	void InitD2DScreenTexture();
	bool InitMainWindow();
	bool InitDirect3D();
	void CalculateFrameStats();
public:
	//Engine life cycle
	QGE_Core(HINSTANCE hInstance);
	virtual ~QGE_Core();
	virtual bool Init();
	int Run();
	//WinAPI application managing
	HINSTANCE AppInst() const;
	//WinAPI window message processing
	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	//Application window managing
	virtual void OnResize();
	HWND MainWnd() const;
	float AspectRatio() const;
	//3D graphics processing
	virtual void UpdateScene(float dt) = 0;
	virtual void DrawScene() = 0; 
	//Mouse input
	virtual void OnMouseDown(WPARAM btnState, int x, int y){ }
	virtual void OnMouseUp(WPARAM btnState, int x, int y)  { }
	virtual void OnMouseMove(WPARAM btnState, int x, int y){ }
	//2D GUI processing
	virtual void DrawGUI() = 0;
protected:
	//General application WinAPI parameters
	HINSTANCE mhAppInst;
	HWND      mhMainWnd;
	bool      mAppPaused;
	bool      mMinimized;
	bool      mMaximized;
	bool      mResizing;
	UINT      m4xMsaaQuality;
	std::wstring mMainWndCaption;
	std::wstring AllInfo;
	//Logging
	std::string LogFileName;
	//Timer
	GameTimer mTimer;
	//3D DirectX 11 graphics
	ID3D11Device* md3dDevice;
	ID3D11DeviceContext* md3dImmediateContext;
	IDXGISwapChain* mSwapChain;
	ID3D11Texture2D* mDepthStencilBuffer;
	ID3D11RenderTargetView* mRenderTargetView;
	ID3D11DepthStencilView* mDepthStencilView;
	ID3D11Texture2D* mBackbuffer;
	D3D11_VIEWPORT mScreenViewport;
	D3D_DRIVER_TYPE md3dDriverType;
	int mClientWidth;
	int mClientHeight;
	bool mEnable4xMsaa;
	bool mUpdateInactive;
	//DirectX 10, 11 2D grpahic user interface
	ID3D10Device1 *d3d101Device;
	IDXGIKeyedMutex *keyedMutex11;
	IDXGIKeyedMutex *keyedMutex10;
	ID2D1RenderTarget *D2DRenderTarget;
	ID3D11Texture2D *sharedTex11;
	ID3D11Buffer *d2dVertBuffer;
	ID3D11Buffer *d2dIndexBuffer;
	ID3D11ShaderResourceView *d2dTexture;
	std::wstring TextInfo;
	GUI* pGUI;
};

#endif