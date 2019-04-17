#include "d3dUtil.h"
#include <d2d1.h>
#include <dwrite.h>


class Text{
public:
	Text();
	~Text();

	void InitAll(HWND *hwnd, IDXGISurface* pSurface);

	void Draw(HWND *hwnd, IDXGISurface* pSurface);

private:
	IDWriteFactory* pDWriteFactory;
	IDWriteTextFormat* pTextFormat;

	const wchar_t* TextW;
	UINT TextLength;

	ID2D1Factory* pD2DFactory;
	ID2D1RenderTarget* pRenderTarget;
	ID2D1SolidColorBrush* pBrush;

	D2D1_RECT_F layoutRect;
};

Text::Text() : pDWriteFactory(0), pTextFormat(0), pD2DFactory(0), pRenderTarget(0), pBrush(0)
{

}

Text::~Text()
{

}

void Text::Draw(HWND *hwnd, IDXGISurface* pSurface)
{
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);

	DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(&pDWriteFactory)
		);

	TextW = L"HelloWorld!";
	TextLength = wcslen(TextW);

	pDWriteFactory->CreateTextFormat(L"Gabriola", NULL, DWRITE_FONT_WEIGHT_REGULAR,
		DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 72.0f, L"en-ru", &pTextFormat);

	pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

	RECT rc;
	GetClientRect(*hwnd, &rc);
	D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

	pD2DFactory->CreateDxgiSurfaceRenderTarget(pSurface, D2D1::RenderTargetProperties(), &pRenderTarget);

	pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pBrush);

	layoutRect = D2D1::RectF(100, 100, 1900, 1000);

	pRenderTarget->BeginDraw();
	
	pRenderTarget->SetTransform(D2D1::IdentityMatrix());
	
	pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::LightSteelBlue));

	pRenderTarget->DrawTextW(TextW, TextLength, pTextFormat, layoutRect, pBrush);

	pRenderTarget->EndDraw();
}