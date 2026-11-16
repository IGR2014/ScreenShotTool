////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Minimal thread-safe wide-char file logger
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


#pragma once


// C++
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <string_view>
#include <utility>


// ScreenShotTool hook code zone
namespace sst {


	/// @brief Append-only UTF-16 log file with formatted output
	///
	/// Replaces the former raw @c std::wfstream* global and its scattered
	/// @c operator<< chains with std::format-based lines guarded by a mutex,
	/// so the two hook threads cannot interleave half-written lines.
	class Logger final {

		std::wofstream	   mStream {}; ///< Backing file stream
		mutable std::mutex mMutex {};	///< Serialises writes across hook threads


	public:

		/// @brief Default constructor (no file open yet)
		Logger() noexcept = default;

		// Non-copyable, non-movable (owns a stream and a mutex, used as a global)
		Logger(const Logger&)			= delete;
		Logger(Logger&&)			= delete;
		auto operator=(const Logger&) -> Logger&	= delete;
		auto operator=(Logger&&) -> Logger&		= delete;

		/// @brief Destructor
		~Logger() noexcept = default;

		/// @brief (Re)open the log file, truncating any previous contents
		/// @param path Log file path (parent directories must exist)
		/// @return true if the file was opened
		[[nodiscard]]
		auto open(const std::filesystem::path& path) noexcept -> bool;

		/// @brief Write one preformatted line (newline appended)
		/// @param line Text to append
		void line(std::wstring_view line) noexcept;

		/// @brief Write one std::format-style line (newline appended)
		/// @tparam Args Formatted argument types
		/// @param fmt Format string
		/// @param args Format arguments
		template<class... Args>
		void operator()(const std::wformat_string<Args...> fmt, Args&&... args) noexcept {
			line(std::format(fmt, std::forward<Args>(args)...));
		}

	};


	/// @brief Process-wide hook logger (defined in DLLMain.cpp)
	extern Logger gLog;


} // namespace sst
