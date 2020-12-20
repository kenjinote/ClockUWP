#include "pch.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"

using namespace D2D1;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Platform;

namespace DisplayMetrics
{
	// 高解像度ディスプレイは、レンダリングに多くの GPU とバッテリ電力を必要とします。
	// ゲームを完全な再現性を維持して毎秒 60 フレームでレンダリングしようとすると、
	// 高解像度の携帯電話などではバッテリの寿命の短さに悩まされる場合があります。
	// すべてのプラットフォームとフォーム ファクターにわたって完全な再現性を維持してのレンダリングは、
	// 慎重に検討して決定する必要があります。
	static const bool SupportHighResolutions = false;

	// "高解像度" ディスプレイを定義する既定のしきい値。しきい値を
	// 超え、SupportHighResolutions が false の場合は、ディメンションが
	// 50 % のスケールになります。
	static const float DpiThreshold = 192.0f;		// 標準のデスクトップの 200% 表示。
	static const float WidthThreshold = 1920.0f;	// 幅 1080p。
	static const float HeightThreshold = 1080.0f;	// 高さ 1080p。
};

// 画面の回転の計算に使用する定数
namespace ScreenRotation
{
	// 0 度 Z 回転
	static const XMFLOAT4X4 Rotation0(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 90 度 Z 回転
	static const XMFLOAT4X4 Rotation90(
		0.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 180 度 Z 回転
	static const XMFLOAT4X4 Rotation180(
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 270 度 Z 回転
	static const XMFLOAT4X4 Rotation270(
		0.0f, -1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);
};

// DeviceResources に対するコンストラクター。
DX::DeviceResources::DeviceResources() :
	m_screenViewport(),
	m_d3dFeatureLevel(D3D_FEATURE_LEVEL_9_1),
	m_d3dRenderTargetSize(),
	m_outputSize(),
	m_logicalSize(),
	m_nativeOrientation(DisplayOrientations::None),
	m_currentOrientation(DisplayOrientations::None),
	m_dpi(-1.0f),
	m_effectiveDpi(-1.0f),
	m_deviceNotify(nullptr)
{
	CreateDeviceIndependentResources();
	CreateDeviceResources();
}

// Direct3D デバイスに依存しないリソースを構成します。
void DX::DeviceResources::CreateDeviceIndependentResources()
{
	// Direct2D リソースを初期化します。
	D2D1_FACTORY_OPTIONS options;
	ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));

#if defined(_DEBUG)
	// プロジェクトがデバッグ ビルドに含まれている場合は、Direct2D デバッグを SDK レイヤーを介して有効にします。
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

	// Direct2D ファクトリを初期化します。
	DX::ThrowIfFailed(
		D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
			__uuidof(ID2D1Factory3),
			&options,
			&m_d2dFactory
			)
		);

	// DirectWrite ファクトリを初期化します。
	DX::ThrowIfFailed(
		DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory3),
			&m_dwriteFactory
			)
		);

	// Windows Imaging Component (WIC) ファクトリを初期化します。
	DX::ThrowIfFailed(
		CoCreateInstance(
			CLSID_WICImagingFactory2,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&m_wicFactory)
			)
		);
}

