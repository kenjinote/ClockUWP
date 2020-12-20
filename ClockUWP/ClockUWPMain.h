#pragma once

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"
#include "Content\TextRenderer.h"

// Direct2D および 3D コンテンツを画面上でレンダリングします。
namespace App1
{
	class ClockUWPMain : public DX::IDeviceNotify
	{
	public:
		ClockUWPMain(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		~ClockUWPMain();
		void Update();
		bool Render();

		// IDeviceNotify
		virtual void OnDeviceLost();
		virtual void OnDeviceRestored();

	private:
		// デバイス リソースへのキャッシュされたポインター。
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// TODO: これを独自のコンテンツ レンダラーで置き換えます。
		std::unique_ptr<SampleFpsTextRenderer> m_textRenderer;

		// ループ タイマーをレンダリングしています。
		DX::StepTimer m_timer;
	};
}