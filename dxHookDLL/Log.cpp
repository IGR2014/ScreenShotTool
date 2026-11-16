////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Minimal thread-safe wide-char file logger implementation
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


// C++
#include <filesystem>
#include <ios>
#include <mutex>
#include <string_view>
// ScreenShotTool hook
#include "Log.hpp"


// ScreenShotTool hook code zone
namespace sst {


	// (Re)open the log file, truncating any previous contents
	auto Logger::open(const std::filesystem::path& path) noexcept -> bool {
		const std::scoped_lock lock {mMutex};
		mStream.open(path, std::ios::out | std::ios::trunc);
		return mStream.is_open();
	}

	// Write one preformatted line (newline appended)
	void Logger::line(const std::wstring_view line) noexcept {
		const std::scoped_lock lock {mMutex};
		if (mStream.is_open()) {
			mStream << line << L'\n';
			mStream.flush();
		}
	}


} // namespace sst
