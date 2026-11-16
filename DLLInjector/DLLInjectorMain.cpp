////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		DLL injector GUI: finds a target game and injects the hook
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


// Enable visual styles for common controls (MSVC linker only)
#if defined(_MSC_VER)
	#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif


// Windows
#include <windows.h>
#include <tlhelp32.h>
// C++
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
// ScreenShotTool common
#include <common/Common.hpp>
#include <common/DLLInjector.hpp>


// ScreenShotTool injector code zone
namespace sst {


	using namespace std::string_view_literals;


	// UI text
	constexpr auto PROGRAM_TITLE {L"DLLInjector v0.2"sv};	///< Window / class title
	constexpr auto LABEL_NAME {L"Name:"sv};			///< Executable name label
	constexpr auto LABEL_PID {L"Process ID:"sv};		///< Process ID label
	constexpr auto BUTTON_OPEN {L"Open folder"sv};		///< Open-folder button caption
	constexpr auto STATUS_WAITING {L"Waiting for a supported game..."sv};	///< Idle status text
	constexpr auto HOTKEY_HINT {L"In game, press F10 or Print Screen to capture."sv};	///< Capture hint
	constexpr auto UI_FONT_FACE {L"Segoe UI"sv};		///< Control font face

	// File names, relative to the injector directory
	constexpr auto SCREENSHOTS_DIR {L"Screenshots"sv};	///< Screenshots subfolder
	constexpr auto HOOK_DLL_X86 {L"hookX86.dll"sv};		///< 32-bit hook DLL
	constexpr auto HOOK_DLL_X64 {L"hookX64.dll"sv};		///< 64-bit hook DLL
	constexpr auto PATCHER_EXE {L"x64Patcher.exe"sv};	///< 64-bit injection helper

	// Control identifiers and timer
	constexpr int	 ID_OPEN_FOLDER_BUTTON {1000};		///< Open-folder button command ID
	constexpr ::UINT WINDOW_TIMER {1U};			///< Target-polling timer ID

	/// @brief Processes the injector watches for
	constexpr std::array TARGET_PROCESSES {
		L"dx11.exe"sv,
		L"dx9.exe"sv,
		L"dx11 x64.exe"sv,
		L"dx9 x64.exe"sv,
		L"dota2.exe"sv,
		L"Hearthstone.exe"sv,
		L"League of Legends.exe"sv,
		L"WorldOfTanks.exe"sv
	};


	/// @brief A located target process
	struct found_process final {
		std::wstring_view name {};	 ///< Matched executable name
		::DWORD		  id {0UL};	 ///< Process ID
	};


	/// @brief Enable or disable a named privilege on the current process token
	/// @param privilege Privilege name (e.g. SE_DEBUG_NAME)
	/// @param enable Whether to enable (true) or disable (false)
	/// @return true on success
	[[nodiscard]]
	auto setPrivilege(const ::LPCWSTR privilege, const bool enable) noexcept -> bool {

		::HANDLE rawToken {nullptr};
		if (FALSE == ::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &rawToken)) {
			return false;
		}
		const scoped_handle token {rawToken};

		::LUID luid {};
		if (FALSE == ::LookupPrivilegeValueW(nullptr, privilege, &luid)) {
			return false;
		}

		::TOKEN_PRIVILEGES tp {};
		tp.PrivilegeCount	    = 1UL;
		tp.Privileges[0].Luid	    = luid;
		tp.Privileges[0].Attributes = enable ? static_cast<::DWORD>(SE_PRIVILEGE_ENABLED) : 0UL;

