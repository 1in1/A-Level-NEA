#include "pch.h"

#include "BitmapManipulation.h"
#include <thread>
#include <math.h>

#define pi 3.14159265

using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::Storage::Streams;
using namespace Windows::System::Threading;

using namespace Concurrency;


int Bitmap::NextStackHead()
{
	return (PixelStackHead + 1) % 3;
}


void Bitmap::Undo()
{
	if (PixelData[(PixelStackHead + 2) % 3].size() > 0)
	{
		PixelStackHead = (PixelStackHead + 2) % 3;
	}
}

void Bitmap::Invert()
{
	//This is a simple inversion function that flips each pixel in place
	//Iterating over each block of pixel data (note, this means 4, one for each channel for each pixel)
	//it performs an XOR with a byte of 1s

	int a = 1;
	for (std::vector<unsigned char>::iterator i = PixelData[PixelStackHead].begin(); i < PixelData[PixelStackHead].end(); ++i, a++)
	{
		if (a % 4)	*i = *i ^ 0xff;
	}
}
void Bitmap::Invert_P()
{
	//Alternative function to perform the task in parallel using the correlation function written, and scaling
	double k = 1;
	CorrelateKernel_P(&k, 1, 1, -1);
	PixelStackHead = NextStackHead();
}
void Bitmap::BoxBlur(int Size)
{
	if (!(Size % 2)) return;
	//Don't attempt to blur if the blur size isn't an odd number; it would cause a
	//shift in the image

	else {
		//Create an array of 1s to act as the correlation kernel
		double* kernel = new double[Size*Size];
		for (int i = 0; i < Size*Size; i++)
		{
			kernel[i] = 1;
		}
		CorrelateKernel_P(kernel, Size, Size, (double)1 / (double)(Size*Size));
		delete[] kernel;
	}
	PixelStackHead = NextStackHead();
}
double Bitmap::GaussianFunction(double Variance, int x, int y)
{
	//Private function to return the value of the Gaussian function at the given location
	return ((1 / (2 * pi*Variance)) * exp(-((x*x + y*y) / (2 * Variance))));
}
void Bitmap::GaussianBlur(double SD)
{
	double variance = SD*SD;
	int size = ceil(6 * SD);
	//6 standard deviations should be enough to be practically zero
	//Make the size odd to avoid image shifts
	if (!(size % 2)) size++;

	//Allocate memory to what will become the kernel, before dynamically creating it
	double* kernel = new double[size*size];
	int i = 0;
	int mid = size / 2;
	double val;
	for (int y = 0; y <= mid; y++)
	{
		for (int x = 0; x <= mid; x++)
		{
			val = GaussianFunction(variance, x - mid, y - mid);
			//There is symmetry in the kernel, we don't need to calculate 4 times
			//We are dereferencing the area of memory at a known offset from the start
			//of an array; we can just calculate the value and deference it 

			*(kernel + (y*size + x)) = val;
			*(kernel + (y*size + (size - 1 - x))) = val;
			*(kernel + ((size - 1 - y)*size + x)) = val;
			*(kernel + ((size - 1 - y)*size + (size - 1 - x))) = val;
		}
	}

	CorrelateKernel_P(kernel, size, size, 1);
	PixelStackHead = NextStackHead();
}
void Bitmap::Greyscale()
{
	//We won't be able to go through CorrelateKernel here, so create a new vector to return
	std::vector<unsigned char> newPixelData;
	newPixelData.resize(ImageWidth * ImageHeight * 4);
	double value = 0;
	double intensity = 0;
	//Average the RGB channel values, and use this as intensity value
	//Write this intensity value to all colour channels to get grey tone

	for (int y = 0; y < ImageHeight; y++)
	{
		for (int x = 0; x < ImageWidth; x++)
		{
			//For each pixel
			value = 0;
			//Loop over channels
			for (int channelNo = 0; channelNo < 3; channelNo++)
			{
				value += (double)RetrieveValue(x, y, channelNo);
			}
			for (int channelNo = 0; channelNo < 3; channelNo++)
			{
				newPixelData[(y*ImageWidth + x) * 4 + channelNo] = (unsigned char)(value / (double)3);
			}
			//Alpha channel needs to stay at max
			newPixelData[(y*ImageWidth + x) * 4 + 3] = (unsigned char)0xff;
		}
	}

	PixelData[NextStackHead()] = newPixelData;
	PixelStackHead = NextStackHead();
}
void Bitmap::SobelFilter(int Boundary)
{
	//Greyscale will be needed to avoid having to find 3 different derivatives
	//It does however mean we will only be checking intensity gradients
	Greyscale();

	//These predefined kernels give approximate derivatives
	int PRE_Gx[9] = { 1, 0, -1, 2, 0, -2, 1, 0, -1 };
	int PRE_Gy[9] = { 1, 2, 1, 0, 0, 0, -1, -2, -1 };
	std::vector<unsigned char> Gx, Gy;
	ConvolveKernel_P(PRE_Gx, 3, 3, 1, &Gx);
	ConvolveKernel_P(PRE_Gy, 3, 3, 1, &Gy);
	//We now have Gx and Gy as approximate deriviatives in horizontal and vertical direction

	std::vector<unsigned char> G_Magnitude;
	int x, y, s;

	//We will compare the gradient magnitude (ie. not in x or y dimensions) to
	//the boundary provided, and if is above, we will write a white pixel, if below,
	//a black pixel
	for (int i = 0; i < ImageWidth*ImageHeight * 4; i++)
	{
		x = Gx.at(i);
		y = Gy.at(i);
		s = sqrt(x*x + y*y);
		if (s < Boundary)
		{
			G_Magnitude.push_back(0x00);
		}
		else
		{
			G_Magnitude.push_back(0xff);
		}
	}
	PixelData[PixelStackHead] = G_Magnitude;
}
void Bitmap::ApproxHSVTransform(double Hue, double Sat, double Val)
{
	double VSU = Val * Sat * cos(Hue * pi / 180);
	double VSW = Val * Sat * sin(Hue * pi / 180);

	std::vector<unsigned char> newPixelData;
	newPixelData.resize(ImageWidth * ImageHeight * 4);

	//This is a horrendous matrix multiplication; as I only need to do this once in the whole program I'm
	//willing to write this out here. It is done with a precalculated kernel
	double out_blue, out_green, out_red;
	for (int y = 0; y < ImageHeight; y++)
	{
		for (int x = 0; x < ImageWidth; x++)
		{
			out_blue = 0;
			out_green = 0;
			out_red = 0;

			out_blue += (double)RetrieveValue(x, y, 2) * (.299*Val - .3*VSU + 1.25*VSW);
			out_blue += (double)RetrieveValue(x, y, 1) * (.587*Val - .588*VSU - 1.05*VSW);
			out_blue += (double)RetrieveValue(x, y, 0) * (.114*Val + .886*VSU - .203*VSW);

			out_green += (double)RetrieveValue(x, y, 2) * (.299*Val - .299*VSU - .328*VSW);
			out_green += (double)RetrieveValue(x, y, 1) * (.587*Val + .413*VSU + .035*VSW);
			out_green += (double)RetrieveValue(x, y, 0) * (.114*Val - .114*VSU + .292*VSW);

			out_red += (double)RetrieveValue(x, y, 2) * (.299*Val + .701*VSU + .168*VSW);
			out_red += (double)RetrieveValue(x, y, 1) * (.587*Val - .587*VSU + .330*VSW);
			out_red += (double)RetrieveValue(x, y, 0) * (.114*Val - .114*VSU - .497*VSW);

			newPixelData[(y*ImageWidth + x) * 4] = out_blue;
			newPixelData[(y*ImageWidth + x) * 4 + 1] = out_green;
			newPixelData[(y*ImageWidth + x) * 4 + 2] = out_red;
			newPixelData[(y*ImageWidth + x) * 4 + 3] = (unsigned char)0xff;
		}
	}

	PixelData[NextStackHead()] = newPixelData;
	PixelStackHead = NextStackHead();
}