// Direct3D デバイスを構成し、このハンドルとデバイスのコンテキストを保存します。
void DX::DeviceResources::CreateDeviceResources() 
{
	//このフラグは、カラー チャネルの順序が API の既定値とは異なるサーフェスのサポートを追加します。
	// これは、Direct2D との互換性を保持するために必要です。
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	if (DX::SdkLayersAvailable())
	{
		// プロジェクトがデバッグ ビルドに含まれる場合、このフラグを使用して SDK レイヤーによるデバッグを有効にします。
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}
#endif

	// この配列では、このアプリケーションでサポートされる DirectX ハードウェア機能レベルのセットを定義します。
	// 順序が保存されることに注意してください。
	//アプリケーションの最低限必要な機能レベルをその説明で宣言することを忘れないでください。
	//特に記載がない限り、すべてのアプリケーションは 9.1 をサポートすることが想定されます。
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1
	};

	// Direct3D 11 API デバイス オブジェクトと、対応するコンテキストを作成します。
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;

	HRESULT hr = D3D11CreateDevice(
		nullptr,					// 既定のアダプターを使用する nullptr を指定します。
		D3D_DRIVER_TYPE_HARDWARE,	// ハードウェア グラフィックス ドライバーを使用してデバイスを作成します。
		0,							// ドライバーが D3D_DRIVER_TYPE_SOFTWARE でない限り、0 を使用してください。
		creationFlags,				// デバッグ フラグと Direct2D 互換性フラグを設定します。
		featureLevels,				// このアプリがサポートできる機能レベルの一覧を表示します。
		ARRAYSIZE(featureLevels),	// 上記リストのサイズ。
		D3D11_SDK_VERSION,			// Microsoft Store アプリでは、これには常に D3D11_SDK_VERSION を設定します。
		&device,					// 作成された Direct3D デバイスを返します。
		&m_d3dFeatureLevel,			// 作成されたデバイスの機能レベルを返します。
		&context					// デバイスのイミディエイト コンテキストを返します。
		);

	if (FAILED(hr))
	{
		// 初期化が失敗した場合は、WARP デバイスにフォール バックします。
		// WARP の詳細については、次を参照してください: 
		// https://go.microsoft.com/fwlink/?LinkId=286690
		DX::ThrowIfFailed(
			D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP, // ハードウェア デバイスの代わりに WARP デバイスを作成します。
				0,
				creationFlags,
				featureLevels,
				ARRAYSIZE(featureLevels),
				D3D11_SDK_VERSION,
				&device,
				&m_d3dFeatureLevel,
				&context
				)
			);
	}

	// Direct3D 11.3 API デバイスへのポインターとイミディエイト コンテキストを保存します。
	DX::ThrowIfFailed(
		device.As(&m_d3dDevice)
		);

	DX::ThrowIfFailed(
		context.As(&m_d3dContext)
		);

	// Direct2D デバイス オブジェクトと、対応するコンテキストを作成します。
	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(
		m_d3dDevice.As(&dxgiDevice)
		);

	DX::ThrowIfFailed(
		m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice)
		);

	DX::ThrowIfFailed(
		m_d2dDevice->CreateDeviceContext(
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
			&m_d2dContext
			)
		);
}

