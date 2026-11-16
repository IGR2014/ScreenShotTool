////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Remote DLL injector implementation
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


// Windows
#include <windows.h>
#include <tlhelp32.h>
// C++
#include <bit>
#include <string>
#include <string_view>
// ScreenShotTool common
#include <common/Common.hpp>
#include <common/DLLInjector.hpp>


// ScreenShotTool common code zone
namespace sst {


	// Access rights requested on the target process (open once, reuse everywhere)
	constexpr auto PROCESS_ACCESS {
		PROCESS_CREATE_THREAD	|
		PROCESS_QUERY_INFORMATION	|
		PROCESS_VM_OPERATION	|
		PROCESS_VM_READ		|
		PROCESS_VM_WRITE
	};


	// Construct from a process name and a DLL path
	DLLInjector::DLLInjector(const std::wstring_view procName, const std::wstring_view dllName) noexcept {
		// Discarded: a constructor cannot report failure, callers re-check before inject()
		static_cast<void>(setProcessByName(procName));
		static_cast<void>(setDLLByName(dllName));
	}

	// Construct from a process ID and a DLL path
	DLLInjector::DLLInjector(const ::DWORD procID, const std::wstring_view dllName) noexcept {
		static_cast<void>(setProcessByID(procID));
		static_cast<void>(setDLLByName(dllName));
	}

	// Select the target process by ID
	auto DLLInjector::setProcessByID(const ::DWORD procID) noexcept -> bool {
		// A process ID of zero is never a valid injection target
		if (0UL == procID) {
			return false;
		}
		mProcessID = procID;
		return true;
	}

	// Select the target process by executable name
	auto DLLInjector::setProcessByName(const std::wstring_view procName) noexcept -> bool {
		// Snapshot of all processes (RAII-closed on every return path)
		const scoped_file_handle snapshot {::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0UL)};
		if (!snapshot) {
			mProcessID = 0UL;
			return false;
		}

		::PROCESSENTRY32W entry {};
		entry.dwSize = sizeof(entry);

		// Walk the process list looking for a matching executable name
		for (auto more {::Process32FirstW(snapshot.get(), &entry)}; FALSE != more; more = ::Process32NextW(snapshot.get(), &entry)) {
			if (procName == entry.szExeFile) {
				return setProcessByID(entry.th32ProcessID);
			}
		}

		return false;
	}

	// Set the path of the DLL to inject
	auto DLLInjector::setDLLByName(const std::wstring_view dllName) noexcept -> bool {
		mDLLName = dllName;
		return true;
	}

	// Inject the configured DLL into the configured process
	auto DLLInjector::inject() noexcept -> bool {
		// Need both a target and a DLL
		if ((0UL == mProcessID) || mDLLName.empty()) {
			return false;
		}

		// Open the target process (RAII-closed on scope exit)
		const scoped_handle process {::OpenProcess(PROCESS_ACCESS, FALSE, mProcessID)};
		if (!process) {
			return false;
		}

		// Bytes needed for the DLL path, terminator included
		const auto bytes {(mDLLName.size() + 1U) * sizeof(wchar_t)};

		// Reserve a buffer for the DLL path inside the target address space
		auto* const remotePath {::VirtualAllocEx(process.get(), nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)};
		if (nullptr == remotePath) {
			return false;
		}
		// Free that remote buffer no matter how we leave this function
		const scope_exit freeRemote {[&process, remotePath]() noexcept {
			::VirtualFreeEx(process.get(), remotePath, 0U, MEM_RELEASE);
		}};

		// Copy the DLL path into the target process
		if (FALSE == ::WriteProcessMemory(process.get(), remotePath, mDLLName.c_str(), bytes, nullptr)) {
			return false;
		}

		// Run LoadLibraryW(remotePath) in the target process
		const scoped_handle thread {::CreateRemoteThread(process.get(), nullptr, 0U, std::bit_cast<::LPTHREAD_START_ROUTINE>(&::LoadLibraryW), remotePath, 0U, nullptr)};
		if (!thread) {
			return false;
		}

		// Wait for LoadLibraryW to return and capture the resulting HMODULE
		::WaitForSingleObject(thread.get(), INFINITE);
		static_cast<void>(::GetExitCodeThread(thread.get(), &mDLLHandle));

		return true;
	}

	// Eject the previously injected DLL
	auto DLLInjector::eject() noexcept -> bool {
		// Need a target and something previously injected
		if ((0UL == mProcessID) || (0UL == mDLLHandle)) {
			return false;
		}

		const scoped_handle process {::OpenProcess(PROCESS_ACCESS, FALSE, mProcessID)};
		if (!process) {
			return false;
		}

		// Run FreeLibrary(mDLLHandle) in the target process
		const scoped_handle thread {::CreateRemoteThread(process.get(), nullptr, 0U, std::bit_cast<::LPTHREAD_START_ROUTINE>(&::FreeLibrary), std::bit_cast<::LPVOID>(static_cast<::UINT_PTR>(mDLLHandle)), 0U, nullptr)};
		if (!thread) {
			return false;
		}

		::WaitForSingleObject(thread.get(), INFINITE);

		// FreeLibrary returns BOOL (non-zero on success) as the thread exit code
		::DWORD result {0UL};
		static_cast<void>(::GetExitCodeThread(thread.get(), &result));
		// Forget the handle so a second eject() is a no-op
		mDLLHandle = 0UL;

		return 0UL != result;
	}


} // namespace sst
