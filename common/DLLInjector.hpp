////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Remote DLL injector (LoadLibrary / FreeLibrary technique)
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
#include <string>
#include <string_view>


// ScreenShotTool common code zone
namespace sst {


	/// @brief Injects a DLL into a target process and ejects it again
	///
	/// Injection writes the DLL path into the target address space and runs
	/// ::LoadLibraryW() there via a remote thread; ejection runs
	/// ::FreeLibrary() on the resulting module handle the same way.
	class DLLInjector final {

		std::wstring mDLLName {};	 ///< Full path of the DLL to inject
		::DWORD	     mProcessID {0UL}; ///< Target process ID
		::DWORD	     mDLLHandle {0UL}; ///< HMODULE of the injected DLL (as returned by LoadLibrary)


	public:

		/// @brief Default constructor (no target, no DLL)
		DLLInjector() noexcept = default;
		/// @brief Construct from a process name and a DLL path
		/// @param procName Executable name of the target process
		/// @param dllName Path of the DLL to inject
		DLLInjector(std::wstring_view procName, std::wstring_view dllName) noexcept;
		/// @brief Construct from a process ID and a DLL path
		/// @param procID Target process ID
		/// @param dllName Path of the DLL to inject
		DLLInjector(::DWORD procID, std::wstring_view dllName) noexcept;

		// Trivially copyable/movable: it only holds a string and two IDs
		DLLInjector(const DLLInjector&)			= default;
		DLLInjector(DLLInjector&&) noexcept		= default;
		auto operator=(const DLLInjector&) -> DLLInjector&	= default;
		auto operator=(DLLInjector&&) noexcept -> DLLInjector&	= default;
		/// @brief Destructor
		~DLLInjector() noexcept = default;

		/// @brief Select the target process by ID
		/// @param procID Non-zero process ID
		/// @return true on success (false if procID is 0)
		[[nodiscard]]
		auto setProcessByID(::DWORD procID) noexcept -> bool;
		/// @brief Select the target process by executable name
		/// @param procName Executable name to search for in the process list
		/// @return true if a matching process was found
		[[nodiscard]]
		auto setProcessByName(std::wstring_view procName) noexcept -> bool;

		/// @brief Set the path of the DLL to inject
		/// @param dllName DLL path
		/// @return true (always; retained for a uniform call-site style)
		auto setDLLByName(std::wstring_view dllName) noexcept -> bool;

		/// @brief Inject the configured DLL into the configured process
		/// @return true if the remote ::LoadLibraryW() ran to completion
		[[nodiscard]]
		auto inject() noexcept -> bool;
		/// @brief Eject the previously injected DLL
		/// @return true if the remote ::FreeLibrary() reported success
		[[nodiscard]]
		auto eject() noexcept -> bool;

	};


} // namespace sst
