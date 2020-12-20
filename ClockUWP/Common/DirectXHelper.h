#pragma once

#include <ppltasks.h>	// create_task 用

namespace DX
{
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			// Win32 API エラーをキャッチするためのブレークポイントをこの行に設定します。
			throw Platform::Exception::CreateException(hr);
		}
	}

	// バイナリ ファイルから非同期に読み取る関数。
	inline Concurrency::task<std::vector<byte>> ReadDataAsync(const std::wstring& filename)
	{
		using namespace Windows::Storage;
		using namespace Concurrency;

		auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;

		return create_task(folder->GetFileAsync(Platform::StringReference(filename.c_str()))).then([] (StorageFile^ file) 
		{
			return FileIO::ReadBufferAsync(file);
		}).then([] (Streams::IBuffer^ fileBuffer) -> std::vector<byte> 
		{
			std::vector<byte> returnBuffer;
			returnBuffer.resize(fileBuffer->Length);
			Streams::DataReader::FromBuffer(fileBuffer)->ReadBytes(Platform::ArrayReference<byte>(returnBuffer.data(), fileBuffer->Length));
			return returnBuffer;
		});
	}

	// デバイスに依存しないピクセル単位 (DIP) の長さを物理的なピクセルの長さに変換します。
	inline float ConvertDipsToPixels(float dips, float dpi)
	{
		static const float dipsPerInch = 96.0f;
		return floorf(dips * dpi / dipsPerInch + 0.5f); // 最も近い整数値に丸めます。
	}

#if defined(_DEBUG)
	// SDK レイヤーのサポートを確認してください。
	inline bool SdkLayersAvailable()
	{
		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_NULL,       // 実際のハードウェア デバイスを作成する必要はありません。
			0,
			D3D11_CREATE_DEVICE_DEBUG,  // SDK レイヤーを確認してください。
			nullptr,                    // どの機能レベルでも対応できます。
			0,
			D3D11_SDK_VERSION,          // Microsoft Store アプリでは、これには常に D3D11_SDK_VERSION を設定します。
			nullptr,                    // D3D デバイスの参照を保持する必要はありません。
			nullptr,                    // 機能レベルを調べる必要はありません。
			nullptr                     // D3D デバイスのコンテキスト参照を保持する必要はありません。
			);

		return SUCCEEDED(hr);
	}
#endif
}
