////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		WIC-based image saving (replacement for legacy D3DX helpers)
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


#pragma once


// Windows
#include <windows.h>
#include <wincodec.h>
// C++
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>


// ScreenShotTool hook code zone
namespace sst::wic {


	/// @brief Source memory layout of a 32-bpp frame
	enum class pixel_order : std::uint8_t {
		bgra,	///< Blue, green, red, alpha (Direct3D 9 back buffers)
		rgba	///< Red, green, blue, alpha (Direct3D 11 R8G8B8A8 textures)
	};


	/// @brief Encode a 32-bpp frame to a JPEG file through WIC
	///
	/// Supersedes @c D3DXSaveSurfaceToFile / @c D3DX11SaveTextureToFile, which
	/// live only in the removed legacy DirectX SDK. Initialises COM for the
	/// calling thread if needed.
	/// @param path Output file path (".jpg")
	/// @param width Frame width in pixels
	/// @param height Frame height in pixels
	/// @param stride Source row pitch in bytes
	/// @param pixels Source pixel bytes (at least @c stride * @c height)
	/// @param order Channel order of the source pixels
	/// @return S_OK on success, otherwise the first failing HRESULT
	[[nodiscard]]
	auto saveJpeg(
		const std::filesystem::path& path,
		std::uint32_t		     width,
		std::uint32_t		     height,
		std::uint32_t		     stride,
		std::span<const std::byte>   pixels,
		pixel_order		     order
	) noexcept -> ::HRESULT;


} // namespace sst::wic
