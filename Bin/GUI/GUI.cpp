#include "GUI.h"

void GUI::CreateButton(ID2D1RenderTarget* D2DRenderTarget, std::wstring Caption, FLOAT Height, FLOAT Width, ButtonType Shape, XMFLOAT2 Position,
	Alignment ButtonAlign, BrushInit ButtonBrush, FontInit ButtonFont, bool Scrollable, bool Activated, std::wstring Name)
{
	Button NewButton;
	NewButton.ButtonCaption = Caption;
	NewButton.ButtonHeight = Height;
	NewButton.ButtonWidth = Width;
	NewButton.ButtonShape = Shape;
	NewButton.ButtonPosition = Position;

	NewButton.ButtonAlignment = ButtonAlign;

	NewButton.ButtonBrush.BrushColor = ButtonBrush.BrushColor;
	NewButton.ButtonBrush.BrushStyle = ButtonBrush.BrushStyle;

	if (NewButton.ButtonBrush.BrushStyle == BrushType::SOLID_COLOR)
		D2DRenderTarget->CreateSolidColorBrush(NewButton.ButtonBrush.BrushColor, &NewButton.ButtonBrush.SolidColorBrush);
	else if (NewButton.ButtonBrush.BrushStyle == BrushType::LINEAR_GRADIENT);
	else if (NewButton.ButtonBrush.BrushStyle == BrushType::RADIAL_GRADIENT);

	NewButton.ButtonFont.FontCollection = ButtonFont.FontCollection;
	NewButton.ButtonFont.FontFamilyName = ButtonFont.FontFamilyName;
	NewButton.ButtonFont.FontLocaleName = ButtonFont.FontLocaleName;
	NewButton.ButtonFont.fontSize = ButtonFont.fontSize;
	NewButton.ButtonFont.FontStretch = ButtonFont.FontStretch;
	NewButton.ButtonFont.FontStyle = ButtonFont.FontStyle;
	NewButton.ButtonFont.FontWeight = ButtonFont.FontWeight;

	IDWriteFactory *DWriteFactory;

	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(&DWriteFactory));

	DWriteFactory->CreateTextFormat(
		NewButton.ButtonFont.FontFamilyName.c_str(),
		NewButton.ButtonFont.FontCollection,
		NewButton.ButtonFont.FontWeight,
		NewButton.ButtonFont.FontStyle,
		NewButton.ButtonFont.FontStretch,
		NewButton.ButtonFont.fontSize,
		NewButton.ButtonFont.FontLocaleName.c_str(),
		&NewButton.ButtonFont.FontTextFormat
		);

	NewButton.ButtonFont.FontTextFormat->SetTextAlignment(NewButton.ButtonAlignment.FontTextAlignment);
	NewButton.ButtonFont.FontTextFormat->SetParagraphAlignment(NewButton.ButtonAlignment.FontTextParagraphAligment);

	NewButton.ButtonRect.left = NewButton.ButtonPosition.x;
	NewButton.ButtonRect.top = NewButton.ButtonPosition.y;

	NewButton.ButtonRect.right = NewButton.ButtonRect.left +
		NewButton.ButtonCaption.size()*NewButton.ButtonFont.fontSize;

	NewButton.ButtonRect.bottom = NewButton.ButtonRect.top +
		NewButton.ButtonFont.fontSize;

	NewButton.ButtonEllipse.point.x = (NewButton.ButtonRect.right + NewButton.ButtonRect.left) / 2;
	NewButton.ButtonEllipse.point.y = (NewButton.ButtonRect.top + NewButton.ButtonRect.bottom) / 2;

	NewButton.ButtonEllipse.radiusX = fabs(NewButton.ButtonRect.left - NewButton.ButtonRect.right) * 0.625f;
	NewButton.ButtonEllipse.radiusY = fabs(NewButton.ButtonRect.top - NewButton.ButtonRect.bottom) * 0.625f;

	NewButton.Activated = Activated;
	NewButton.Scrollable = Scrollable;

	NewButton.Name = Name;

	buttons.push_back(NewButton);
}

