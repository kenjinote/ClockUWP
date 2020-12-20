#pragma once
namespace App1
{
	class Theme
	{
	public:
		static Windows::UI::Color GetThemeColor(Windows::UI::ViewManagement::UIColorType colorType)
		{
			auto settings = ref new Windows::UI::ViewManagement::UISettings();
			return settings->GetColorValue(colorType);
		}
	};
}

