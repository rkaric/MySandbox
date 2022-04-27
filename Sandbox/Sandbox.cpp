#include "CppUnitTest.h"

#include <cstdint>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinUser.h>
#include <ObjIdl.h>
#include <gdiplus.h>
#include <gdiplusimaging.h>
#pragma comment (lib, "Gdiplus.lib")

// For _aligned_malloc, MSVC doesn't support C11's aligned_alloc() because of compatibility.
#include <corecrt_malloc.h>

#include "Sandbox.generated.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Sandbox
{
	void GetProjectPath(const std::wstring& delim, std::wstring& outPath)
	{
		wchar_t directoryBuf[MAX_PATH];
		size_t workingDirLength = GetCurrentDirectory(MAX_PATH, directoryBuf);
		Assert::IsTrue(workingDirLength);
		std::wstring tempPath(directoryBuf);
		size_t targetDirIndex = tempPath.find_last_of(delim);
		Assert::AreNotEqual(targetDirIndex, std::wstring::npos);
		tempPath = tempPath.substr(0, targetDirIndex);
		size_t archDirIndex = tempPath.find_last_of(delim);
		Assert::AreNotEqual(archDirIndex, std::wstring::npos);
		outPath.append(directoryBuf, archDirIndex + 1);
	}

	// https://docs.microsoft.com/en-us/windows/win32/gdiplus/-gdiplus-retrieving-the-class-identifier-for-an-encoder-use
	int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
	{
		UINT  num = 0;          // number of image encoders
		UINT  size = 0;         // size of the image encoder array in bytes

		Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

		Gdiplus::GetImageEncodersSize(&num, &size);
		if (size == 0)
			return -1;  // Failure

		pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
		if (pImageCodecInfo == NULL)
			return -1;  // Failure

		GetImageEncoders(num, size, pImageCodecInfo);

		for (UINT j = 0; j < num; ++j)
		{
			if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
			{
				*pClsid = pImageCodecInfo[j].Clsid;
				free(pImageCodecInfo);
				return j;  // Success
			}
		}

		free(pImageCodecInfo);
		return -1;  // Failure
	}

	TEST_CLASS(Sandbox)
	{
	public:
		TEST_METHOD(ISPCColorShift)
		{
			Gdiplus::GdiplusStartupInput startupInput;
			ULONG_PTR gdiPlusToken;

			Gdiplus::GdiplusStartup(&gdiPlusToken, &startupInput, NULL);

			std::wstring inImagePath;
			GetProjectPath(mPathDelim, inImagePath);
			std::wstring outImagePath(inImagePath);
			inImagePath.append(mInImagePath);
			outImagePath.append(mOutImagePath);

			Gdiplus::Bitmap* inBitmap = Gdiplus::Bitmap::FromFile(inImagePath.c_str(), false);
			if (inBitmap != NULL)
			{
				const size_t imageWidth = inBitmap->GetWidth();
				const size_t imageHeight = inBitmap->GetHeight();

				Gdiplus::Rect inRect(0, 0, imageWidth, imageHeight);
				Gdiplus::BitmapData inBitmapData;

				if (inBitmap->LockBits(&inRect, Gdiplus::ImageLockMode::ImageLockModeWrite, PixelFormat32bppARGB, &inBitmapData) == Gdiplus::Status::Ok)
				{
					ispc::ColorShift((uint32_t*)inBitmapData.Scan0,
						imageWidth,
						imageHeight,
						mRedWeight,
						mGreenWeight,
						mBlueWeight);

					inBitmap->UnlockBits(&inBitmapData);

					CLSID pngClsid;
					GetEncoderClsid(L"image/png", &pngClsid);
					inBitmap->Save(outImagePath.c_str(), &pngClsid, NULL);
				}
			}
			else
			{
				Gdiplus::GdiplusShutdown(gdiPlusToken);
				int error = GetLastError();
				Assert::Fail();
			}

			Gdiplus::GdiplusShutdown(gdiPlusToken);
		}

		TEST_METHOD(CPPColorShift)
		{
			Gdiplus::GdiplusStartupInput startupInput;
			ULONG_PTR gdiPlusToken;

			Gdiplus::GdiplusStartup(&gdiPlusToken, &startupInput, NULL);

			std::wstring inImagePath;
			GetProjectPath(mPathDelim, inImagePath);
			std::wstring outImagePath(inImagePath);
			inImagePath.append(mInImagePath);
			outImagePath.append(mOutImagePath);

			Gdiplus::Bitmap* inBitmap = Gdiplus::Bitmap::FromFile(inImagePath.c_str(), false);
			if (inBitmap != NULL)
			{
				const size_t imageWidth = inBitmap->GetWidth();
				const size_t imageHeight = inBitmap->GetHeight();

				Gdiplus::Rect inRect(0, 0, imageWidth, imageHeight);
				Gdiplus::BitmapData inBitmapData;

				if (inBitmap->LockBits(&inRect, Gdiplus::ImageLockMode::ImageLockModeWrite, PixelFormat32bppARGB, &inBitmapData) == Gdiplus::Status::Ok)
				{
					uint32_t* imageData = (uint32_t*)inBitmapData.Scan0;

					for (size_t y = 0; y < imageHeight; ++y)
					{
						for (size_t x = 0; x < imageWidth; ++x)
						{
							const size_t pixelIndex = (y * imageWidth) + x;

							// A = 0x000000FF
							// G = 0x0000FF00
							// R = 0x00FF0000
							// B = 0xFF000000
							uint32_t originalPixel = imageData[pixelIndex];
							float rValue = ((float)((originalPixel & 0x00FF0000) >> 16));
							float gValue = ((float)((originalPixel & 0x0000FF00) >> 8));
							float bValue = ((float)((originalPixel & 0xFF000000) >> 24));
							const uint32_t redShift = ((uint32_t)(rValue * mRedWeight) << 16) & 0x00FF0000;
							const uint32_t greenShift = ((uint32_t)(gValue * mGreenWeight) << 8) & 0x0000FF00;
							const uint32_t blueShift = ((uint32_t)(bValue * mBlueWeight) << 24) & 0xFF000000;
							originalPixel &= 0x000000FF;
							originalPixel |= redShift;
							originalPixel |= greenShift;
							originalPixel |= blueShift;
							imageData[pixelIndex] = originalPixel;
						}
					}

					inBitmap->UnlockBits(&inBitmapData);

					CLSID pngClsid;
					GetEncoderClsid(L"image/png", &pngClsid);
					inBitmap->Save(outImagePath.c_str(), &pngClsid, NULL);
				}
			}
			else
			{
				Gdiplus::GdiplusShutdown(gdiPlusToken);
				int error = GetLastError();
				Assert::Fail();
			}

			Gdiplus::GdiplusShutdown(gdiPlusToken);
		}

		TEST_METHOD(ISPCCopy)
		{
			float* inValues = new float[mSize];
			float* outValues = new float[mSize];

			Assert::IsNotNull(inValues, L"InValues");
			Assert::IsNotNull(outValues, L"OutValues");

			ispc::FillValues(inValues, outValues, 10.f, 0.f, mSize);
			ispc::SimpleCopy(inValues, outValues, mSize);

			for (size_t i = 0; i < mSize; ++i)
			{
				Assert::AreEqual(inValues[i], outValues[i], 0.f, L"ISPC Copy");
			}
		}

		TEST_METHOD(ISPCCopy_Aligned)
		{
			float* inValues = (float*)_aligned_malloc(mSize * sizeof(float), mAlignment);
			float* outValues = (float*)_aligned_malloc(mSize * sizeof(float), mAlignment);

			Assert::IsNotNull(inValues, L"InValues");
			Assert::IsNotNull(outValues, L"OutValues");

			ispc::FillValues(inValues, outValues, 10.f, 0.f, mSize);
			ispc::SimpleCopy(inValues, outValues, mSize);

			for (size_t i = 0; i < mSize; ++i)
			{
				Assert::AreEqual(inValues[i], outValues[i], 0.f, L"ISPC Copy");
			}
		}

		TEST_METHOD(CPPCopy)
		{
			float* inValues = new float[mSize];
			float* outValues = new float[mSize];

			Assert::IsNotNull(inValues, L"InValues");
			Assert::IsNotNull(outValues, L"OutValues");

			for (size_t i = 0; i < mSize; ++i)
			{
				inValues[i] = 10.f;
				outValues[i] = 0.f;
			}

			for (size_t i = 0; i < mSize; ++i)
			{
				outValues[i] = inValues[i];
			}

			for (size_t i = 0; i < mSize; ++i)
			{
				Assert::AreEqual(inValues[i], outValues[i], 0.f, L"CPP Copy");
			}
		}

		TEST_METHOD(CPPCopy_Aligned)
		{
			float* inValues = (float*)_aligned_malloc(mSize * sizeof(float), mAlignment);
			float* outValues = (float*)_aligned_malloc(mSize * sizeof(float), mAlignment);

			Assert::IsNotNull(inValues, L"InValues");
			Assert::IsNotNull(outValues, L"OutValues");

			for (size_t i = 0; i < mSize; ++i)
			{
				inValues[i] = 10.f;
				outValues[i] = 0.f;
			}

			for (size_t i = 0; i < mSize; ++i)
			{
				outValues[i] = inValues[i];
			}

			for (size_t i = 0; i < mSize; ++i)
			{
				Assert::AreEqual(inValues[i], outValues[i], 0.f, L"CPP Copy");
			}
		}

	private:
		const size_t mAlignment = 16;
		const size_t mSize = 10000 * mAlignment;

		const std::wstring mPathDelim = L"\\";
		const std::wstring mInImagePath = L"In.png";
		const std::wstring mOutImagePath = L"Out.png";

		const float mRedWeight = .5f;
		const float mGreenWeight = .3f;
		const float mBlueWeight = 1.f;
	};
}
