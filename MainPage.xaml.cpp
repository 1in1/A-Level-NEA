//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"

#include "BitmapManipulation.h"

using namespace Image_Filtering_NEA;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

using namespace Concurrency;


// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409




Bitmap MainBitmap;
ImageSource^ TempSource = nullptr;
bool TrackingOn = false;
int Progress = 0;

MainPage::MainPage()
{
	InitializeComponent();
}

void MainPage::Load(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	//Define default file types, location, and view mode in window
	auto file_open_handle = ref new FileOpenPicker();
	file_open_handle->SuggestedStartLocation = PickerLocationId::PicturesLibrary;
	file_open_handle->ViewMode = PickerViewMode::Thumbnail;
	file_open_handle->FileTypeFilter->Append(".bmp");
	file_open_handle->FileTypeFilter->Append(".jpg");
	file_open_handle->FileTypeFilter->Append(".png");
	//etc..


	//Create task to handle Asynch behaviour, this structure comes up a lot
	auto chose_file_task = Concurrency::create_task(file_open_handle->PickSingleFileAsync());
	chose_file_task.then([this](StorageFile^ chosen_file_handle)
	{
		if (!chosen_file_handle)
		{
			//Operation was cancelled
			return;
		}
		MainBitmap.OriginalFileHandle = chosen_file_handle;

		//Get file as SoftwareBitmap class
		//Scoping is nasty but appears to be the only way to handle Asynch in cpp
		auto get_stream_task = Concurrency::create_task(chosen_file_handle->OpenAsync(FileAccessMode::Read));
		get_stream_task.then([this](IRandomAccessStream^ stream)
		{
			auto get_decoder_task = Concurrency::create_task(BitmapDecoder::CreateAsync(stream));
			get_decoder_task.then([this](BitmapDecoder^ decoder)
			{
				//Get width and height before passing to lambda; much less data than the whole decoder class
				int w = decoder->PixelWidth;
				int h = decoder->PixelHeight;
				BitmapTransform ^bt = ref new BitmapTransform;
				bt->InterpolationMode = BitmapInterpolationMode::NearestNeighbor;

				auto get_pixel_data_task = Concurrency::create_task(decoder->GetPixelDataAsync(BitmapPixelFormat::Bgra8,
					BitmapAlphaMode::Premultiplied,
					bt,
					ExifOrientationMode::RespectExifOrientation,
					ColorManagementMode::ColorManageToSRgb));
				get_pixel_data_task.then([this, w, h](PixelDataProvider^ pixel_data)
				{
					//This lambda retreives the actual pixel data, so we can apply matrix operations to it later

					Platform::Array<unsigned char, 1U>^ raw_pixel_data = pixel_data->DetachPixelData();


					MainBitmap.CopyFromArray(raw_pixel_data->Data, w, h);
					SoftwareBitmapSource^ image_source = MainBitmap.GetSoftwareBitmapSource();
					x_OriginalImage->Source = image_source;
					x_EditedImage->Source = image_source;
				});
			});
		});
	});
}
void MainPage::Save(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto file_extensions = ref new Platform::Collections::Vector<String^>();
	file_extensions->Append(".jpg");
	file_extensions->Append(".bmp");
	auto file_save_handle = ref new FileSavePicker;
	file_save_handle->SuggestedStartLocation = PickerLocationId::PicturesLibrary;
	file_save_handle->FileTypeChoices->Insert("JPEG files, BMP files", file_extensions);
	file_save_handle->SuggestedFileName = "Edited_image";

	auto chose_file_task = create_task(file_save_handle->PickSaveFileAsync());
	chose_file_task.then([this](StorageFile^ chosen_file_handle)
	{
		if (!chosen_file_handle)
		{
			//Operation was cancelled
			return;
		}

		MainBitmap.SaveImage(chosen_file_handle);
	});
}


