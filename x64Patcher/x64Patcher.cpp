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
#include <charconv>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
// ScreenShotTool common
#include <common/DLLInjector.hpp>


// ScreenShotTool patcher code zone
namespace sst {

	/// @brief Parsed command line
	struct patcher_args final {
		std::wstring dllPath {};	 ///< Path of the DLL to inject (-d)
		::DWORD	     processID {0UL}; ///< Target process ID (-f)
	};


	/// @brief Parse "-d <dll> -f <pid>" style arguments
	/// @param args Argument views (excluding argv[0])
	/// @return Parsed arguments, or std::nullopt if required values are missing
	[[nodiscard]]
	auto parseArgs(std::span<const std::wstring_view> args) noexcept -> std::optional<patcher_args> {

		patcher_args parsed {};

		for (std::size_t i {0U}; i < args.size(); ++i) {
			const auto arg {args[i]};
			// Options take the following token as their value
			if ((arg == L"-d") && ((i + 1U) < args.size())) {
				parsed.dllPath = args[++i];
			} else if ((arg == L"-f") && ((i + 1U) < args.size())) {
				const auto value {args[++i]};
				// Parse the PID from narrow chars (ASCII digits only)
				::DWORD pid {0UL};
				std::string narrow {};
				narrow.reserve(value.size());
				for (const auto wc : value) {
					narrow.push_back(static_cast<char>(wc));
				}
				const auto* const begin {narrow.data()};
				const auto* const end {narrow.data() + narrow.size()};
				if (std::from_chars(begin, end, pid).ec == std::errc {}) {
					parsed.processID = pid;
				}
			}
		}

		if (parsed.dllPath.empty() || (0UL == parsed.processID)) {
			return std::nullopt;
		}
		return parsed;
	}

} // namespace sst


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