SoftwareBitmap^ Bitmap::GetSoftwareBitmap()
{
	//We need to write to a buffer, and create a SoftwareBitmap from it
	DataWriter^ writer = ref new DataWriter();
	//Slow, but I don't know how to pass the whole C-style buffer into this awful new Windows stuff
	for (std::vector<unsigned char>::iterator i = PixelData[PixelStackHead].begin(); i < PixelData[PixelStackHead].end(); i += 1)
	{
		writer->WriteByte(*i);
	}
	IBuffer^ buffer = writer->DetachBuffer();
	writer = nullptr;

	//SetBitmapAsync doesn't accept bitmaps without positive W/H, BGRA8, or ones that use Straight alpha, so we set as appropriate
	SoftwareBitmap^ software_bitmap = ref new SoftwareBitmap(BitmapPixelFormat::Bgra8, ImageWidth, ImageHeight, BitmapAlphaMode::Premultiplied);

	//This line may throw a "Not enough memory for response" error;
	//normally this means the image is the wrong size, it can be too small and throw this!
	software_bitmap->CopyFromBuffer(buffer);
	buffer = nullptr;

	return software_bitmap;
}
SoftwareBitmapSource^ Bitmap::GetSoftwareBitmapSource()
{
	try
	{

		//We need to write to a buffer, and create a SoftwareBitmap from it
		DataWriter^ writer = ref new DataWriter();
		//Slow, but I don't know how to pass the whole C-style buffer into this awful new Windows stuff
		for (std::vector<unsigned char>::iterator i = PixelData[PixelStackHead].begin(); i < PixelData[PixelStackHead].end(); i += 1)
		{
			writer->WriteByte(*i);
		}
		IBuffer^ buffer = writer->DetachBuffer();
		writer = nullptr;

		//SetBitmapAsync doesn't accept bitmaps without positive W/H, BGRA8, or ones that use Straight alpha, so we set as appropriate
		SoftwareBitmap^ software_bitmap = ref new SoftwareBitmap(BitmapPixelFormat::Bgra8, ImageWidth, ImageHeight, BitmapAlphaMode::Premultiplied);

		//This line may throw a "Not enough memory for response" error;
		//normally this means the image is the wrong size, it can be too small and throw this!
		software_bitmap->CopyFromBuffer(buffer);
		buffer = nullptr;

		//Create the source, write to it. This operation is Async, so I should really wait for it to complete,
		//but it's tricky in C++ and it tends to return bloody quick, so hasn't been an issue.
		//Will get round to it
		SoftwareBitmapSource^ image_source = ref new SoftwareBitmapSource();
		auto get_source_task = create_task(image_source->SetBitmapAsync(software_bitmap));

		return image_source;
	}
	catch(Platform::Exception ^E)
	{
		return nullptr;
	}
}

