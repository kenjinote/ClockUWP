#include "pch.h"
#include "App.h"

#include <ppltasks.h>

using namespace App1;
using namespace concurrency;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;

// main 関数は、IFrameworkView クラスを初期化する場合にのみ使用します。
[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
	auto direct3DApplicationSource = ref new Direct3DApplicationSource();
	CoreApplication::Run(direct3DApplicationSource);
	return 0;
}

IFrameworkView^ Direct3DApplicationSource::CreateView()
{
	return ref new App();
}

App::App() :
	m_windowClosed(false),
	m_windowVisible(true),
	m_settings(nullptr),
	m_titlebar(nullptr)
{
}

// IFrameworkView の作成時に最初のメソッドが呼び出されます。
void App::Initialize(CoreApplicationView^ applicationView)
{
	// アプリ ライフサイクルのイベント ハンドラーを登録します。この例にはアクティブ化が含まれているため、
	// CoreWindow をアクティブにし、ウィンドウで描画を開始できます。
	applicationView->Activated +=
		ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &App::OnActivated);

	CoreApplication::Suspending +=
		ref new EventHandler<SuspendingEventArgs^>(this, &App::OnSuspending);

	CoreApplication::Resuming +=
		ref new EventHandler<Platform::Object^>(this, &App::OnResuming);

	//この時点では、デバイスにアクセスできます。
	// デバイスに依存するリソースを作成できます。
	m_deviceResources = std::make_shared<DX::DeviceResources>();
	m_settings = ref new Windows::UI::ViewManagement::UISettings();
}

//CoreWindow オブジェクトが作成 (または再作成) されるときに呼び出されます。
void App::SetWindow(CoreWindow^ window)
{
	window->SizeChanged += 
		ref new TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^>(this, &App::OnWindowSizeChanged);

	window->VisibilityChanged +=
		ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &App::OnVisibilityChanged);

	window->Closed += 
		ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &App::OnWindowClosed);

	if (m_settings != nullptr) {
		m_settings->ColorValuesChanged += ref new TypedEventHandler<Windows::UI::ViewManagement::UISettings^, Platform::Object^>(this, &App::OnColorValuesChanged);
	}

	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	currentDisplayInformation->DpiChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnDpiChanged);

	currentDisplayInformation->OrientationChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnOrientationChanged);

	DisplayInformation::DisplayContentsInvalidated +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnDisplayContentsInvalidated);

	m_deviceResources->SetWindow(window);
}

// シーンのリソースを初期化するか、以前に保存したアプリ状態を読み込みます。
void App::Load(Platform::String^ entryPoint)
{
	if (m_main == nullptr)
	{
		m_main = std::unique_ptr<ClockUWPMain>(new ClockUWPMain(m_deviceResources));
	}
}

// このメソッドは、ウィンドウがアクティブになると、呼び出されます。
void App::Run()
{
	while (!m_windowClosed)
	{
		if (m_windowVisible)
		{
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

			m_main->Update();

			if (m_main->Render())
			{
				m_deviceResources->Present();
			}
		}
		else
		{
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
		}
	}
}

// IFrameworkView で必要です。
// 終了イベントでは初期化解除は呼び出されません。アプリケーションが前景に表示されている間に
//IFrameworkView クラスが解体されると呼び出されます。
void App::Uninitialize()
{
}

// アプリケーション ライフサイクル イベント ハンドラー。

void App::OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
{
	// Run() は CoreWindow がアクティブ化されるまで起動されません。
	CoreWindow::GetForCurrentThread()->Activate();
	m_titlebar = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->TitleBar;
	CoreApplication::GetCurrentView()->TitleBar->ExtendViewIntoTitleBar = true;
	SetTitleBarColor();
}

void App::OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args)
{
	// 遅延を要求した後にアプリケーションの状態を保存します。遅延状態を保持することは、
	//中断操作の実行でアプリケーションがビジー状態であることを示します。
	//遅延は制限なく保持されるわけではないことに注意してください。約 5 秒後に、
	// アプリケーションは強制終了されます。
	SuspendingDeferral^ deferral = args->SuspendingOperation->GetDeferral();

	create_task([this, deferral]()
	{
        m_deviceResources->Trim();

		// ここにコードを挿入します。

		deferral->Complete();
	});
}

void App::OnResuming(Platform::Object^ sender, Platform::Object^ args)
{
	// 中断時にアンロードされたデータまたは状態を復元します。既定では、データと状態は
	// 中断から再開するときに保持されます。このイベントは、アプリが既に終了されている場合は
	//発生しません。

	// ここにコードを挿入します。
}

// ウィンドウ イベント ハンドラー。

void App::OnWindowSizeChanged(CoreWindow^ sender, WindowSizeChangedEventArgs^ args)
{
	m_deviceResources->SetLogicalSize(Size(sender->Bounds.Width, sender->Bounds.Height));
}

void App::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
	m_windowVisible = args->Visible;
}

void App::OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args)
{
	m_windowClosed = true;
}

// DisplayInformation イベント ハンドラー。

void App::OnDpiChanged(DisplayInformation^ sender, Object^ args)
{
	// 注意: 高解像度のデバイス用にスケーリングされている場合は、取得した LogicalDpi の値がアプリの有効な DPI と一致しない場合があります。
	// DPI が DeviceResources 上に設定された場合、
	// 常に GetDpi メソッドを使用してそれを取得する必要があります。
	// 詳細については、DeviceResources.cpp を参照してください。
	m_deviceResources->SetDpi(sender->LogicalDpi);
}

void App::OnOrientationChanged(DisplayInformation^ sender, Object^ args)
{
	m_deviceResources->SetCurrentOrientation(sender->CurrentOrientation);
}

void App::OnDisplayContentsInvalidated(DisplayInformation^ sender, Object^ args)
{
	m_deviceResources->ValidateDevice();
}

void App::OnColorValuesChanged(Windows::UI::ViewManagement::UISettings^ sender, Platform::Object^ args)
{
	SetTitleBarColor();
}

void App::SetTitleBarColor()
{
	if (m_titlebar != nullptr) {
		Windows::UI::Color colorForeground = Theme::GetThemeColor(Windows::UI::ViewManagement::UIColorType::Foreground);

		m_titlebar->ButtonForegroundColor = colorForeground;
		m_titlebar->ButtonHoverForegroundColor = colorForeground;
		m_titlebar->ButtonInactiveForegroundColor = colorForeground;
		m_titlebar->ButtonPressedForegroundColor = colorForeground;
		m_titlebar->ForegroundColor = colorForeground;
		m_titlebar->InactiveForegroundColor = colorForeground;

		m_titlebar->BackgroundColor = Windows::UI::Colors::Transparent;
		m_titlebar->ButtonBackgroundColor = Windows::UI::Colors::Transparent;
		m_titlebar->ButtonHoverBackgroundColor = Windows::UI::Colors::Transparent;
		m_titlebar->ButtonInactiveBackgroundColor = Windows::UI::Colors::Transparent;
		m_titlebar->ButtonPressedBackgroundColor = Windows::UI::Colors::Transparent;
		m_titlebar->InactiveBackgroundColor = Windows::UI::Colors::Transparent;
	}
}
