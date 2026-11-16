////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Tests for x64Patcher command-line parsing
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


// C++
#include <array>
#include <span>
#include <string_view>
// ScreenShotTool patcher
#include <x64Patcher/ArgParse.hpp>
// Tests
#include "TestFramework.hpp"


using namespace std::string_view_literals;


namespace {

	// Parse a fixed argument list (views, excluding argv[0])
	template<std::size_t N>
	auto parse(const std::array<std::wstring_view, N>& args) {
		return sst::parseArgs(std::span<const std::wstring_view> {args});
	}

}


auto wmain() -> int {

	// Well-formed arguments parse both values
	{
		const auto result {parse(std::array {L"-d"sv, L"C:\\hook.dll"sv, L"-f"sv, L"4321"sv})};
		CHECK(result.has_value());
		CHECK(result->dllPath == L"C:\\hook.dll");
		CHECK(result->processID == 4321UL);
	}

	// Order does not matter
	{
		const auto result {parse(std::array {L"-f"sv, L"7"sv, L"-d"sv, L"a.dll"sv})};
		CHECK(result.has_value());
		CHECK(result->processID == 7UL);
		CHECK(result->dllPath == L"a.dll");
	}

	// Missing DLL path -> rejected
	{
		const auto result {parse(std::array {L"-f"sv, L"10"sv})};
		CHECK(!result.has_value());
	}

	// Missing process ID -> rejected
	{
		const auto result {parse(std::array {L"-d"sv, L"a.dll"sv})};
		CHECK(!result.has_value());
	}

	// Zero process ID -> rejected (0 is never a valid target)
	{
		const auto result {parse(std::array {L"-d"sv, L"a.dll"sv, L"-f"sv, L"0"sv})};
		CHECK(!result.has_value());
	}

	// Non-numeric process ID -> rejected
	{
		const auto result {parse(std::array {L"-d"sv, L"a.dll"sv, L"-f"sv, L"abc"sv})};
		CHECK(!result.has_value());
	}

	// Dangling option with no value -> rejected
	{
		const auto result {parse(std::array {L"-d"sv})};
		CHECK(!result.has_value());
	}

	return sst::test::report("x64Patcher.parseArgs");
}