// これらのリソースは、ウィンドウ サイズが変更されるたびに再作成する必要があります。
void DX::DeviceResources::CreateWindowSizeDependentResources() 
{
	// 前のウィンドウ サイズに固有のコンテキストをクリアします。
	ID3D11RenderTargetView* nullViews[] = {nullptr};
	m_d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
	m_d3dRenderTargetView = nullptr;
	m_d2dContext->SetTarget(nullptr);
	m_d2dTargetBitmap = nullptr;
	m_d3dDepthStencilView = nullptr;
	m_d3dContext->Flush1(D3D11_CONTEXT_TYPE_ALL, nullptr);

	UpdateRenderTargetSize();

	// スワップ チェーンの幅と高さは、ウィンドウのネイティブ方向の幅と高さに
	// 基づいている必要があります。ウィンドウがネイティブではない場合は、
	// サイズを反転させる必要があります。
	DXGI_MODE_ROTATION displayRotation = ComputeDisplayRotation();

	bool swapDimensions = displayRotation == DXGI_MODE_ROTATION_ROTATE90 || displayRotation == DXGI_MODE_ROTATION_ROTATE270;
	m_d3dRenderTargetSize.Width = swapDimensions ? m_outputSize.Height : m_outputSize.Width;
	m_d3dRenderTargetSize.Height = swapDimensions ? m_outputSize.Width : m_outputSize.Height;

	if (m_swapChain != nullptr)
	{
		// スワップ チェーンが既に存在する場合は、そのサイズを変更します。
		HRESULT hr = m_swapChain->ResizeBuffers(
			2, // ダブル バッファーされたスワップ チェーンです。
			lround(m_d3dRenderTargetSize.Width),
			lround(m_d3dRenderTargetSize.Height),
			DXGI_FORMAT_B8G8R8A8_UNORM,
			0
			);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// 何らかの理由でデバイスを削除した場合、新しいデバイスとスワップ チェーンを作成する必要があります。
			HandleDeviceLost();

			//すべての設定が完了しました。このメソッドの実行を続行しないでください。HandleDeviceLost はこのメソッドに再入し、
			// 新しいデバイスを正しく設定します。
			return;
		}
		else
		{
			DX::ThrowIfFailed(hr);
		}
	}
	else
	{
		// それ以外の場合は、既存の Direct3D デバイスと同じアダプターを使用して、新規作成します。
		DXGI_SCALING scaling = DisplayMetrics::SupportHighResolutions ? DXGI_SCALING_NONE : DXGI_SCALING_STRETCH;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};

		swapChainDesc.Width = lround(m_d3dRenderTargetSize.Width);		// ウィンドウのサイズと一致させます。
		swapChainDesc.Height = lround(m_d3dRenderTargetSize.Height);
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;				// これは、最も一般的なスワップ チェーンのフォーマットです。
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;								// マルチサンプリングは使いません。
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;									// 遅延を最小限に抑えるにはダブル バッファーを使用します。
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;	// すべての Microsoft Store アプリは、この SwapEffect を使用する必要があります。
		swapChainDesc.Flags = 0;
		swapChainDesc.Scaling = scaling;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		// このシーケンスは、上の Direct3D デバイスを作成する際に使用された DXGI ファクトリを取得します。
		ComPtr<IDXGIDevice3> dxgiDevice;
		DX::ThrowIfFailed(
			m_d3dDevice.As(&dxgiDevice)
			);

		ComPtr<IDXGIAdapter> dxgiAdapter;
		DX::ThrowIfFailed(
			dxgiDevice->GetAdapter(&dxgiAdapter)
			);

		ComPtr<IDXGIFactory4> dxgiFactory;
		DX::ThrowIfFailed(
			dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))
			);

		ComPtr<IDXGISwapChain1> swapChain;
		DX::ThrowIfFailed(
			dxgiFactory->CreateSwapChainForCoreWindow(
				m_d3dDevice.Get(),
				reinterpret_cast<IUnknown*>(m_window.Get()),
				&swapChainDesc,
				nullptr,
				&swapChain
				)
			);
		DX::ThrowIfFailed(
			swapChain.As(&m_swapChain)
			);

		// DXGI が 1 度に複数のフレームをキュー処理していないことを確認します。これにより、遅延が減少し、
		// アプリケーションが各 VSync の後でのみレンダリングすることが保証され、消費電力が最小限に抑えられます。
		DX::ThrowIfFailed(
			dxgiDevice->SetMaximumFrameLatency(1)
			);
	}

	// スワップ チェーンの適切な方向を設定し、回転されたスワップ チェーンにレンダリングするための 2D および
	// 3D マトリックス変換を生成します。
	// 2D および 3D 変換の回転角度は異なります。
	//これは座標空間の違いによります。さらに、
	// 丸めエラーを回避するために 3D マトリックスが明示的に指定されます。

	switch (displayRotation)
	{
	case DXGI_MODE_ROTATION_IDENTITY:
		m_orientationTransform2D = Matrix3x2F::Identity();
		m_orientationTransform3D = ScreenRotation::Rotation0;
		break;

	case DXGI_MODE_ROTATION_ROTATE90:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(90.0f) *
			Matrix3x2F::Translation(m_logicalSize.Height, 0.0f);
		m_orientationTransform3D = ScreenRotation::Rotation270;
		break;

	case DXGI_MODE_ROTATION_ROTATE180:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(180.0f) *
			Matrix3x2F::Translation(m_logicalSize.Width, m_logicalSize.Height);
		m_orientationTransform3D = ScreenRotation::Rotation180;
		break;

	case DXGI_MODE_ROTATION_ROTATE270:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(270.0f) *
			Matrix3x2F::Translation(0.0f, m_logicalSize.Width);
		m_orientationTransform3D = ScreenRotation::Rotation90;
		break;

	default:
		throw ref new FailureException();
	}

	DX::ThrowIfFailed(
		m_swapChain->SetRotation(displayRotation)
		);

	// スワップ チェーンのバック バッファーのレンダリング ターゲット ビューを作成します。
	ComPtr<ID3D11Texture2D1> backBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))
		);

	DX::ThrowIfFailed(
		m_d3dDevice->CreateRenderTargetView1(
			backBuffer.Get(),
			nullptr,
			&m_d3dRenderTargetView
			)
		);

	// 必要な場合は 3D レンダリングで使用する深度ステンシル ビューを作成します。
	CD3D11_TEXTURE2D_DESC1 depthStencilDesc(
		DXGI_FORMAT_D24_UNORM_S8_UINT, 
		lround(m_d3dRenderTargetSize.Width),
		lround(m_d3dRenderTargetSize.Height),
		1, // この深度ステンシル ビューには、1 つのテクスチャしかありません。
		1, // 1 つの MIPMAP レベルを使用します。
		D3D11_BIND_DEPTH_STENCIL
		);

	ComPtr<ID3D11Texture2D1> depthStencil;
	DX::ThrowIfFailed(
		m_d3dDevice->CreateTexture2D1(
			&depthStencilDesc,
			nullptr,
			&depthStencil
			)
		);

	CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
	DX::ThrowIfFailed(
		m_d3dDevice->CreateDepthStencilView(
			depthStencil.Get(),
			&depthStencilViewDesc,
			&m_d3dDepthStencilView
			)
		);
	
	// 3D レンダリング ビューポートをウィンドウ全体をターゲットにするように設定します。
	m_screenViewport = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		m_d3dRenderTargetSize.Width,
		m_d3dRenderTargetSize.Height
		);

	m_d3dContext->RSSetViewports(1, &m_screenViewport);

	// スワップ チェーン バック バッファーに関連付けられた Direct2D ターゲット ビットマップを作成し、
	// それを現在のターゲットとして設定します。
	D2D1_BITMAP_PROPERTIES1 bitmapProperties = 
		D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
			m_dpi,
			m_dpi
			);

	ComPtr<IDXGISurface2> dxgiBackBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer))
		);

	DX::ThrowIfFailed(
		m_d2dContext->CreateBitmapFromDxgiSurface(
			dxgiBackBuffer.Get(),
			&bitmapProperties,
			&m_d2dTargetBitmap
			)
		);

	m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());
	m_d2dContext->SetDpi(m_effectiveDpi, m_effectiveDpi);

	// すべての Microsoft Store アプリで、グレースケール テキストのアンチエイリアシングをお勧めします。
	m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
}