void GUI::CreateTextBox(ID2D1RenderTarget* D2DRenderTarget, std::wstring Caption, FLOAT Height, FLOAT Width, XMFLOAT2 Position,
	Alignment TextBoxAlign, BrushInit TextBoxBrush, FontInit TextBoxFont, bool Scrollable, std::wstring Name)
{
	TextBox NewTextBox;
	NewTextBox.TextBoxCaption = Caption;
	NewTextBox.TextBoxHeight = Height;
	NewTextBox.TextBoxWidth = Width;
	NewTextBox.TextBoxPosition = Position;

	NewTextBox.TextBoxAlignment = TextBoxAlign;

	NewTextBox.TextBoxBrush.BrushColor = TextBoxBrush.BrushColor;
	NewTextBox.TextBoxBrush.BrushStyle = TextBoxBrush.BrushStyle;

	if (NewTextBox.TextBoxBrush.BrushStyle == BrushType::SOLID_COLOR)
		D2DRenderTarget->CreateSolidColorBrush(NewTextBox.TextBoxBrush.BrushColor, &NewTextBox.TextBoxBrush.SolidColorBrush);
	else if (NewTextBox.TextBoxBrush.BrushStyle == BrushType::LINEAR_GRADIENT);
	else if (NewTextBox.TextBoxBrush.BrushStyle == BrushType::RADIAL_GRADIENT);

	NewTextBox.TextBoxFont.FontCollection = TextBoxFont.FontCollection;
	NewTextBox.TextBoxFont.FontFamilyName = TextBoxFont.FontFamilyName;
	NewTextBox.TextBoxFont.FontLocaleName = TextBoxFont.FontLocaleName;
	NewTextBox.TextBoxFont.fontSize = TextBoxFont.fontSize;
	NewTextBox.TextBoxFont.FontStretch = TextBoxFont.FontStretch;
	NewTextBox.TextBoxFont.FontStyle = TextBoxFont.FontStyle;
	NewTextBox.TextBoxFont.FontWeight = TextBoxFont.FontWeight;

	IDWriteFactory *DWriteFactory;

	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(&DWriteFactory));

	DWriteFactory->CreateTextFormat(
		NewTextBox.TextBoxFont.FontFamilyName.c_str(),
		NewTextBox.TextBoxFont.FontCollection,
		NewTextBox.TextBoxFont.FontWeight,
		NewTextBox.TextBoxFont.FontStyle,
		NewTextBox.TextBoxFont.FontStretch,
		NewTextBox.TextBoxFont.fontSize,
		NewTextBox.TextBoxFont.FontLocaleName.c_str(),
		&NewTextBox.TextBoxFont.FontTextFormat
		);

	NewTextBox.TextBoxFont.FontTextFormat->SetTextAlignment(NewTextBox.TextBoxAlignment.FontTextAlignment);
	NewTextBox.TextBoxFont.FontTextFormat->SetParagraphAlignment(NewTextBox.TextBoxAlignment.FontTextParagraphAligment);

	NewTextBox.TextBoxRect.left = NewTextBox.TextBoxPosition.x;
	NewTextBox.TextBoxRect.top = NewTextBox.TextBoxPosition.y;

	NewTextBox.TextBoxRect.right = NewTextBox.TextBoxRect.left +
		NewTextBox.TextBoxCaption.size()*NewTextBox.TextBoxFont.fontSize;

	NewTextBox.TextBoxRect.bottom = NewTextBox.TextBoxRect.top +
		NewTextBox.TextBoxFont.fontSize;

	NewTextBox.Scrollable = Scrollable;

	NewTextBox.Name = Name;

	textBoxes.push_back(NewTextBox);
}