void Bitmap::CopyFromArray(unsigned char* incoming_pixels, int Width, int Height)
{
	//Straight copy of data from one array to the other
	//Documentation says this could be eased by using Platform::ArrayReference
	//but I can't get that to work.

	PixelData[PixelStackHead].clear();
	int length = Width * Height * 4;
	PixelData[PixelStackHead].reserve(length);
	for (int i = 0; i < length; i++)
	{
		PixelData[PixelStackHead].push_back(*(incoming_pixels + i));
	}
	PixelData[PixelStackHead].shrink_to_fit();
	ImageWidth = Width;
	ImageHeight = Height;
}
int Bitmap::RetrieveValue(int X, int Y, int Channel)
{
	//Differing implementations here

	//Null values outside borders
	/*
	if (Location.X < 0 || Location.X >= ImageWidth || Location.Y < 0 || Location.Y >= ImageHeight)
	{
	return 0;
	}
	else
	{
	return PixelData[(Location.Y * ImageWidth + Location.X) * 4 + Channel];
	}
	*/


	//Repeated edge values outside borders (recursive)
	if (X < 0)
	{
		return RetrieveValue(0, Y, Channel);
	}
	else if (X >= ImageWidth)
	{
		return RetrieveValue(ImageWidth - 1, Y, Channel);
	}

	if (Y < 0)
	{
		return RetrieveValue(X, 0, Channel);
	}
	else if (Y >= ImageHeight)
	{
		return RetrieveValue(X, ImageHeight - 1, Channel);
	}

	return PixelData[PixelStackHead][(Y * ImageWidth + X) * 4 + Channel];
}