void MainPage::ApplyBoxBlur(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	MainBitmap.BoxBlur(3);
	x_EditedImage->Source = MainBitmap.GetSoftwareBitmapSource();
}
void MainPage::ApplyGaussianBlur(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	try
	{
		double SD = _wtof(x_InputGaussianSD->Text->Data());
		if (SD > 8 || SD < 0)
		{
			x_OutputInfo->Text = "Gaussian blur standard deviation out of range; value must be within [0, 8].";
			return;
		}
		else
		{
			MainBitmap.GaussianBlur(SD);
			x_EditedImage->Source = MainBitmap.GetSoftwareBitmapSource();
			x_OutputInfo->Text = "Ready.";
		}
	}
	catch (Exception ^E)
	{}
}
void MainPage::ApplyInvert(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	MainBitmap.Invert_P();
	x_EditedImage->Source = MainBitmap.GetSoftwareBitmapSource();
	x_OutputInfo->Text = "Ready.";
}
void MainPage::ApplySobelFilter(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	try
	{
		int Boundary = _wtof(x_InputSobelBoundary->Text->Data());
		if (Boundary > 356 || Boundary < 0)
		{
			x_OutputInfo->Text = "Sobel boundary out of range; value must be within [0, 356].";
			return;
		}
		else
		{
			MainBitmap.SobelFilter(Boundary);
			x_EditedImage->Source = MainBitmap.GetSoftwareBitmapSource();
			x_OutputInfo->Text = "Ready.";
		}
	}
	catch (Exception ^E)
	{}
}
void MainPage::ApplyHSVShift(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	try
	{
		float Hue = _wtof(x_InputHue->Text->Data());
		float Sat = _wtof(x_InputSat->Text->Data());
		float Val = _wtof(x_InputVal->Text->Data());
		MainBitmap.ApproxHSVTransform(Hue, Sat, Val);
		x_EditedImage->Source = MainBitmap.GetSoftwareBitmapSource();
		x_OutputInfo->Text = "Ready.";
	}
	catch(Exception ^E)
	{}
}
void MainPage::ApplyGreyscaleFilter(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	MainBitmap.Greyscale();
	x_EditedImage->Source = MainBitmap.GetSoftwareBitmapSource();
	x_OutputInfo->Text = "Ready.";
}

void MainPage::DisplayOriginal(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	try {
		TempSource = x_EditedImage->Source;
		x_EditedImage->Source = x_OriginalImage->Source;
		x_OutputInfo->Text = "Click to return to the original.";
	}
	catch (Exception^ E)
	{}
}
void MainPage::HideOriginal(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	if (TempSource != nullptr)
	{
		x_EditedImage->Source = TempSource;
		x_OutputInfo->Text = "Ready.";
	}
}
void MainPage::Undo(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	try
	{
		MainBitmap.Undo();
		x_EditedImage->Source = MainBitmap.GetSoftwareBitmapSource();
		x_OutputInfo->Text = "Ready.";
	}
	catch (Platform::COMException ^E)
	{

	}
}
void MainPage::UndoAll(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	try
	{
		//Creates a dialog to ask user for confirmation then proceeds with same code as original load
		ContentDialog^ restore_dialog = ref new ContentDialog();
		restore_dialog->Title = "Undo all";
		restore_dialog->Content = "Revert to original?";
		restore_dialog->CloseButtonText = "No";
		restore_dialog->SecondaryButtonText = "Yes";
		auto dialog_box_task = Concurrency::create_task(restore_dialog->ShowAsync());
		dialog_box_task.then([this](ContentDialogResult result)
		{
			if (result == ContentDialogResult::Secondary)
			{
				//Get file as SoftwareBitmap class
				//Scoping is nasty but appears to be the only way to handle Asynch in cpp
				auto get_stream_task = Concurrency::create_task(MainBitmap.OriginalFileHandle->OpenAsync(FileAccessMode::Read));
				get_stream_task.then([this](IRandomAccessStream^ stream)
				{
					auto get_decoder_task = Concurrency::create_task(BitmapDecoder::CreateAsync(stream));
					get_decoder_task.then([this](BitmapDecoder^ decoder)
					{
						//Get width and height before passing to lambda; much less data than the whole decoder class
						int w = decoder->PixelWidth;
						int h = decoder->PixelHeight;

						auto get_pixel_data_task = Concurrency::create_task(decoder->GetPixelDataAsync(BitmapPixelFormat::Bgra8,
							BitmapAlphaMode::Premultiplied,
							ref new BitmapTransform(),
							ExifOrientationMode::RespectExifOrientation,
							ColorManagementMode::ColorManageToSRgb));
						get_pixel_data_task.then([this, w, h](PixelDataProvider^ pixel_data)
						{
							//This lambda retreives the actual pixel data, so we can apply matrix operations to it later

							Platform::Array<unsigned char, 1U>^ raw_pixel_data = pixel_data->DetachPixelData();


							MainBitmap.CopyFromArray(raw_pixel_data->Data, w, h);
							SoftwareBitmapSource^ image_source = MainBitmap.GetSoftwareBitmapSource();
							x_OriginalImage->Source = image_source;
							x_EditedImage->Source = image_source;
						});
					});
				});
			}
		});
	}
	catch (Platform::COMException ^E)
	{

	}
}