void GUI::CreateRadioButton(ID2D1RenderTarget* D2DRenderTarget, std::wstring Caption, FLOAT Height, FLOAT Width, ButtonType Shape, XMFLOAT2 Position,
	Alignment RadioButtonAlign, BrushInit not_pressed_RadioButtonBrush, FontInit not_pressed_RadioButtonFont,
	BrushInit pressed_RadioButtonBrush, FontInit pressed_RadioButtonFont, bool Scrollable, bool Activated, std::wstring Name)
{
	RadioButton NewRadioButton;
	NewRadioButton.RadioButtonCaption = Caption;
	NewRadioButton.RadioButtonHeight = Height;
	NewRadioButton.RadioButtonWidth = Width;
	NewRadioButton.RadioButtonShape = Shape;
	NewRadioButton.RadioButtonPosition = Position;

	NewRadioButton.RadioButtonAlignment = RadioButtonAlign;

	NewRadioButton.NOT_PRESSED_RadioButtonBrush.BrushColor = not_pressed_RadioButtonBrush.BrushColor;
	NewRadioButton.NOT_PRESSED_RadioButtonBrush.BrushStyle = not_pressed_RadioButtonBrush.BrushStyle;

	NewRadioButton.PRESSED_RadioButtonBrush.BrushColor = pressed_RadioButtonBrush.BrushColor;
	NewRadioButton.PRESSED_RadioButtonBrush.BrushStyle = pressed_RadioButtonBrush.BrushStyle;

	if (NewRadioButton.PRESSED_RadioButtonBrush.BrushStyle == BrushType::SOLID_COLOR)
		D2DRenderTarget->CreateSolidColorBrush(NewRadioButton.PRESSED_RadioButtonBrush.BrushColor, &NewRadioButton.PRESSED_RadioButtonBrush.SolidColorBrush);
	else if (NewRadioButton.PRESSED_RadioButtonBrush.BrushStyle == BrushType::LINEAR_GRADIENT);
	else if (NewRadioButton.PRESSED_RadioButtonBrush.BrushStyle == BrushType::RADIAL_GRADIENT);

	if (NewRadioButton.NOT_PRESSED_RadioButtonBrush.BrushStyle == BrushType::SOLID_COLOR)
		D2DRenderTarget->CreateSolidColorBrush(NewRadioButton.NOT_PRESSED_RadioButtonBrush.BrushColor, &NewRadioButton.NOT_PRESSED_RadioButtonBrush.SolidColorBrush);
	else if (NewRadioButton.NOT_PRESSED_RadioButtonBrush.BrushStyle == BrushType::LINEAR_GRADIENT);
	else if (NewRadioButton.NOT_PRESSED_RadioButtonBrush.BrushStyle == BrushType::RADIAL_GRADIENT);


	NewRadioButton.NOT_PRESSED_RadioButtonFont.FontCollection = not_pressed_RadioButtonFont.FontCollection;
	NewRadioButton.NOT_PRESSED_RadioButtonFont.FontFamilyName = not_pressed_RadioButtonFont.FontFamilyName;
	NewRadioButton.NOT_PRESSED_RadioButtonFont.FontLocaleName = not_pressed_RadioButtonFont.FontLocaleName;
	NewRadioButton.NOT_PRESSED_RadioButtonFont.fontSize = not_pressed_RadioButtonFont.fontSize;
	NewRadioButton.NOT_PRESSED_RadioButtonFont.FontStretch = not_pressed_RadioButtonFont.FontStretch;
	NewRadioButton.NOT_PRESSED_RadioButtonFont.FontStyle = not_pressed_RadioButtonFont.FontStyle;
	NewRadioButton.NOT_PRESSED_RadioButtonFont.FontWeight = not_pressed_RadioButtonFont.FontWeight;

	NewRadioButton.PRESSED_RadioButtonFont.FontCollection = pressed_RadioButtonFont.FontCollection;
	NewRadioButton.PRESSED_RadioButtonFont.FontFamilyName = pressed_RadioButtonFont.FontFamilyName;
	NewRadioButton.PRESSED_RadioButtonFont.FontLocaleName = pressed_RadioButtonFont.FontLocaleName;
	NewRadioButton.PRESSED_RadioButtonFont.fontSize = pressed_RadioButtonFont.fontSize;
	NewRadioButton.PRESSED_RadioButtonFont.FontStretch = pressed_RadioButtonFont.FontStretch;
	NewRadioButton.PRESSED_RadioButtonFont.FontStyle = pressed_RadioButtonFont.FontStyle;
	NewRadioButton.PRESSED_RadioButtonFont.FontWeight = pressed_RadioButtonFont.FontWeight;

	IDWriteFactory *DWriteFactory;

	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(&DWriteFactory));

	DWriteFactory->CreateTextFormat(
		NewRadioButton.NOT_PRESSED_RadioButtonFont.FontFamilyName.c_str(),
		NewRadioButton.NOT_PRESSED_RadioButtonFont.FontCollection,
		NewRadioButton.NOT_PRESSED_RadioButtonFont.FontWeight,
		NewRadioButton.NOT_PRESSED_RadioButtonFont.FontStyle,
		NewRadioButton.NOT_PRESSED_RadioButtonFont.FontStretch,
		NewRadioButton.NOT_PRESSED_RadioButtonFont.fontSize,
		NewRadioButton.NOT_PRESSED_RadioButtonFont.FontLocaleName.c_str(),
		&NewRadioButton.NOT_PRESSED_RadioButtonFont.FontTextFormat
		);

	DWriteFactory->CreateTextFormat(
		NewRadioButton.PRESSED_RadioButtonFont.FontFamilyName.c_str(),
		NewRadioButton.PRESSED_RadioButtonFont.FontCollection,
		NewRadioButton.PRESSED_RadioButtonFont.FontWeight,
		NewRadioButton.PRESSED_RadioButtonFont.FontStyle,
		NewRadioButton.PRESSED_RadioButtonFont.FontStretch,
		NewRadioButton.PRESSED_RadioButtonFont.fontSize,
		NewRadioButton.PRESSED_RadioButtonFont.FontLocaleName.c_str(),
		&NewRadioButton.PRESSED_RadioButtonFont.FontTextFormat
		);

	NewRadioButton.NOT_PRESSED_RadioButtonFont.FontTextFormat->SetTextAlignment(NewRadioButton.RadioButtonAlignment.FontTextAlignment);
	NewRadioButton.NOT_PRESSED_RadioButtonFont.FontTextFormat->SetParagraphAlignment(NewRadioButton.RadioButtonAlignment.FontTextParagraphAligment);

	NewRadioButton.PRESSED_RadioButtonFont.FontTextFormat->SetTextAlignment(NewRadioButton.RadioButtonAlignment.FontTextAlignment);
	NewRadioButton.PRESSED_RadioButtonFont.FontTextFormat->SetParagraphAlignment(NewRadioButton.RadioButtonAlignment.FontTextParagraphAligment);

	NewRadioButton.NOT_PRESSED_RadioButtonRect.left = NewRadioButton.RadioButtonPosition.x;
	NewRadioButton.NOT_PRESSED_RadioButtonRect.top = NewRadioButton.RadioButtonPosition.y;

	NewRadioButton.NOT_PRESSED_RadioButtonRect.right = NewRadioButton.NOT_PRESSED_RadioButtonRect.left + NewRadioButton.NOT_PRESSED_RadioButtonFont.fontSize;
	NewRadioButton.NOT_PRESSED_RadioButtonRect.bottom = NewRadioButton.NOT_PRESSED_RadioButtonRect.top + NewRadioButton.NOT_PRESSED_RadioButtonFont.fontSize;

	NewRadioButton.NOT_PRESSED_RadioButtonEllipse.point.x = (NewRadioButton.NOT_PRESSED_RadioButtonRect.right + NewRadioButton.NOT_PRESSED_RadioButtonRect.left) / 2;
	NewRadioButton.NOT_PRESSED_RadioButtonEllipse.point.y = (NewRadioButton.NOT_PRESSED_RadioButtonRect.top + NewRadioButton.NOT_PRESSED_RadioButtonRect.bottom) / 2;
	NewRadioButton.NOT_PRESSED_RadioButtonEllipse.radiusX = fabs(NewRadioButton.NOT_PRESSED_RadioButtonRect.left - NewRadioButton.NOT_PRESSED_RadioButtonRect.right) / 2;
	NewRadioButton.NOT_PRESSED_RadioButtonEllipse.radiusY = fabs(NewRadioButton.NOT_PRESSED_RadioButtonRect.top - NewRadioButton.NOT_PRESSED_RadioButtonRect.bottom) / 2;

	FLOAT delta = NewRadioButton.NOT_PRESSED_RadioButtonFont.fontSize / 2;
	FLOAT center_Y = (NewRadioButton.NOT_PRESSED_RadioButtonRect.top + NewRadioButton.NOT_PRESSED_RadioButtonRect.bottom) / 2;

	NewRadioButton.NOT_PRESSED_RadioButtonTextRect.left = NewRadioButton.NOT_PRESSED_RadioButtonRect.right;
	NewRadioButton.NOT_PRESSED_RadioButtonTextRect.right = NewRadioButton.NOT_PRESSED_RadioButtonTextRect.left +
		NewRadioButton.NOT_PRESSED_RadioButtonFont.fontSize*NewRadioButton.RadioButtonCaption.size();
	NewRadioButton.NOT_PRESSED_RadioButtonTextRect.top = center_Y - delta;
	NewRadioButton.NOT_PRESSED_RadioButtonTextRect.bottom = center_Y + delta;



	NewRadioButton.PRESSED_RadioButtonRect.left = NewRadioButton.RadioButtonPosition.x;
	NewRadioButton.PRESSED_RadioButtonRect.top = NewRadioButton.RadioButtonPosition.y;

	NewRadioButton.PRESSED_RadioButtonRect.right = NewRadioButton.PRESSED_RadioButtonRect.left + NewRadioButton.PRESSED_RadioButtonFont.fontSize;
	NewRadioButton.PRESSED_RadioButtonRect.bottom = NewRadioButton.PRESSED_RadioButtonRect.top + NewRadioButton.PRESSED_RadioButtonFont.fontSize;

	NewRadioButton.PRESSED_RadioButtonEllipse.point.x = (NewRadioButton.PRESSED_RadioButtonRect.right + NewRadioButton.PRESSED_RadioButtonRect.left) / 2;
	NewRadioButton.PRESSED_RadioButtonEllipse.point.y = (NewRadioButton.PRESSED_RadioButtonRect.top + NewRadioButton.PRESSED_RadioButtonRect.bottom) / 2;
	NewRadioButton.PRESSED_RadioButtonEllipse.radiusX = fabs(NewRadioButton.PRESSED_RadioButtonRect.left - NewRadioButton.PRESSED_RadioButtonRect.right) / 2;
	NewRadioButton.PRESSED_RadioButtonEllipse.radiusY = fabs(NewRadioButton.PRESSED_RadioButtonRect.top - NewRadioButton.PRESSED_RadioButtonRect.bottom) / 2;

	delta = NewRadioButton.PRESSED_RadioButtonFont.fontSize / 2;
	center_Y = (NewRadioButton.PRESSED_RadioButtonRect.top + NewRadioButton.PRESSED_RadioButtonRect.bottom) / 2;

	NewRadioButton.PRESSED_RadioButtonTextRect.left = NewRadioButton.PRESSED_RadioButtonRect.right;
	NewRadioButton.PRESSED_RadioButtonTextRect.right = NewRadioButton.PRESSED_RadioButtonTextRect.left +
		NewRadioButton.PRESSED_RadioButtonFont.fontSize*NewRadioButton.RadioButtonCaption.size();
	NewRadioButton.PRESSED_RadioButtonTextRect.top = center_Y - delta;
	NewRadioButton.PRESSED_RadioButtonTextRect.bottom = center_Y + delta;

	NewRadioButton.Activated = Activated;
	NewRadioButton.Scrollable = Scrollable;
	NewRadioButton.Pressed = false;

	NewRadioButton.Name = Name;

	radioButtons.push_back(NewRadioButton);
}

