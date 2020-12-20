#include "pch.h"
#include "TextRenderer.h"

#include "Common/DirectXHelper.h"

using namespace App1;
using namespace Microsoft::WRL;

// テキスト レンダリングで使用する D2D リソースを初期化します。
SampleFpsTextRenderer::SampleFpsTextRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) : 
	m_text(L""),
	m_deviceResources(deviceResources)
{
	ZeroMemory(&m_textMetrics, sizeof(DWRITE_TEXT_METRICS));

	//デバイスに依存するリソースを作成します。
	ComPtr<IDWriteTextFormat> textFormat;
	DX::ThrowIfFailed(
		m_deviceResources->GetDWriteFactory()->CreateTextFormat(
			L"Segoe UI",
			nullptr,
			DWRITE_FONT_WEIGHT_LIGHT,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			32.0f,
			L"en-US",
			&textFormat
			)
		);

	DX::ThrowIfFailed(
		textFormat.As(&m_textFormat)
		);

	DX::ThrowIfFailed(
		m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER)
	);

	DX::ThrowIfFailed(
		m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER)
	);

	DX::ThrowIfFailed(
		m_deviceResources->GetD2DFactory()->CreateDrawingStateBlock(&m_stateBlock)
		);

	CreateDeviceDependentResources();
}

// 表示するテキストを更新します。
void SampleFpsTextRenderer::Update(DX::StepTimer const& timer)
{
	// 表示するテキストを更新します。
	uint32 fps = timer.GetFramesPerSecond();

	// 現在時間を取得
	const time_t t = time(0);
	tm t2 = { 0 };
	errno_t error = localtime_s(&t2, &t);
	if (!error)
	{
		wchar_t date[64];
		wcsftime(date, _countof(date), L"%Y年%m月%d日 %H時%M分%S秒", &t2);
		m_text = date;
	}

	ComPtr<IDWriteTextLayout> textLayout;
	DX::ThrowIfFailed(
		m_deviceResources->GetDWriteFactory()->CreateTextLayout(
			m_text.c_str(),
			(uint32) m_text.length(),
			m_textFormat.Get(),
			m_deviceResources->GetLogicalSize().Width, // 入力テキストの最大幅。
			m_deviceResources->GetLogicalSize().Height, // 入力テキストの最大高さ。
			&textLayout
			)
		);

	DX::ThrowIfFailed(
		textLayout.As(&m_textLayout)
		);

	DX::ThrowIfFailed(
		m_textLayout->GetMetrics(&m_textMetrics)
		);
}

// フレームを画面に描画します。
void SampleFpsTextRenderer::Render()
{
	ID2D1DeviceContext* context = m_deviceResources->GetD2DDeviceContext();
	Windows::Foundation::Size logicalSize = m_deviceResources->GetLogicalSize();

	context->SaveDrawingState(m_stateBlock.Get());
	context->BeginDraw();

	context->DrawTextLayout(
		D2D1::Point2F(0.f, 0.f),
		m_textLayout.Get(),
		m_textColor.Get()
		);

	//D2DERR_RECREATE_TARGET をここで無視します。このエラーは、デバイスが失われたことを示します。
	// これは、Present に対する次回の呼び出し中に処理されます。
	HRESULT hr = context->EndDraw();
	if (hr != D2DERR_RECREATE_TARGET)
	{
		DX::ThrowIfFailed(hr);
	}

	context->RestoreDrawingState(m_stateBlock.Get());
}

void SampleFpsTextRenderer::CreateDeviceDependentResources()
{
	Windows::UI::Color textColor = Theme::GetThemeColor(Windows::UI::ViewManagement::UIColorType::Background);
	DX::ThrowIfFailed(
		m_deviceResources->GetD2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(textColor.R, textColor.G, textColor.B), &m_textColor)
		);
}
void SampleFpsTextRenderer::ReleaseDeviceDependentResources()
{
	m_textColor.Reset();
}