// レンダー ターゲットのディメンションを決定し、それをスケール ダウンするかどうかを判断します。
void DX::DeviceResources::UpdateRenderTargetSize()
{
	m_effectiveDpi = m_dpi;

	// 高解像度のデバイスのバッテリ寿命を上げるためには、より小さいレンダー ターゲットにレンダリングして
	// 出力が提示された場合は GPU で出力をスケーリングできるようにします。
	if (!DisplayMetrics::SupportHighResolutions && m_dpi > DisplayMetrics::DpiThreshold)
	{
		float width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_dpi);
		float height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_dpi);

		// デバイスが縦の向きの場合、高さ > 幅となります。
		// 寸法の大きい方を幅しきい値と、小さい方を高さしきい値と
		// それぞれ比較します。
		if (max(width, height) > DisplayMetrics::WidthThreshold && min(width, height) > DisplayMetrics::HeightThreshold)
		{
			// アプリをスケーリングするには有効な DPI を変更します。論理サイズは変更しません。
			m_effectiveDpi /= 2.0f;
		}
	}

	// 必要なレンダリング ターゲットのサイズをピクセル単位で計算します。
	m_outputSize.Width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_effectiveDpi);
	m_outputSize.Height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_effectiveDpi);

	// サイズ 0 の DirectX コンテンツが作成されることを防止します。
	m_outputSize.Width = max(m_outputSize.Width, 1);
	m_outputSize.Height = max(m_outputSize.Height, 1);
}