void Bitmap::CorrelateKernel(int* Kernel, int kWidth, int kHeight, double ScaleFactor)
{
	//THIS APPLIES CORRELATION, NOT CONVOLUTION

	//Ignore kernels with even dimensions
	if (!(kWidth % 2) || !(kHeight % 2)) return;
	//Apply kernel to rest of the image then to special cases
	int kernelCentreX = (kWidth - 1) / 2;
	int kernelCentreY = (kHeight - 1) / 2;
	std::vector<unsigned char> newPixelData;
	newPixelData.reserve(ImageWidth * ImageHeight * 4);

	
	double value = 0;
	unsigned char test_container = 0;
	for (int y = 0; y < ImageHeight; y++)
	{
		for (int x = 0; x < ImageWidth; x++)
		{
			//For each pixel
			for (int channelNo = 0; channelNo < 3; channelNo++)
			{
				value = 0;
				for (int ky = 0; ky < kHeight; ky++)
				{
					for (int kx = 0; kx < kWidth; kx++)
					{
						//We get the value of the datum at each address near the current pixel,
						//then multiply it by the value of the kernel at that point
						value += (double)RetrieveValue(x + kx - kernelCentreX, y + ky - kernelCentreY, channelNo) * Kernel[ky*kWidth + kx];
					}
				}
				test_container = (value * ScaleFactor);
				newPixelData.push_back(test_container);
				value = 0;
			}
			//No sense running the whole nested loop just for alpha channel
			newPixelData.push_back(0xff);
		}
	}
	PixelData[NextStackHead()] = newPixelData;
	PixelStackHead = NextStackHead();
}
void Bitmap::Correlate_SingleChannel(double* Kernel, int kWidth, int kHeight, double ScaleFactor, std::vector<unsigned char>* data, int channelNo)
{
	//Not to be run other than by CorrelateKernel_P

	int kernelCentreX = (kWidth - 1) / 2;
	int kernelCentreY = (kHeight - 1) / 2;
	double value = 0;
	for (int y = 0; y < ImageHeight; y++)
	{
		for (int x = 0; x < ImageWidth; x++)
		{
			for (int ky = 0; ky < kHeight; ky++)
			{
				for (int kx = 0; kx < kWidth; kx++)
				{
					//We get the value of the datum at each address near the current pixel,
					//then multiply it by the value of the kernel at that point
					value += (double)RetrieveValue(x + kx - kernelCentreX, y + ky - kernelCentreY, channelNo) * Kernel[ky*kWidth + kx];
				}
			}
			data->at((y*ImageWidth + x) * 4 + channelNo) = (unsigned char)(value * ScaleFactor);
			value = 0;
		}
	}
}
void Bitmap::CorrelateKernel_P(double* Kernel, int kWidth, int kHeight, double ScaleFactor, std::vector<unsigned char>* output)
{
	if (!(kWidth % 2) || !(kHeight % 2)) return;

	std::vector<unsigned char> newPixelData;
	newPixelData.resize(ImageWidth * ImageHeight * 4);

	//Initiate three parallel threads to apply kernel to different colour channels
	std::thread blue(&Bitmap::Correlate_SingleChannel, this, Kernel, kWidth, kHeight, ScaleFactor, &newPixelData, 0);
	std::thread green(&Bitmap::Correlate_SingleChannel, this, Kernel, kWidth, kHeight, ScaleFactor, &newPixelData, 1);
	std::thread red(&Bitmap::Correlate_SingleChannel, this, Kernel, kWidth, kHeight, ScaleFactor, &newPixelData, 2);

	//Handle alpha channel in this block
	for (int y = 0; y < ImageHeight; y++)
	{
		for (int x = 0; x < ImageWidth; x++)
		{
			newPixelData[(y*ImageWidth + x) * 4 + 3] = (unsigned char)0xff;
		}
	}

	blue.join();
	green.join();
	red.join();

	//If we've been provided with an output pointer, then dump the result there; otherwise
	//change the value of the Bitmap's PixelData
	if (!output)
	{
		PixelData[NextStackHead()] = newPixelData;
	}
	else
	{
		*output = newPixelData;
	}
}
void Bitmap::ConvolveKernel_P(int* Kernel, int kWidth, int kHeight, double ScaleFactor, std::vector<unsigned char>* output = NULL)
{
	double* InvKernel = new double[kWidth*kHeight];
	for (int i = 0; i < kWidth*kHeight; i++)
	{
		InvKernel[i] = Kernel[kWidth*kHeight - 1 - i];
	}
	CorrelateKernel_P(InvKernel, kWidth, kHeight, ScaleFactor, output);
}

