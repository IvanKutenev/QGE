#ifndef _GUI_H_
#define _GUI_H_

#include "../Common/d3dUtil.h"

enum ButtonType {RECTANGULAR, ELLIPSOID};
enum BrushType {LINEAR_GRADIENT, RADIAL_GRADIENT, SOLID_COLOR};

static bool EndInit = false;

using namespace std;

struct FontInit {
	std::wstring FontFamilyName;
	IDWriteFontCollection* FontCollection;
	DWRITE_FONT_WEIGHT FontWeight;
	DWRITE_FONT_STYLE FontStyle;
	DWRITE_FONT_STRETCH FontStretch;
	FLOAT fontSize;
	std::wstring FontLocaleName;
};

struct Font {
	std::wstring FontFamilyName;
	IDWriteFontCollection* FontCollection;
	DWRITE_FONT_WEIGHT FontWeight;
	DWRITE_FONT_STYLE FontStyle;
	DWRITE_FONT_STRETCH FontStretch;
	FLOAT fontSize;
	std::wstring FontLocaleName;
	IDWriteTextFormat* FontTextFormat;
};

struct Alignment {
	DWRITE_TEXT_ALIGNMENT FontTextAlignment;
	DWRITE_PARAGRAPH_ALIGNMENT FontTextParagraphAligment;
};

struct BrushInit {
	D2D1_COLOR_F BrushColor;
	BrushType BrushStyle;
};

struct Brush {
	D2D1_COLOR_F BrushColor;
	BrushType BrushStyle;
	ID2D1SolidColorBrush* SolidColorBrush;
	ID2D1LinearGradientBrush* LinearGradientBrush;
	ID2D1RadialGradientBrush* RadialGradientBrush;
};

struct Button {
	std::wstring ButtonCaption;
	FLOAT ButtonHeight;
	FLOAT ButtonWidth;
	ButtonType ButtonShape;
	XMFLOAT2 ButtonPosition;
	Alignment ButtonAlignment;
	Brush ButtonBrush;
	Font ButtonFont;

	D2D1_RECT_F ButtonRect;
	D2D1_ELLIPSE ButtonEllipse;

	bool Scrollable;
	bool Activated;

	std::wstring Name;
};

struct TextBox {
	std::wstring TextBoxCaption;
	FLOAT TextBoxHeight;
	FLOAT TextBoxWidth;
	XMFLOAT2 TextBoxPosition;
	Alignment TextBoxAlignment;
	Brush TextBoxBrush;
	Font TextBoxFont;

	D2D1_RECT_F TextBoxRect;

	bool Scrollable;

	std::wstring Name;
};

struct RadioButton
{
	std::wstring RadioButtonCaption;
	FLOAT RadioButtonHeight;
	FLOAT RadioButtonWidth;
	ButtonType RadioButtonShape;
	XMFLOAT2 RadioButtonPosition;
	Alignment RadioButtonAlignment;

	Brush NOT_PRESSED_RadioButtonBrush;
	Font NOT_PRESSED_RadioButtonFont;

	Brush PRESSED_RadioButtonBrush;
	Font PRESSED_RadioButtonFont;

	D2D1_RECT_F NOT_PRESSED_RadioButtonRect;
	D2D1_ELLIPSE NOT_PRESSED_RadioButtonEllipse;

	D2D1_RECT_F NOT_PRESSED_RadioButtonTextRect;

	D2D1_RECT_F PRESSED_RadioButtonRect;
	D2D1_ELLIPSE PRESSED_RadioButtonEllipse;

	D2D1_RECT_F PRESSED_RadioButtonTextRect;

	bool Pressed;

	bool Scrollable;
	bool Activated;
	
	std::wstring Name;
};

class GUI {
public:

	GUI(ID2D1RenderTarget*, FLOAT ClientH, FLOAT ClientW)
	{
		InitClientH = ClientH;
		InitClientW = ClientW;

		CurClientH = ClientH;
		CurClientW = ClientW;
	};
	~GUI(void) {};
	void DrawGUI(ID2D1RenderTarget*);
	
	void MessageProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void CreateButton(ID2D1RenderTarget*, std::wstring Caption, FLOAT Height, FLOAT Width, ButtonType Shape, XMFLOAT2 Position,
		Alignment ButtonAlign, BrushInit ButtonBrush, FontInit ButtonFont, bool Scrollable, bool Activated, std::wstring Name);

	void CreateTextBox(ID2D1RenderTarget*, std::wstring Caption, FLOAT Height, FLOAT Width, XMFLOAT2 Position,
		Alignment TextBoxAlign, BrushInit TextBoxBrush, FontInit TextBoxFont, bool Scrollable, std::wstring Name);

	void CreateRadioButton(ID2D1RenderTarget*, std::wstring Caption, FLOAT Height, FLOAT Width, ButtonType Shape, XMFLOAT2 Position,
		Alignment RadioButtonAlign, BrushInit not_pressed_RadioButtonBrush, FontInit not_pressed_RadioButtonFont,
		BrushInit pressed_RadioButtonBrush, FontInit pressed_RadioButtonFont, bool Scrollable, bool Activated, std::wstring Name);

	void UpdateTextBox(std::wstring Caption, std::wstring ID);
	bool GetRadioButtonState(std::wstring ID) const;

	void ActivateButton(std::wstring ID);
	void DeactivateButton(std::wstring ID);

	void OnSize(FLOAT ClientH, FLOAT ClientW);

	std::vector<std::wstring>ActionQueue;

private:
	FLOAT InitClientH, InitClientW;
	FLOAT CurClientH, CurClientW;

	void ScrollElements(FLOAT WheelDelta);

	//Buttons
	void DrawButton(ID2D1RenderTarget*, UINT ButtonIndex);
	void DeleteButton(UINT ButtonIndex) { buttons.erase(buttons.begin() + ButtonIndex); };
	void DrawButtons(ID2D1RenderTarget*);
	void ScrollButton(UINT ButtonIndex, FLOAT WheelDelta);
	std::vector<Button>buttons;

	//Text boxes
	void DrawTextBox(ID2D1RenderTarget*, UINT TextBoxindex);
	void DeleteTextBox(UINT TextBoxIndex) { textBoxes.erase(textBoxes.begin() + TextBoxIndex); };
	void DrawTextBoxes(ID2D1RenderTarget*);
	void ScrollTextBox(UINT TextBoxIndex, FLOAT WheelDelta);
	std::vector<TextBox>textBoxes;

	//Radio buttons
	void DrawRadioButton(ID2D1RenderTarget*, UINT RadioButtonIndex);
	void DeleteRadioButton(UINT RadioButtonIndex) { radioButtons.erase(radioButtons.begin() + RadioButtonIndex); };
	void DrawRadioButtons(ID2D1RenderTarget*);
	void ScrollRadioButton(UINT RadioButtonIndex, FLOAT WheelDelta);
	std::vector<RadioButton>radioButtons;

	//Helper functions
	bool IsPointInEllipse(FLOAT radius_X, FLOAT radius_Y, XMFLOAT2 CenterPos, XMFLOAT2 PointPos) const;
	bool IsPointInRect(D2D1_RECT_F Rect, XMFLOAT2 PointPos) const;

	static const int ScrollScale = 10;
};
#endif