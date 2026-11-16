////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		x64 helper: injects the hook DLL into a target process
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


// Windows
#include <windows.h>
// C++
#include <span>
#include <string_view>
#include <vector>
// ScreenShotTool common
#include <common/DLLInjector.hpp>
// ScreenShotTool patcher
#include "ArgParse.hpp"


// Program entry point
auto wmain(const int argc, wchar_t* argv[]) -> int {

	// Views over argv[1..], no copies
	std::vector<std::wstring_view> args {};
	args.reserve(static_cast<std::size_t>(argc));
	for (int i {1}; i < argc; ++i) {
		args.emplace_back(argv[i]);
	}

	const auto parsed {sst::parseArgs(args)};
	if (!parsed) {
		::MessageBoxW(nullptr, L"Usage: x64Patcher.exe -d <dll path> -f <process id>", L"ScreenShotTool", MB_OK | MB_ICONINFORMATION);
		return 1;
	}

	sst::DLLInjector injector {};
	if (!injector.setProcessByID(parsed->processID)) {
		::MessageBoxW(nullptr, L"Process does not exist!", L"Error", MB_OK | MB_ICONERROR);
		return 2;
	}
	static_cast<void>(injector.setDLLByName(parsed->dllPath));
	if (!injector.inject()) {
		::MessageBoxW(nullptr, L"Injection failed!", L"Error", MB_OK | MB_ICONERROR);
		return 3;
	}

	return 0;

}