bool GUI::GetRadioButtonState(std::wstring ID) const
{
	for (int i = 0; i < radioButtons.size(); ++i) {
		if (ID == radioButtons[i].Name)
			return radioButtons[i].Pressed;
	}
	return false;
}

void GUI::UpdateTextBox(std::wstring Caption, std::wstring ID)
{
	for (int i = 0; i < textBoxes.size(); ++i) {
		if (ID == textBoxes[i].Name) {
			textBoxes[i].TextBoxCaption = Caption;
			break;
		}
	}
}

void GUI::DrawGUI(ID2D1RenderTarget* D2D1RT)
{
	D2D1RT->Clear(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.0f));

	DrawButtons(D2D1RT);
	DrawTextBoxes(D2D1RT);
	DrawRadioButtons(D2D1RT);
}

void GUI::DrawButtons(ID2D1RenderTarget* D2D1RT)
{
	for (int i = 0; i < buttons.size(); ++i)
		DrawButton(D2D1RT, i);
}

void GUI::DrawTextBoxes(ID2D1RenderTarget* D2D1RT)
{
	for (int i = 0; i < textBoxes.size(); ++i)
		DrawTextBox(D2D1RT, i);
}

void GUI::DrawRadioButtons(ID2D1RenderTarget* D2D1RT)
{
	for (int i = 0; i < radioButtons.size(); ++i)
		DrawRadioButton(D2D1RT, i);
}

