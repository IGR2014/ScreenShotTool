////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		WIC-based image saving implementation
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


// Windows
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
// C++
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
// ScreenShotTool hook
#include "WICImage.hpp"


// ScreenShotTool hook code zone
namespace sst::wic {


	using Microsoft::WRL::ComPtr;


	/// @brief RAII COM apartment for the calling thread
	///
	/// WIC needs COM initialised. RPC_E_CHANGED_MODE means the thread already
	/// had an apartment of another kind, which is fine - we just must not
	/// uninitialise what we did not initialise.
	class com_apartment final {

		bool mOwned {false}; ///< Whether this object called ::CoInitializeEx()


	public:

		com_apartment() noexcept {
			const auto hr {::CoInitializeEx(nullptr, COINIT_MULTITHREADED)};
			mOwned = SUCCEEDED(hr);
		}

		com_apartment(const com_apartment&)			= delete;
		com_apartment(com_apartment&&)				= delete;
		auto operator=(const com_apartment&) -> com_apartment&	= delete;
		auto operator=(com_apartment&&) -> com_apartment&	= delete;

		~com_apartment() noexcept {
			if (mOwned) {
				::CoUninitialize();
			}
		}

	};


	// Encode a 32-bpp frame to a JPEG file through WIC
	auto saveJpeg(
		const std::filesystem::path& path,
		const std::uint32_t	     width,
		const std::uint32_t	     height,
		const std::uint32_t	     stride,
		std::span<const std::byte>   pixels,
		const pixel_order	     order
	) noexcept -> ::HRESULT {

		// Reject obviously malformed frames up front
		if ((0U == width) || (0U == height) || (stride < (width * 4U)) || (pixels.size() < static_cast<std::size_t>(stride) * height)) {
			return E_INVALIDARG;
		}

		const com_apartment com {};

		// Shared WIC entry point
		ComPtr<IWICImagingFactory> factory {};
		auto hr {::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))};
		if (FAILED(hr)) {
			return hr;
		}

		// Output file stream
		ComPtr<IWICStream> stream {};
		hr = factory->CreateStream(&stream);
		if (FAILED(hr)) {
			return hr;
		}
		hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
		if (FAILED(hr)) {
			return hr;
		}

		// JPEG encoder bound to the file stream
		ComPtr<IWICBitmapEncoder> encoder {};
		hr = factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder);
		if (FAILED(hr)) {
			return hr;
		}
		hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
		if (FAILED(hr)) {
			return hr;
		}

		// Single frame
		ComPtr<IWICBitmapFrameEncode> frame {};
		ComPtr<IPropertyBag2>	      props {};
		hr = encoder->CreateNewFrame(&frame, &props);
		if (FAILED(hr)) {
			return hr;
		}
		hr = frame->Initialize(props.Get());
		if (FAILED(hr)) {
			return hr;
		}
		hr = frame->SetSize(width, height);
		if (FAILED(hr)) {
			return hr;
		}

		// Ask for a 24-bpp BGR frame; WIC converts from our source format
		auto			 pixelFormat {GUID_WICPixelFormat24bppBGR};
		hr = frame->SetPixelFormat(&pixelFormat);
		if (FAILED(hr)) {
			return hr;
		}

		// WIC consumes non-const pixel data; the buffer is ours and not reused after
		auto* const	    data {const_cast<::BYTE*>(reinterpret_cast<const ::BYTE*>(pixels.data()))};
		const ::UINT	    bufferSize {stride * height};
		const auto&	    sourceFormat {(pixel_order::rgba == order) ? GUID_WICPixelFormat32bppRGBA : GUID_WICPixelFormat32bppBGRA};

		// Wrap the raw frame as a WIC bitmap, then convert into the encoder frame
		ComPtr<IWICBitmap> source {};
		hr = factory->CreateBitmapFromMemory(width, height, sourceFormat, stride, bufferSize, data, &source);
		if (FAILED(hr)) {
			return hr;
		}

		ComPtr<IWICFormatConverter> converter {};
		hr = factory->CreateFormatConverter(&converter);
		if (FAILED(hr)) {
			return hr;
		}
		hr = converter->Initialize(source.Get(), GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
		if (FAILED(hr)) {
			return hr;
		}

		hr = frame->WriteSource(converter.Get(), nullptr);
		if (FAILED(hr)) {
			return hr;
		}

		hr = frame->Commit();
		if (FAILED(hr)) {
			return hr;
		}
		return encoder->Commit();
	}


} // namespace sst::wic