void MainPage::UpdateTracker(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	if (TrackingOn)
	{
		Windows::UI::Input::PointerPoint^ pp = e->GetCurrentPoint(x_EditedImage);
		unsigned char blue = MainBitmap.RetrieveValue(pp->Position.X, pp->Position.Y, 0);
		unsigned char green = MainBitmap.RetrieveValue(pp->Position.X, pp->Position.Y, 1);
		unsigned char red = MainBitmap.RetrieveValue(pp->Position.X, pp->Position.Y, 2);
		//std::string hex;
		//char hex_string[7];
		//std::sprintf(&hex_string[0], "%X", (int)red);
		//std::sprintf(&hex_string[2], "%X", (int)blue);
		//std::sprintf(&hex_string[4], "%X", (int)green);
		//hex_string[6] = 0; //Null terminator
		//hex = hex_string;
		//Text_PixelColour->Text = hex;
		//hex = "#" + hex;
		Windows::UI::Color col;
		col.A = 0xff;
		col.B = (byte)blue;
		col.G = (byte)green;
		col.R = (byte)red;
		x_RectPixelColour->Fill = ref new SolidColorBrush(col);

		//x_OutputPixelColour->IsReadOnly = false;
		x_OutputPixelColour->Text = ByteToHex(red, green, blue);
		//x_OutputPixelColour->IsReadOnly = true;
	}
}
void MainPage::ToggleTracking(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e)
{
	if (e->Key == Windows::System::VirtualKey::X)
	{
		TrackingOn = !TrackingOn;
		if (TrackingOn) x_RectHighlight->Opacity = 100;
		else x_RectHighlight->Opacity = 0;
	}
}



Platform::String^ MainPage::ByteToHex(int red, int green, int blue)
{
	Platform::String^ ret = ref new Platform::String;
	ret += ByteToHex(red / 16);
	ret += ByteToHex(red % 16);
	ret += ByteToHex(green / 16);
	ret += ByteToHex(green % 16);
	ret += ByteToHex(blue / 16);
	ret += ByteToHex(blue % 16);
	return ret;
}
Platform::String^ MainPage::ByteToHex(int t)
{
	Platform::String^ ret = ref new Platform::String;
	switch (t)
	{
	case 0:
		ret = "0";
		break;
	case 1:
		ret = "1";
		break;
	case 2:
		ret = "2";
		break;
	case 3:
		ret = "3";
		break;
	case 4:
		ret = "4";
		break;
	case 5:
		ret = "5";
		break;
	case 6:
		ret = "6";
		break;
	case 7:
		ret = "7";
		break;
	case 8:
		ret = "8";
		break;
	case 9:
		ret = "9";
		break;
	case 10:
		ret = "A";
		break;
	case 11:
		ret = "B";
		break;
	case 12:
		ret = "C";
		break;
	case 13:
		ret = "D";
		break;
	case 14:
		ret = "E";
		break;
	case 15:
		ret = "F";
	}
	return ret;
}