		return FALSE != ::AdjustTokenPrivileges(token.get(), FALSE, &tp, sizeof(tp), nullptr, nullptr);
	}

	/// @brief Whether a process runs as native 64-bit code
	/// @param processID Target process ID
	/// @return true if the process is 64-bit on a 64-bit OS
	[[nodiscard]]
	auto isProcess64Bit(const ::DWORD processID) noexcept -> bool {

		// 32-bit OS can only host 32-bit processes
		::SYSTEM_INFO info {};
		::GetNativeSystemInfo(&info);
		if ((PROCESSOR_ARCHITECTURE_AMD64 != info.wProcessorArchitecture) && (PROCESSOR_ARCHITECTURE_IA64 != info.wProcessorArchitecture)) {
			return false;
		}

		const scoped_handle process {::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processID)};
		if (!process) {
			return false;
		}

		// On a 64-bit OS, a process is 64-bit exactly when it is NOT running under WOW64
		::BOOL wow64 {FALSE};
		if (FALSE == ::IsWow64Process(process.get(), &wow64)) {
			return false;
		}
		return FALSE == wow64;
	}

	/// @brief Find the first running process from the watch list
	/// @param names Executable names to look for
	/// @return The located process, or std::nullopt if none is running
	[[nodiscard]]
	auto findProcess(const std::span<const std::wstring_view> names) noexcept -> std::optional<found_process> {

		const scoped_file_handle snapshot {::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0UL)};
		if (!snapshot) {
			return std::nullopt;
		}

		::PROCESSENTRY32W entry {};
		entry.dwSize = sizeof(entry);

		for (auto more {::Process32FirstW(snapshot.get(), &entry)}; FALSE != more; more = ::Process32NextW(snapshot.get(), &entry)) {
			for (const auto name : names) {
				if (name == entry.szExeFile) {
					return found_process {.name = name, .id = entry.th32ProcessID};
				}
			}
		}

		return std::nullopt;
	}


	/// @brief Anonymous namespace for GUI state
	namespace {

		std::filesystem::path	gBaseDir {};		///< Injector directory
		std::filesystem::path	gScreenshotsDir {};	///< Screenshots directory
		DLLInjector		gInjector {};		///< Shared injector instance
		std::optional<::DWORD>	gHookedID {};		///< Currently hooked process, if any

		::HWND gStatus {nullptr};	///< Live status line
		::HWND gExeName {nullptr};	///< Executable-name label
		::HWND gExePID {nullptr};	///< Process-ID label
		::HWND gHint {nullptr};		///< Hotkey hint line
		::HWND gPathEdit {nullptr};	///< Screenshots-path edit box
		::HWND gOpenButton {nullptr};	///< Open-folder button
		::HFONT gFont {nullptr};	///< Shared UI font (Segoe UI)


		/// @brief Set the live status line text
		/// @param text New status text
		void setStatus(const std::wstring_view text) noexcept {
			::SetWindowTextW(gStatus, text.data());
		}


		/// @brief Inject the matching hook DLL into a freshly found process
		/// @param window Owner window for message boxes
		/// @param target The located target process
		void injectInto(const ::HWND window, const found_process& target) noexcept {

			// Choose the DLL that matches the target's bitness
			const auto is64 {isProcess64Bit(target.id)};
			const auto dllPath {gBaseDir / (is64 ? HOOK_DLL_X64 : HOOK_DLL_X86)};

			if (is64) {
				// A 32-bit injector cannot inject into a 64-bit process: delegate to the helper
				const auto arguments {std::format(L"-d \"{}\" -f {}", dllPath.wstring(), target.id)};
				::ShellExecuteW(nullptr, nullptr, PATCHER_EXE.data(), arguments.c_str(), nullptr, SW_HIDE);
			} else {
				if (!gInjector.setProcessByID(target.id)) {
					::MessageBoxW(window, L"Process does not exist!", L"Error", MB_OK | MB_ICONERROR);
					return;
				}
				static_cast<void>(gInjector.setDLLByName(dllPath.wstring()));
				if (!gInjector.inject()) {
					::MessageBoxW(window, L"Injection failed!", L"Error", MB_OK | MB_ICONERROR);
					return;
				}
			}

			gHookedID = target.id;

			// Reflect the hooked process in the UI
			const auto arch {is64 ? L"64-bit"sv : L"32-bit"sv};
			setStatus(std::format(L"Hooked ({}) - capturing on hotkey.", arch));
			::SetWindowTextW(gExeName, std::format(L"{}\t\t{}", LABEL_NAME, target.name).c_str());
			::SetWindowTextW(gExePID, std::format(L"{}\t0x{:04X} ({})", LABEL_PID, target.id, target.id).c_str());
			::SetWindowTextW(window, std::format(L"{} - [{}]", PROGRAM_TITLE, target.name).c_str());
		}

		/// @brief Verify the hooked process is still alive, else reset the UI
		/// @param window Main window
		void refreshHookState(const ::HWND window) noexcept {

			if (!gHookedID.has_value()) {
				// Not hooked: look for a target to hook
				if (const auto target {findProcess(TARGET_PROCESSES)}; target.has_value()) {
					injectInto(window, *target);
				}
				return;
			}

			// Hooked: confirm the process is still running
			const scoped_handle process {::OpenProcess(SYNCHRONIZE, FALSE, *gHookedID)};
			if (process && (WAIT_TIMEOUT == ::WaitForSingleObject(process.get(), 0UL))) {
				return;
			}

			// Target gone: reset the UI
			gHookedID.reset();
			setStatus(STATUS_WAITING);
			::SetWindowTextW(window, PROGRAM_TITLE.data());
			::SetWindowTextW(gExeName, LABEL_NAME.data());
			::SetWindowTextW(gExePID, LABEL_PID.data());
		}

		/// @brief Create the child controls once the window exists
		/// @param window Main window
		void createControls(const ::HWND window) noexcept {

			const auto instance {std::bit_cast<::HINSTANCE>(::GetWindowLongPtrW(window, GWLP_HINSTANCE))};
			const auto child {static_cast<::DWORD>(WS_CHILD | WS_VISIBLE)};

			// A modern UI font (Win32 controls default to the old bitmap system font)
			gFont = ::CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, UI_FONT_FACE.data());

			gStatus	    = ::CreateWindowW(L"STATIC", STATUS_WAITING.data(), child, 14, 14, 404, 22, window, nullptr, instance, nullptr);
			gExeName    = ::CreateWindowW(L"STATIC", LABEL_NAME.data(), child, 14, 46, 404, 22, window, nullptr, instance, nullptr);
			gExePID	    = ::CreateWindowW(L"STATIC", LABEL_PID.data(), child, 14, 72, 404, 22, window, nullptr, instance, nullptr);
			gHint	    = ::CreateWindowW(L"STATIC", HOTKEY_HINT.data(), child, 14, 104, 404, 22, window, nullptr, instance, nullptr);
			gPathEdit   = ::CreateWindowW(L"EDIT", gScreenshotsDir.wstring().c_str(), child | WS_BORDER | ES_READONLY | ES_AUTOHSCROLL, 14, 140, 404, 24, window, nullptr, instance, nullptr);
			gOpenButton = ::CreateWindowW(L"BUTTON", BUTTON_OPEN.data(), child | WS_TABSTOP, 300, 174, 118, 30, window, std::bit_cast<::HMENU>(static_cast<std::intptr_t>(ID_OPEN_FOLDER_BUTTON)), instance, nullptr);

			// Apply the shared font to every control
			for (const auto control : {gStatus, gExeName, gExePID, gHint, gPathEdit, gOpenButton}) {
				::SendMessageW(control, WM_SETFONT, std::bit_cast<::WPARAM>(gFont), TRUE);
			}
		}

	} // namespace


	// Window procedure
	::LRESULT __stdcall MainWinProc(const ::HWND window, const ::UINT message, const ::WPARAM wParam, const ::LPARAM lParam) {

		switch (message) {

			case WM_CREATE: {
				::SetTimer(window, WINDOW_TIMER, 1000U, nullptr);
				if (!setPrivilege(SE_DEBUG_NAME, true)) {
					::MessageBoxW(window, L"Unable to acquire debug privilege for the process!", L"Critical error", MB_OK | MB_ICONERROR);
					::PostQuitMessage(0);
				}
				createControls(window);
			} break;

			case WM_TIMER: {
				if (WINDOW_TIMER == wParam) {
					refreshHookState(window);
				}
			} break;

			case WM_COMMAND: {
				if (ID_OPEN_FOLDER_BUTTON == LOWORD(wParam)) {
					::ShellExecuteW(nullptr, L"open", gScreenshotsDir.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
				}
			} break;

			case WM_DESTROY: {
				::KillTimer(window, WINDOW_TIMER);
				if (nullptr != gFont) {
					::DeleteObject(gFont);
					gFont = nullptr;
				}
				::PostQuitMessage(0);
			} break;

			default: return ::DefWindowProcW(window, message, wParam, lParam);

		}

		return ::DefWindowProcW(window, message, wParam, lParam);
	}


} // namespace sst