//このメソッドは、CoreWindow オブジェクトが作成 (または再作成) されるときに呼び出されます。
void DX::DeviceResources::SetWindow(CoreWindow^ window)
{
	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	m_window = window;
	m_logicalSize = Windows::Foundation::Size(window->Bounds.Width, window->Bounds.Height);
	m_nativeOrientation = currentDisplayInformation->NativeOrientation;
	m_currentOrientation = currentDisplayInformation->CurrentOrientation;
	m_dpi = currentDisplayInformation->LogicalDpi;
	m_d2dContext->SetDpi(m_dpi, m_dpi);

	CreateWindowSizeDependentResources();
}

// このメソッドは、SizeChanged イベント用のイベント ハンドラーの中で呼び出されます。
void DX::DeviceResources::SetLogicalSize(Windows::Foundation::Size logicalSize)
{
	if (m_logicalSize != logicalSize)
	{
		m_logicalSize = logicalSize;
		CreateWindowSizeDependentResources();
	}
}

// このメソッドは、DpiChanged イベント用のイベント ハンドラーの中で呼び出されます。
void DX::DeviceResources::SetDpi(float dpi)
{
	if (dpi != m_dpi)
	{
		m_dpi = dpi;

		// ディスプレイ DPI の変更時に、ウィンドウの論理サイズ (Dip 単位) も変更されるため、更新する必要があります。
		m_logicalSize = Windows::Foundation::Size(m_window->Bounds.Width, m_window->Bounds.Height);

		m_d2dContext->SetDpi(m_dpi, m_dpi);
		CreateWindowSizeDependentResources();
	}
}

// このメソッドは、OrientationChanged イベント用のイベント ハンドラーの中で呼び出されます。
void DX::DeviceResources::SetCurrentOrientation(DisplayOrientations currentOrientation)
{
	if (m_currentOrientation != currentOrientation)
	{
		m_currentOrientation = currentOrientation;
		CreateWindowSizeDependentResources();
	}
}

// このメソッドは、DisplayContentsInvalidated イベント用のイベント ハンドラーの中で呼び出されます。
void DX::DeviceResources::ValidateDevice()
{
	//デバイスが作成された後に既定のアダプターが変更された、
	// またはこのデバイスが削除された場合は、D3D デバイスが有効でなくなります。

	// まず、デバイスが作成された時点から、既定のアダプターに関する情報を取得します。

	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

	ComPtr<IDXGIAdapter> deviceAdapter;
	DX::ThrowIfFailed(dxgiDevice->GetAdapter(&deviceAdapter));

	ComPtr<IDXGIFactory4> deviceFactory;
	DX::ThrowIfFailed(deviceAdapter->GetParent(IID_PPV_ARGS(&deviceFactory)));

	ComPtr<IDXGIAdapter1> previousDefaultAdapter;
	DX::ThrowIfFailed(deviceFactory->EnumAdapters1(0, &previousDefaultAdapter));

	DXGI_ADAPTER_DESC1 previousDesc;
	DX::ThrowIfFailed(previousDefaultAdapter->GetDesc1(&previousDesc));

	// 次に、現在の既定のアダプターの情報を取得します。

	ComPtr<IDXGIFactory4> currentFactory;
	DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&currentFactory)));

	ComPtr<IDXGIAdapter1> currentDefaultAdapter;
	DX::ThrowIfFailed(currentFactory->EnumAdapters1(0, &currentDefaultAdapter));

	DXGI_ADAPTER_DESC1 currentDesc;
	DX::ThrowIfFailed(currentDefaultAdapter->GetDesc1(&currentDesc));

	// アダプターの LUID が一致しない、またはデバイスで LUID が削除されたとの報告があった場合は、
	// 新しい D3D デバイスを作成する必要があります。

	if (previousDesc.AdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart ||
		previousDesc.AdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart ||
		FAILED(m_d3dDevice->GetDeviceRemovedReason()))
	{
		// 古いデバイスに関連したリソースへの参照を解放します。
		dxgiDevice = nullptr;
		deviceAdapter = nullptr;
		deviceFactory = nullptr;
		previousDefaultAdapter = nullptr;

		// 新しいデバイスとスワップ チェーンを作成します。
		HandleDeviceLost();
	}
}

