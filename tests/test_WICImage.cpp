////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Integration test for the WIC JPEG encoder
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


// Windows
#include <windows.h>
// C++
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>
// ScreenShotTool hook
#include <dxHookDLL/WICImage.hpp>
// Tests
#include "TestFramework.hpp"


auto wmain() -> int {

	constexpr std::uint32_t width {16U};
	constexpr std::uint32_t height {16U};
	constexpr std::uint32_t stride {width * 4U};

	// A simple BGRA gradient
	std::vector<std::byte> pixels (static_cast<std::size_t>(stride) * height);
	for (std::uint32_t y {0U}; y < height; ++y) {
		for (std::uint32_t x {0U}; x < width; ++x) {
			auto* const px {pixels.data() + (static_cast<std::size_t>(y) * stride) + (static_cast<std::size_t>(x) * 4U)};
			px[0] = static_cast<std::byte>(x * 16U); // B
			px[1] = static_cast<std::byte>(y * 16U); // G
			px[2] = std::byte {0x80};		 // R
			px[3] = std::byte {0xFF};		 // A
		}
	}

	const auto path {std::filesystem::temp_directory_path() / L"sst_wic_test.jpg"};
	std::error_code ec {};
	std::filesystem::remove(path, ec);

	// Encoding succeeds
	const auto hr {sst::wic::saveJpeg(path, width, height, stride, std::span<const std::byte> {pixels}, sst::wic::pixel_order::bgra)};
	CHECK(SUCCEEDED(hr));

	// A non-empty file exists with the JPEG SOI marker (0xFF 0xD8)
	CHECK(std::filesystem::exists(path));
	if (std::filesystem::exists(path)) {
		CHECK(std::filesystem::file_size(path) > 0U);
		std::ifstream file {path, std::ios::binary};
		std::array<unsigned char, 2> magic {};
		file.read(reinterpret_cast<char*>(magic.data()), magic.size());
		CHECK(0xFFU == magic[0]);
		CHECK(0xD8U == magic[1]);
	}

	// Invalid arguments are rejected without crashing
	CHECK(E_INVALIDARG == sst::wic::saveJpeg(path, 0U, 0U, 0U, std::span<const std::byte> {}, sst::wic::pixel_order::bgra));

	std::filesystem::remove(path, ec);
	return sst::test::report("WICImage.saveJpeg");
}
