////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Tests for InlineHook NOP padding
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


// C++
#include <array>
#include <cstddef>
#include <span>
#include <vector>
// ScreenShotTool hook
#include <dxHookDLL/InlineHook.hpp>
// Tests
#include "TestFramework.hpp"


// A valid NOP stream decodes to exactly one instruction per published width,
// so we check that fillWithNOPs reproduces the canonical encodings and never
// writes past the requested range.
namespace {

	// Canonical multi-byte NOP for a given exact width
	auto canonical(const std::size_t width) -> std::vector<std::byte> {
		using namespace sst::nop;
		switch (width) {
			case 1U: return {ONE};
			case 2U: return {TWO.begin(), TWO.end()};
			case 3U: return {THREE.begin(), THREE.end()};
			case 4U: return {FOUR.begin(), FOUR.end()};
			case 5U: return {FIVE.begin(), FIVE.end()};
			case 6U: return {SIX.begin(), SIX.end()};
			case 7U: return {SEVEN.begin(), SEVEN.end()};
			case 8U: return {EIGHT.begin(), EIGHT.end()};
			case 9U: return {NINE.begin(), NINE.end()};
			case 10U: return {TEN.begin(), TEN.end()};
			default: return {};
		}
	}

}


auto wmain() -> int {

	// Exact single-NOP widths 1..10 are emitted verbatim
	for (std::size_t width {1U}; width <= 10U; ++width) {
		std::vector<std::byte> buffer (width, std::byte {0xCC});
		sst::fillWithNOPs(buffer);
		CHECK(buffer == canonical(width));
	}

	// Larger ranges are tiled with 10-byte NOPs plus a remainder NOP,
	// and nothing is written outside the range (guard bytes stay 0xCC)
	for (std::size_t width {11U}; width <= 25U; ++width) {
		std::vector<std::byte> buffer (width + 2U, std::byte {0xCC});
		sst::fillWithNOPs(std::span {buffer}.subspan(0U, width));
		CHECK(buffer[width] == std::byte {0xCC});	// first guard byte untouched
		CHECK(buffer[width + 1U] == std::byte {0xCC}); // second guard byte untouched
		CHECK(buffer[0] == sst::nop::TEN[0]);		// starts with a 10-byte NOP
	}

	// Zero width is a no-op
	std::vector<std::byte> empty {};
	sst::fillWithNOPs(empty);
	CHECK(empty.empty());

	return sst::test::report("InlineHook.fillWithNOPs");
}
