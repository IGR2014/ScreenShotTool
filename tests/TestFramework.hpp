////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Tiny dependency-free unit test harness
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


#pragma once


// C++
#include <cstdio>
#include <string_view>


// ScreenShotTool test code zone
namespace sst::test {


	/// @brief Count of failed checks in the current test binary
	inline int gFailures {0};


	/// @brief Record a check result and print a message on failure
	/// @param condition Result of the checked expression
	/// @param expression Source text of the expression
	/// @param file Source file
	/// @param line Source line
	inline void check(const bool condition, const std::string_view expression, const std::string_view file, const int line) noexcept {
		if (!condition) {
			++gFailures;
			std::fprintf(stderr, "  FAIL: %.*s (%.*s:%d)\n",
				static_cast<int>(expression.size()), expression.data(),
				static_cast<int>(file.size()), file.data(), line);
		}
	}

	/// @brief Final report; return value is the process exit code
	/// @param suite Suite name for the summary line
	/// @return 0 if every check passed, 1 otherwise
	[[nodiscard]]
	inline auto report(const std::string_view suite) noexcept -> int {
		if (0 == gFailures) {
			std::fprintf(stderr, "[ PASS ] %.*s\n", static_cast<int>(suite.size()), suite.data());
			return 0;
		}
		std::fprintf(stderr, "[ FAIL ] %.*s (%d failed)\n", static_cast<int>(suite.size()), suite.data(), gFailures);
		return 1;
	}


} // namespace sst::test


/// @brief Assert that @p expr is true, recording a failure otherwise
#define CHECK(expr) ::sst::test::check((expr), #expr, __FILE__, __LINE__)