void GUI::DrawButton(ID2D1RenderTarget* D2D1RT, UINT ButtonIndex)
{
	if (buttons[ButtonIndex].ButtonShape == ButtonType::RECTANGULAR)
	{
		if (buttons[ButtonIndex].ButtonBrush.BrushStyle == BrushType::SOLID_COLOR) {
			D2D1RT->DrawRectangle(buttons[ButtonIndex].ButtonRect, buttons[ButtonIndex].ButtonBrush.SolidColorBrush, 1.0f, NULL);
			D2D1RT->DrawTextW(
				buttons[ButtonIndex].ButtonCaption.c_str(),
				wcslen(buttons[ButtonIndex].ButtonCaption.c_str()),
				buttons[ButtonIndex].ButtonFont.FontTextFormat,
				buttons[ButtonIndex].ButtonRect,
				buttons[ButtonIndex].ButtonBrush.SolidColorBrush
				);
		}
		else if (buttons[ButtonIndex].ButtonBrush.BrushStyle == BrushType::LINEAR_GRADIENT);
		else if (buttons[ButtonIndex].ButtonBrush.BrushStyle == BrushType::RADIAL_GRADIENT);
	}
	else if (buttons[ButtonIndex].ButtonShape == ButtonType::ELLIPSOID)
	{
		if (buttons[ButtonIndex].ButtonBrush.BrushStyle == BrushType::SOLID_COLOR) {
			D2D1RT->DrawEllipse(buttons[ButtonIndex].ButtonEllipse, buttons[ButtonIndex].ButtonBrush.SolidColorBrush, 1.0f, NULL);
			D2D1RT->DrawTextW(
				buttons[ButtonIndex].ButtonCaption.c_str(),
				wcslen(buttons[ButtonIndex].ButtonCaption.c_str()),
				buttons[ButtonIndex].ButtonFont.FontTextFormat,
				buttons[ButtonIndex].ButtonRect,
				buttons[ButtonIndex].ButtonBrush.SolidColorBrush
				);
		}
		else if (buttons[ButtonIndex].ButtonBrush.BrushStyle == BrushType::LINEAR_GRADIENT);
		else if (buttons[ButtonIndex].ButtonBrush.BrushStyle == BrushType::RADIAL_GRADIENT);
	}
}