// GUI entry point
auto __stdcall wWinMain(const ::HINSTANCE instance, [[maybe_unused]] ::HINSTANCE prevInstance, [[maybe_unused]] ::LPWSTR cmdLine, const int showCmd) -> int {

	// Crisp text and correct metrics on high-DPI displays
	::SetProcessDPIAware();

	// Resolve the injector directory and the screenshots folder once
	std::array<wchar_t, MAX_PATH> moduleBuffer {};
	const auto moduleLength {::GetModuleFileNameW(instance, moduleBuffer.data(), static_cast<::DWORD>(moduleBuffer.size()))};
	sst::gBaseDir	     = std::filesystem::path {std::wstring_view {moduleBuffer.data(), moduleLength}}.parent_path();
	sst::gScreenshotsDir = sst::gBaseDir / sst::SCREENSHOTS_DIR;

	// Register the main window class
	::WNDCLASSW wc {};
	wc.lpfnWndProc	 = &sst::MainWinProc;
	wc.hInstance	 = instance;
	wc.hIcon	 = ::LoadIconW(nullptr, IDI_WINLOGO);
	wc.hbrBackground = std::bit_cast<::HBRUSH>(static_cast<std::intptr_t>(COLOR_WINDOW));
	wc.lpszClassName = sst::PROGRAM_TITLE.data();
	if (0 == ::RegisterClassW(&wc)) {
		return 1;
	}

	// Create and show the main window
	const auto window {::CreateWindowW(sst::PROGRAM_TITLE.data(), sst::PROGRAM_TITLE.data(), WS_BORDER | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 450, 250, nullptr, nullptr, instance, nullptr)};
	if (nullptr == window) {
		return 2;
	}
	::ShowWindow(window, showCmd);
	::UpdateWindow(window);

	// Standard message pump
	::MSG message {};
	while (::GetMessageW(&message, nullptr, 0U, 0U) > 0) {
		::TranslateMessage(&message);
		::DispatchMessageW(&message);
	}

	return static_cast<int>(message.wParam);
}