// すべてのデバイス リソースを再作成し、現在の状態に再設定します。
void DX::DeviceResources::HandleDeviceLost()
{
	m_swapChain = nullptr;

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceLost();
	}

	CreateDeviceResources();
	m_d2dContext->SetDpi(m_dpi, m_dpi);
	CreateWindowSizeDependentResources();

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceRestored();
	}
}

// デバイスが失われたときと作成されたときに通知を受けるように、DeviceNotify を登録します。
void DX::DeviceResources::RegisterDeviceNotify(DX::IDeviceNotify* deviceNotify)
{
	m_deviceNotify = deviceNotify;
}

//アプリが停止したときに、このメソッドを呼び出します。アプリがアイドル状態になっていること、
// 一時バッファーが他のアプリで使用するために解放できることを、ドライバーに示します。
void DX::DeviceResources::Trim()
{
	ComPtr<IDXGIDevice3> dxgiDevice;
	m_d3dDevice.As(&dxgiDevice);

	dxgiDevice->Trim();
}

// スワップ チェーンの内容を画面に表示します。
void DX::DeviceResources::Present() 
{
	// 最初の引数は、DXGI に VSync までブロックするよう指示し、アプリケーションを次の VSync まで
	// スリープさせます。これにより、画面に表示されることのないフレームをレンダリングして
	// サイクルを無駄にすることがなくなります。
	DXGI_PRESENT_PARAMETERS parameters = { 0 };
	HRESULT hr = m_swapChain->Present1(1, 0, &parameters);

	// レンダリング ターゲットのコンテンツを破棄します。
	//この操作は、既存のコンテンツ全体が上書きされる場合のみ有効です。
	// dirty rect または scroll rect を使用する場合は、この呼び出しを削除する必要があります。
	m_d3dContext->DiscardView1(m_d3dRenderTargetView.Get(), nullptr, 0);

	// 深度ステンシルのコンテンツを破棄します。
	m_d3dContext->DiscardView1(m_d3dDepthStencilView.Get(), nullptr, 0);

	//デバイスが切断またはドライバーの更新によって削除された場合は、
	// すべてのデバイス リソースを再作成する必要があります。
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		HandleDeviceLost();
	}
	else
	{
		DX::ThrowIfFailed(hr);
	}
}

// このメソッドは、表示デバイスのネイティブの方向と、現在の表示の方向との間での回転を決定します。
// 回転を決定します。
DXGI_MODE_ROTATION DX::DeviceResources::ComputeDisplayRotation()
{
	DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;

	// メモ: DisplayOrientations 列挙型に他の値があっても、NativeOrientation として使用できるのは、
	// Landscape または Portrait のどちらかのみです。
	switch (m_nativeOrientation)
	{
	case DisplayOrientations::Landscape:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;
		}
		break;

	case DisplayOrientations::Portrait:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;
		}
		break;
	}
	return rotation;
}