void Bitmap::LoadImage(SoftwareBitmapSource^* target, StorageFile^ chosen_file_handle)
{
	OriginalFileHandle = chosen_file_handle;

	//Get file as SoftwareBitmap class
	//Scoping is nasty but appears to be the only way to handle Asynch in cpp
	auto get_stream_task = create_task(chosen_file_handle->OpenAsync(FileAccessMode::Read));
	get_stream_task.then([this, target](IRandomAccessStream^ stream)
	{
		auto get_decoder_task = create_task(BitmapDecoder::CreateAsync(stream));
		get_decoder_task.then([this, target](BitmapDecoder^ decoder)
		{
			//Get width and height before passing to lambda; much less data than the whole decoder class
			ImageWidth = decoder->PixelWidth;
			ImageHeight = decoder->PixelHeight;

			auto get_pixel_data_task = create_task(decoder->GetPixelDataAsync(BitmapPixelFormat::Bgra8,
				BitmapAlphaMode::Premultiplied,
				ref new BitmapTransform(),
				ExifOrientationMode::RespectExifOrientation,
				ColorManagementMode::ColorManageToSRgb));
			get_pixel_data_task.then([this, target](PixelDataProvider^ pixel_data)
			{
				//This lambda retreives the actual pixel data, so we can apply matrix operations to it later
				PixelStackHead = NextStackHead();
				Platform::Array<unsigned char, 1U>^ raw_pixel_data = pixel_data->DetachPixelData();
				CopyFromArray(raw_pixel_data->Data, ImageWidth, ImageHeight);

				*(target) = GetSoftwareBitmapSource();

			});
		});
	});
}
void Bitmap::SaveImage(StorageFile^ chosen_file_handle)
{
		if (!chosen_file_handle)
		{
			//Operation was cancelled
			return;
		}

		//Get file as SoftwareBitmap class
		//Scoping is nasty but appears to be the only way to handle Asynch in cpp
		auto get_stream_task = create_task(chosen_file_handle->OpenAsync(FileAccessMode::ReadWrite));
		get_stream_task.then([this](IRandomAccessStream^ stream)
		{
			auto get_encoder_task = create_task(BitmapEncoder::CreateAsync(BitmapEncoder::JpegEncoderId, stream));
			get_encoder_task.then([this](BitmapEncoder^ encoder)
			{
				SoftwareBitmap^ sb = GetSoftwareBitmap();
				encoder->SetSoftwareBitmap(sb);

				auto flush_task = create_task(encoder->FlushAsync());
				flush_task.then([this]
				{
				});

			});
		});
}
