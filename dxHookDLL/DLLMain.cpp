////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Hook DLL entry point and global definitions
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
#include <atomic>
#include <filesystem>
// ScreenShotTool common
#include <common/Common.hpp>
// ScreenShotTool hook
#include "DX9Screenshoter.hpp"
#include "DX11Screenshoter.hpp"
#include "GLScreenshoter.hpp"
#include "Globals.hpp"
#include "Log.hpp"


// ScreenShotTool hook code zone
namespace sst {


	// Global definitions declared in Log.hpp / Globals.hpp
	Logger			gLog {};
	std::filesystem::path	gScreenDir {};


	/// @brief Anonymous namespace for DLL-local state
	namespace {

		/// @brief Shared "keep the hook threads running" flag
		std::atomic<bool> gKeepRunning {true};

		/// @brief Directory of this DLL (base for Logs/ and Screenshots/)
		/// @param instance This module's instance handle
		/// @return Directory containing the DLL
		[[nodiscard]]
		auto moduleDirectory(const ::HMODULE instance) noexcept -> std::filesystem::path {
			std::array<wchar_t, MAX_PATH> buffer {};
			const auto length {::GetModuleFileNameW(instance, buffer.data(), static_cast<::DWORD>(buffer.size()))};
			return std::filesystem::path {std::wstring_view {buffer.data(), length}}.parent_path();
		}

	} // namespace


} // namespace sst


// Hook DLL entry point
::BOOL __stdcall DllMain(const ::HINSTANCE instance, const ::DWORD reason, [[maybe_unused]] ::LPVOID reserved) {

	switch (reason) {

		case DLL_PROCESS_ATTACH: {

			// Loader lock is held here: do only cheap setup, defer work to threads
			::DisableThreadLibraryCalls(instance);

			const auto base {sst::moduleDirectory(instance)};
			sst::gScreenDir = base / L"Screenshots";

			// Make sure the output directories exist
			std::error_code ec {};
			std::filesystem::create_directories(sst::gScreenDir, ec);
			std::filesystem::create_directories(base / L"Logs", ec);

			static_cast<void>(sst::gLog.open(base / L"Logs" / L"Log.txt"));
			sst::gLog.line(L"======================================");
			sst::gLog.line(L"ScreenShotTool hook attached.");
			sst::gLog.line(L"======================================");

			// One capture thread per supported graphics API
			static_cast<void>(::CreateThread(nullptr, 0U, &sst::HookDX9, &sst::gKeepRunning, 0U, nullptr));
			static_cast<void>(::CreateThread(nullptr, 0U, &sst::HookDX11, &sst::gKeepRunning, 0U, nullptr));
			static_cast<void>(::CreateThread(nullptr, 0U, &sst::HookOpenGL, &sst::gKeepRunning, 0U, nullptr));

		} break;

		case DLL_PROCESS_DETACH: {

			// Ask the capture threads to stop looping
			sst::gKeepRunning.store(false);
			sst::gLog.line(L"ScreenShotTool hook detached.");

		} break;

		default: break;

	}

	return TRUE;

}