void GUI::DrawTextBox(ID2D1RenderTarget* D2D1RT, UINT TextBoxIndex)
{
	if (textBoxes[TextBoxIndex].TextBoxBrush.BrushStyle == BrushType::SOLID_COLOR) {
		D2D1RT->DrawTextW(
			textBoxes[TextBoxIndex].TextBoxCaption.c_str(),
			wcslen(textBoxes[TextBoxIndex].TextBoxCaption.c_str()),
			textBoxes[TextBoxIndex].TextBoxFont.FontTextFormat,
			textBoxes[TextBoxIndex].TextBoxRect,
			textBoxes[TextBoxIndex].TextBoxBrush.SolidColorBrush
			);
	}
	else if (textBoxes[TextBoxIndex].TextBoxBrush.BrushStyle == BrushType::LINEAR_GRADIENT);
	else if (textBoxes[TextBoxIndex].TextBoxBrush.BrushStyle == BrushType::RADIAL_GRADIENT);
}

void GUI::DrawRadioButton(ID2D1RenderTarget* D2D1RT, UINT RadioButtonIndex)
{
	if (!radioButtons[RadioButtonIndex].Pressed)
	{
		if (radioButtons[RadioButtonIndex].RadioButtonShape == ButtonType::RECTANGULAR)
		{
			if (radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonBrush.BrushStyle == BrushType::SOLID_COLOR) {
				D2D1RT->DrawRectangle(radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonRect, radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonBrush.SolidColorBrush, 1.0f, NULL);
				D2D1RT->DrawTextW(
					radioButtons[RadioButtonIndex].RadioButtonCaption.c_str(),
					wcslen(radioButtons[RadioButtonIndex].RadioButtonCaption.c_str()),
					radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonFont.FontTextFormat,
					radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonTextRect,
					radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonBrush.SolidColorBrush
					);
			}
			else if (radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonBrush.BrushStyle == BrushType::LINEAR_GRADIENT);
			else if (radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonBrush.BrushStyle == BrushType::RADIAL_GRADIENT);
		}
		else if (radioButtons[RadioButtonIndex].RadioButtonShape == ButtonType::ELLIPSOID)
		{
			if (radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonBrush.BrushStyle == BrushType::SOLID_COLOR) {
				D2D1RT->DrawEllipse(radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonEllipse, radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonBrush.SolidColorBrush, 1.0f, NULL);
				D2D1RT->DrawTextW(
					radioButtons[RadioButtonIndex].RadioButtonCaption.c_str(),
					wcslen(radioButtons[RadioButtonIndex].RadioButtonCaption.c_str()),
					radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonFont.FontTextFormat,
					radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonTextRect,
					radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonBrush.SolidColorBrush
					);
			}
			else if (radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonBrush.BrushStyle == BrushType::LINEAR_GRADIENT);
			else if (radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonBrush.BrushStyle == BrushType::RADIAL_GRADIENT);
		}
	}
	else
	{
		if (radioButtons[RadioButtonIndex].RadioButtonShape == ButtonType::RECTANGULAR)
		{
			if (radioButtons[RadioButtonIndex].PRESSED_RadioButtonBrush.BrushStyle == BrushType::SOLID_COLOR) {
				D2D1RT->FillRectangle(radioButtons[RadioButtonIndex].PRESSED_RadioButtonRect, radioButtons[RadioButtonIndex].PRESSED_RadioButtonBrush.SolidColorBrush);
				D2D1RT->DrawTextW(
					radioButtons[RadioButtonIndex].RadioButtonCaption.c_str(),
					wcslen(radioButtons[RadioButtonIndex].RadioButtonCaption.c_str()),
					radioButtons[RadioButtonIndex].PRESSED_RadioButtonFont.FontTextFormat,
					radioButtons[RadioButtonIndex].PRESSED_RadioButtonTextRect,
					radioButtons[RadioButtonIndex].PRESSED_RadioButtonBrush.SolidColorBrush
					);
			}
			else if (radioButtons[RadioButtonIndex].PRESSED_RadioButtonBrush.BrushStyle == BrushType::LINEAR_GRADIENT);
			else if (radioButtons[RadioButtonIndex].PRESSED_RadioButtonBrush.BrushStyle == BrushType::RADIAL_GRADIENT);
		}
		else if (radioButtons[RadioButtonIndex].RadioButtonShape == ButtonType::ELLIPSOID)
		{
			if (radioButtons[RadioButtonIndex].PRESSED_RadioButtonBrush.BrushStyle == BrushType::SOLID_COLOR) {
				D2D1RT->FillEllipse(radioButtons[RadioButtonIndex].PRESSED_RadioButtonEllipse, radioButtons[RadioButtonIndex].PRESSED_RadioButtonBrush.SolidColorBrush);
				D2D1RT->DrawTextW(
					radioButtons[RadioButtonIndex].RadioButtonCaption.c_str(),
					wcslen(radioButtons[RadioButtonIndex].RadioButtonCaption.c_str()),
					radioButtons[RadioButtonIndex].PRESSED_RadioButtonFont.FontTextFormat,
					radioButtons[RadioButtonIndex].PRESSED_RadioButtonTextRect,
					radioButtons[RadioButtonIndex].PRESSED_RadioButtonBrush.SolidColorBrush
					);
			}
			else if (radioButtons[RadioButtonIndex].PRESSED_RadioButtonBrush.BrushStyle == BrushType::LINEAR_GRADIENT);
			else if (radioButtons[RadioButtonIndex].PRESSED_RadioButtonBrush.BrushStyle == BrushType::RADIAL_GRADIENT);
		}
	}
}

