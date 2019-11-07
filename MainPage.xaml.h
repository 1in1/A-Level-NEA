//
// MainPage.xaml.h
// Declaration of the MainPage class.
//

#pragma once

#include "MainPage.g.h"

namespace Image_Filtering_NEA
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	public ref class MainPage sealed
	{
	public:
		MainPage();
	private:

		void Load(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void Save(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

		void ApplyBoxBlur(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void ApplyGaussianBlur(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void ApplyInvert(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void ApplySobelFilter(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void ApplyHSVShift(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void ApplyGreyscaleFilter(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

		void DisplayOriginal(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void HideOriginal(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void Undo(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void UndoAll(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);

		void UpdateTracker(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void ToggleTracking(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);

		Platform::String^ ByteToHex(int t);
		Platform::String^ ByteToHex(int red, int green, int blue);

		void TextChanging_Gaussian(Windows::UI::Xaml::Controls::TextBox^ sender, Windows::UI::Xaml::Controls::TextBoxTextChangingEventArgs^ args);
		void TextChanging_Sobel(Windows::UI::Xaml::Controls::TextBox^ sender, Windows::UI::Xaml::Controls::TextBoxTextChangingEventArgs^ args);
		void TextChanging_Hue(Windows::UI::Xaml::Controls::TextBox^ sender, Windows::UI::Xaml::Controls::TextBoxTextChangingEventArgs^ args);
		void TextChanging_Sat(Windows::UI::Xaml::Controls::TextBox^ sender, Windows::UI::Xaml::Controls::TextBoxTextChangingEventArgs^ args);
		void TextChanging_Val(Windows::UI::Xaml::Controls::TextBox^ sender, Windows::UI::Xaml::Controls::TextBoxTextChangingEventArgs^ args);
	};
}

