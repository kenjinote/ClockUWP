#include "pch.h"
#include "ClockUWPMain.h"
#include "Common\DirectXHelper.h"

using namespace App1;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

// アプリケーションの読み込み時にアプリケーション資産を読み込んで初期化します。
ClockUWPMain::ClockUWPMain(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_deviceResources(deviceResources)
{
	// デバイスが失われたときや再作成されたときに通知を受けるように登録します
	m_deviceResources->RegisterDeviceNotify(this);

	// TODO: これをアプリのコンテンツの初期化で置き換えます。
	//m_sceneRenderer = std::unique_ptr<Sample3DSceneRenderer>(new Sample3DSceneRenderer(m_deviceResources));

	m_textRenderer = std::unique_ptr<SampleFpsTextRenderer>(new SampleFpsTextRenderer(m_deviceResources));

	// TODO: 既定の可変タイムステップ モード以外のモードが必要な場合は、タイマー設定を変更してください。
	// 例: 60 FPS 固定タイムステップ更新ロジックでは、次を呼び出します:
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0); // 1 秒ごとに再描画する
}

ClockUWPMain::~ClockUWPMain()
{
	// デバイスの通知を登録解除しています
	m_deviceResources->RegisterDeviceNotify(nullptr);
}

// アプリケーション状態をフレームごとに 1 回更新します。
void ClockUWPMain::Update() 
{
	// シーン オブジェクトを更新します。
	m_timer.Tick([&]()
	{
		// TODO: これをアプリのコンテンツの更新関数で置き換えます。
		m_textRenderer->Update(m_timer);
	});
}

// 現在のアプリケーション状態に応じて現在のフレームをレンダリングします。
// フレームがレンダリングされ、表示準備が完了すると、true を返します。
bool ClockUWPMain::Render() 
{
	// 初回更新前にレンダリングは行わないようにしてください。
	if (m_timer.GetFrameCount() == 0)
	{
		return false;
	}

	auto context = m_deviceResources->GetD3DDeviceContext();

	// ビューポートをリセットして全画面をターゲットとします。
	auto viewport = m_deviceResources->GetScreenViewport();
	context->RSSetViewports(1, &viewport);

	// レンダリング ターゲットを画面にリセットします。
	ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
	context->OMSetRenderTargets(1, targets, m_deviceResources->GetDepthStencilView());

	// バック バッファーと深度ステンシル ビューをクリアします。
	Windows::UI::Color color = Theme::GetThemeColor(Windows::UI::ViewManagement::UIColorType::Accent);
	const float clearColor[4] = { color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, color.A / 255.0f };
	context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), clearColor);
	context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// シーン オブジェクトをレンダリングします。
	// TODO: これをアプリのコンテンツのレンダリング関数で置き換えます。
	m_textRenderer->Render();

	return true;
}

// デバイス リソースを解放する必要が生じたことをレンダラーに通知します。
void ClockUWPMain::OnDeviceLost()
{
	m_textRenderer->ReleaseDeviceDependentResources();
}

// デバイス リソースの再作成が可能になったことをレンダラーに通知します。
void ClockUWPMain::OnDeviceRestored()
{
	m_textRenderer->CreateDeviceDependentResources();
}