bool GUI::IsPointInEllipse(FLOAT radius_X, FLOAT radius_Y, XMFLOAT2 CenterPos, XMFLOAT2 PointPos) const
{
	XMFLOAT2 RelPointPos = XMFLOAT2(PointPos.x - CenterPos.x, PointPos.y - CenterPos.y);
	return (powf(RelPointPos.x, 2.0f) / powf(radius_X, 2.0f) + powf(RelPointPos.y, 2.0f) / powf(radius_Y, 2.0f) > 1 ? false : true);
}

bool GUI::IsPointInRect(D2D1_RECT_F Rect, XMFLOAT2 PointPos) const
{
	if (PointPos.x <= Rect.right && PointPos.x >= Rect.left && PointPos.y <= Rect.bottom && PointPos.y >= Rect.top)
		return true;
	else
		return false;
}

void GUI::ActivateButton(std::wstring ID)
{
	for (int i = 0; i < buttons.size(); ++i) {
		if (ID == buttons[i].Name) {
			buttons[i].Activated = true;
			break;
		}
	}
}

void GUI::DeactivateButton(std::wstring ID)
{
	for (int i = 0; i < buttons.size(); ++i) {
		if (ID == buttons[i].Name) {
			buttons[i].Activated = false;
			break;
		}
	}
}

void GUI::ScrollElements(FLOAT WheelDelta)
{
	WheelDelta /= fabs(WheelDelta);
	for (int i = 0; i < buttons.size(); ++i)
	{
		if (buttons[i].Scrollable)
			ScrollButton(i, WheelDelta);
	}
	for (int i = 0; i < textBoxes.size(); ++i)
	{
		if (textBoxes[i].Scrollable)
			ScrollTextBox(i, WheelDelta);
	}
	for (int i = 0; i <radioButtons.size(); ++i)
	{
		if (radioButtons[i].Scrollable)
			ScrollRadioButton(i, WheelDelta);
	}
}

void GUI::ScrollButton(UINT ButtonIndex, FLOAT WheelDelta)
{
	buttons[ButtonIndex].ButtonRect.bottom += WheelDelta*ScrollScale;
	buttons[ButtonIndex].ButtonRect.top += WheelDelta*ScrollScale;
	buttons[ButtonIndex].ButtonEllipse.point.y += WheelDelta*ScrollScale;
}

void GUI::ScrollTextBox(UINT TextBoxIndex, FLOAT WheelDelta)
{
	textBoxes[TextBoxIndex].TextBoxRect.bottom += WheelDelta*ScrollScale;
	textBoxes[TextBoxIndex].TextBoxRect.top += WheelDelta*ScrollScale;
}

