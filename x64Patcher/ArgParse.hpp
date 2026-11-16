////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		x64Patcher command-line parsing (header-only, testable)
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


#pragma once


// Windows
#include <windows.h>
// C++
#include <charconv>
#include <optional>
#include <span>
#include <string>
#include <string_view>


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
	inline auto parseArgs(const std::span<const std::wstring_view> args) noexcept -> std::optional<patcher_args> {

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