void Image_Filtering_NEA::MainPage::TextChanging_Gaussian(Windows::UI::Xaml::Controls::TextBox^ sender, Windows::UI::Xaml::Controls::TextBoxTextChangingEventArgs^ args)
{
	if (x_InputGaussianSD->Text->Length() > 0)
	{
		char16 c = x_InputGaussianSD->Text->Data()[x_InputGaussianSD->SelectionStart - 1];
		int SS = x_InputGaussianSD->SelectionStart;
		if (c < 46 || c == 47 || c > 57)
		{
			x_InputGaussianSD->Text = ref new String(x_InputGaussianSD->Text->Data(), x_InputGaussianSD->SelectionStart - 1) + ref new String(x_InputGaussianSD->Text->Data() + x_InputGaussianSD->SelectionStart,
				x_InputGaussianSD->Text->Length() - x_InputGaussianSD->SelectionStart);
			x_InputGaussianSD->SelectionStart = SS - 1;
		}
		if (c == 46)
		{
			for (int i = 0; i < x_InputGaussianSD->Text->Length() - 1; i++)
			{
				if (i == SS - 1) continue;
				if (x_InputGaussianSD->Text->Data()[i] == 46)
				{
					x_InputGaussianSD->Text = ref new String(x_InputGaussianSD->Text->Data(), x_InputGaussianSD->SelectionStart - 1) + ref new String(x_InputGaussianSD->Text->Data() + x_InputGaussianSD->SelectionStart,
						x_InputGaussianSD->Text->Length() - x_InputGaussianSD->SelectionStart);
					x_InputGaussianSD->SelectionStart = SS - 1;
					break;
				}
			}
		}
	}
	if (x_InputGaussianSD->Text->Length() == 0)
	{
		x_InputGaussianSD->Text = L"0";
		x_InputGaussianSD->SelectionStart = 1;
	}
}
void Image_Filtering_NEA::MainPage::TextChanging_Sobel(Windows::UI::Xaml::Controls::TextBox^ sender, Windows::UI::Xaml::Controls::TextBoxTextChangingEventArgs^ args)
{
	if (x_InputSobelBoundary->Text->Length() > 0)
	{
		char16 c = x_InputSobelBoundary->Text->Data()[x_InputSobelBoundary->Text->Length() - 1];
		if (c < 48 || c > 57)
		{
			x_InputSobelBoundary->Text = ref new String(x_InputSobelBoundary->Text->Data(), x_InputSobelBoundary->Text->Length() - 1);;
			x_InputSobelBoundary->SelectionStart = x_InputSobelBoundary->Text->Length();
		}
	}
	if (x_InputSobelBoundary->Text->Length() == 0)
	{
		x_InputSobelBoundary->Text = L"0";
		x_InputSobelBoundary->SelectionStart = 1;
	}
}
void Image_Filtering_NEA::MainPage::TextChanging_Hue(Windows::UI::Xaml::Controls::TextBox^ sender, Windows::UI::Xaml::Controls::TextBoxTextChangingEventArgs^ args)
{
	if (x_InputHue->Text->Length() > 0)
	{
		char16 c = x_InputHue->Text->Data()[x_InputHue->SelectionStart - 1];
		int SS = x_InputHue->SelectionStart;
		if (c < 46 || c == 47 || c > 57)
		{
			x_InputHue->Text = ref new String(x_InputHue->Text->Data(), x_InputHue->SelectionStart - 1) + ref new String(x_InputHue->Text->Data() + x_InputHue->SelectionStart,
				x_InputHue->Text->Length() - x_InputHue->SelectionStart);
			x_InputHue->SelectionStart = SS - 1;
		}
		if (c == 46)
		{
			for (int i = 0; i < x_InputHue->Text->Length() - 1; i++)
			{
				if (i == SS - 1) continue;
				if (x_InputHue->Text->Data()[i] == 46)
				{
					x_InputHue->Text = ref new String(x_InputHue->Text->Data(), x_InputHue->SelectionStart - 1) + ref new String(x_InputHue->Text->Data() + x_InputHue->SelectionStart,
						x_InputHue->Text->Length() - x_InputHue->SelectionStart);
					x_InputHue->SelectionStart = SS - 1;
					break;
				}
			}
		}
	}
	if (x_InputHue->Text->Length() == 0)
	{
		x_InputHue->Text = L"0";
		x_InputHue->SelectionStart = 1;
	}
}
void Image_Filtering_NEA::MainPage::TextChanging_Sat(Windows::UI::Xaml::Controls::TextBox^ sender, Windows::UI::Xaml::Controls::TextBoxTextChangingEventArgs^ args)
{
	if (x_InputSat->Text->Length() > 0)
	{
		char16 c = x_InputSat->Text->Data()[x_InputSat->SelectionStart - 1];
		int SS = x_InputSat->SelectionStart;
		if (c < 46 || c == 47 || c > 57)
		{
			x_InputSat->Text = ref new String(x_InputSat->Text->Data(), x_InputSat->SelectionStart - 1) + ref new String(x_InputSat->Text->Data() + x_InputSat->SelectionStart,
				x_InputSat->Text->Length() - x_InputSat->SelectionStart);
			x_InputSat->SelectionStart = SS - 1;
		}
		if (c == 46)
		{
			for (int i = 0; i < x_InputSat->Text->Length() - 1; i++)
			{
				if (i == SS - 1) continue;
				if (x_InputSat->Text->Data()[i] == 46)
				{
					x_InputSat->Text = ref new String(x_InputSat->Text->Data(), x_InputSat->SelectionStart - 1) + ref new String(x_InputSat->Text->Data() + x_InputSat->SelectionStart,
						x_InputSat->Text->Length() - x_InputSat->SelectionStart);
					x_InputSat->SelectionStart = SS - 1;
					break;
				}
			}
		}
	}
	if (x_InputSat->Text->Length() == 0)
	{
		x_InputSat->Text = L"0";
		x_InputSat->SelectionStart = 1;
	}
}
void Image_Filtering_NEA::MainPage::TextChanging_Val(Windows::UI::Xaml::Controls::TextBox^ sender, Windows::UI::Xaml::Controls::TextBoxTextChangingEventArgs^ args)
{
	if (x_InputVal->Text->Length() > 0)
	{
		char16 c = x_InputVal->Text->Data()[x_InputVal->SelectionStart - 1];
		int SS = x_InputVal->SelectionStart;
		if (c < 46 || c == 47 || c > 57)
		{
			x_InputVal->Text = ref new String(x_InputVal->Text->Data(), x_InputVal->SelectionStart - 1) + ref new String(x_InputVal->Text->Data() + x_InputVal->SelectionStart,
				x_InputVal->Text->Length() - x_InputVal->SelectionStart);
			x_InputVal->SelectionStart = SS - 1;
		}
		if (c == 46)
		{
			for (int i = 0; i < x_InputVal->Text->Length() - 1; i++)
			{
				if (i == SS - 1) continue;
				if (x_InputVal->Text->Data()[i] == 46)
				{
					x_InputVal->Text = ref new String(x_InputVal->Text->Data(), x_InputVal->SelectionStart - 1) + ref new String(x_InputVal->Text->Data() + x_InputVal->SelectionStart,
						x_InputVal->Text->Length() - x_InputVal->SelectionStart);
					x_InputVal->SelectionStart = SS - 1;
					break;
				}
			}
		}
	}
	if (x_InputVal->Text->Length() == 0)
	{
		x_InputVal->Text = L"0";
		x_InputVal->SelectionStart = 1;
	}
}
