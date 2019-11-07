#pragma once

#include <vector>
#include <thread>
#include <math.h>

using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::Storage::Streams;
using namespace Windows::Graphics::Imaging;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::Storage::Streams;

//User defined class allowing for manipulation of raw pixel data
//Keep what user can see to a minimum!
class Bitmap
{
private:
	std::vector<unsigned char> PixelData[3];
	int PixelStackHead = 0;
	int PixelStackBase = 0;
	int NextStackHead();
	int ImageWidth;
	int ImageHeight;
	void CorrelateKernel(int*, int, int, double);
	void CorrelateKernel_P(double*, int, int, double, std::vector<unsigned char>* = NULL);
	void Correlate_SingleChannel(double*, int, int, double, std::vector<unsigned char>*, int);
	void ConvolveKernel_P(int*, int, int, double, std::vector<unsigned char>*);
	double GaussianFunction(double, int, int);
public:
	SoftwareBitmap^ GetSoftwareBitmap();
	SoftwareBitmapSource^ GetSoftwareBitmapSource();
	void LoadImage(SoftwareBitmapSource^*, StorageFile^);
	void SaveImage(StorageFile^);
	StorageFile^ OriginalFileHandle;

	void CopyFromArray(unsigned char*, int, int);
	int RetrieveValue(int x, int y, int channel);

	void Undo();

	void Invert();
	void Invert_P();
	void BoxBlur(int);
	void GaussianBlur(double);
	void Greyscale();
	void SobelFilter(int);
	void ApproxHSVTransform(double, double, double);
	//void SobelBlur();
};