void GUI::ScrollRadioButton(UINT RadioButtonIndex, FLOAT WheelDelta)
{
	radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonRect.bottom += WheelDelta*ScrollScale;
	radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonRect.top += WheelDelta*ScrollScale;
	radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonEllipse.point.y += WheelDelta*ScrollScale;

	radioButtons[RadioButtonIndex].PRESSED_RadioButtonRect.bottom += WheelDelta*ScrollScale;
	radioButtons[RadioButtonIndex].PRESSED_RadioButtonRect.top += WheelDelta*ScrollScale;
	radioButtons[RadioButtonIndex].PRESSED_RadioButtonEllipse.point.y += WheelDelta*ScrollScale;

	radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonTextRect.bottom += WheelDelta*ScrollScale;
	radioButtons[RadioButtonIndex].NOT_PRESSED_RadioButtonTextRect.top += WheelDelta*ScrollScale;

	radioButtons[RadioButtonIndex].PRESSED_RadioButtonTextRect.bottom += WheelDelta*ScrollScale;
	radioButtons[RadioButtonIndex].PRESSED_RadioButtonTextRect.top += WheelDelta*ScrollScale;
}

void GUI::MessageProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	static FLOAT MouseX;
	static FLOAT MouseY;
	
	switch (msg)
	{
	case WM_RBUTTONDOWN:
		break;
	case WM_LBUTTONDOWN:
		MouseX = GET_X_LPARAM(lParam) / CurClientW * InitClientW;
		MouseY = GET_Y_LPARAM(lParam) / CurClientH * InitClientH;

		for (int i = 0; i < buttons.size(); ++i)
		{
			if (buttons[i].Activated)
			{
				if (buttons[i].ButtonShape == ButtonType::ELLIPSOID)
				{
					if (IsPointInEllipse(buttons[i].ButtonEllipse.radiusX, buttons[i].ButtonEllipse.radiusY,
						XMFLOAT2(buttons[i].ButtonEllipse.point.x, buttons[i].ButtonEllipse.point.y),
						XMFLOAT2(MouseX, MouseY))) {
						ActionQueue.push_back(buttons[i].Name);
					}
				}
				else
				{
					if (IsPointInRect(buttons[i].ButtonRect,
						XMFLOAT2(MouseX, MouseY))) {
						ActionQueue.push_back(buttons[i].Name);
					}
				}
			}
		}
		for (int i = 0; i < radioButtons.size(); ++i)
		{
			if (radioButtons[i].Activated)
			{
				if (radioButtons[i].Pressed) {
					if (radioButtons[i].RadioButtonShape == ButtonType::ELLIPSOID)
					{
						if (IsPointInEllipse(radioButtons[i].PRESSED_RadioButtonEllipse.radiusX, radioButtons[i].PRESSED_RadioButtonEllipse.radiusY,
							XMFLOAT2(radioButtons[i].PRESSED_RadioButtonEllipse.point.x, radioButtons[i].PRESSED_RadioButtonEllipse.point.y),
							XMFLOAT2(MouseX, MouseY))) {
							radioButtons[i].Pressed = !radioButtons[i].Pressed;
							ActionQueue.push_back(radioButtons[i].Name);
						}
					}
					else
					{
						if (IsPointInRect(radioButtons[i].PRESSED_RadioButtonRect,
							XMFLOAT2(MouseX, MouseY))) {
							radioButtons[i].Pressed = !radioButtons[i].Pressed;
							ActionQueue.push_back(radioButtons[i].Name);
						}
					}
				}
				else
				{
					if (radioButtons[i].RadioButtonShape == ButtonType::ELLIPSOID)
					{
						if (IsPointInEllipse(radioButtons[i].NOT_PRESSED_RadioButtonEllipse.radiusX, radioButtons[i].NOT_PRESSED_RadioButtonEllipse.radiusY,
							XMFLOAT2(radioButtons[i].NOT_PRESSED_RadioButtonEllipse.point.x, radioButtons[i].NOT_PRESSED_RadioButtonEllipse.point.y),
							XMFLOAT2(MouseX, MouseY))) {
							radioButtons[i].Pressed = !radioButtons[i].Pressed;
							ActionQueue.push_back(radioButtons[i].Name);
						}
					}
					else
					{
						if (IsPointInRect(radioButtons[i].NOT_PRESSED_RadioButtonRect,
							XMFLOAT2(MouseX, MouseY))) {
							radioButtons[i].Pressed = !radioButtons[i].Pressed;
							ActionQueue.push_back(radioButtons[i].Name);
						}
					}
				}
			}
		}
		break;
	case WM_MOUSEWHEEL:
		ScrollElements(GET_WHEEL_DELTA_WPARAM(wParam));
		break;
	};
}

void GUI::OnSize(FLOAT ClientH, FLOAT ClientW)
{ 
	if (ClientH > InitClientH)
		InitClientH = ClientH;
	if (ClientW > InitClientW)
		InitClientW = ClientW;
	
	CurClientH = ClientH;
	CurClientW = ClientW;
};