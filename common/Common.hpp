////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Shared modern C++ helpers (Win32 RAII, HRESULT utils)
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
#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>


// ScreenShotTool common code zone
namespace sst {


	/// @brief Which "no handle" sentinel a wrapper uses
	///
	/// ::OpenProcess() yields nullptr on failure, whereas snapshot and file
	/// APIs yield INVALID_HANDLE_VALUE; a pointer value cannot be a template
	/// argument, so the sentinel is selected by this tag instead.
	enum class handle_sentinel : bool {
		null,			///< nullptr means "no handle"
		invalid_handle_value	///< INVALID_HANDLE_VALUE means "no handle"
	};


	/// @brief Owning RAII wrapper around a Win32 ::HANDLE
	///
	/// Closes the handle on destruction, move-only.
	/// @tparam SENTINEL Which value is treated as "no handle" (not closed)
	template<handle_sentinel SENTINEL>
	class unique_handle final {

		/// @brief The sentinel value for this instantiation
		/// @return Handle value meaning "empty"
		[[nodiscard]]
		static auto invalid() noexcept -> ::HANDLE {
			if constexpr (handle_sentinel::invalid_handle_value == SENTINEL) {
				return INVALID_HANDLE_VALUE;
			} else {
				return nullptr;
			}
		}

		::HANDLE mHandle {invalid()}; ///< Owned handle


	public:

		/// @brief Empty handle
		unique_handle() noexcept = default;
		/// @brief Adopt a raw handle
		/// @param handle Raw handle to take ownership of
		constexpr explicit unique_handle(const ::HANDLE handle) noexcept :
			mHandle {handle} {}

		// Move-only (a handle has a single owner)
		unique_handle(const unique_handle&)		  = delete;
		auto operator=(const unique_handle&) -> unique_handle& = delete;

		/// @brief Move constructor
		/// @param other Handle to steal from
		unique_handle(unique_handle&& other) noexcept :
			mHandle {std::exchange(other.mHandle, invalid())} {}

		/// @brief Move assignment
		/// @param other Handle to steal from
		/// @return Reference to this
		auto operator=(unique_handle&& other) noexcept -> unique_handle& {
			if (this != &other) {
				reset(std::exchange(other.mHandle, invalid()));
			}
			return *this;
		}

		/// @brief Destructor (closes the owned handle)
		~unique_handle() noexcept {
			reset();
		}

		/// @brief Check whether a valid handle is held
		/// @return true if the handle is usable
		[[nodiscard]]
		auto valid() const noexcept -> bool {
			return invalid() != mHandle;
		}

		/// @brief Bool conversion mirroring valid()
		[[nodiscard]]
		explicit operator bool() const noexcept {
			return valid();
		}

		/// @brief Access the raw handle (for Win32 calls)
		/// @return Raw handle value
		[[nodiscard]]
		constexpr auto get() const noexcept -> ::HANDLE {
			return mHandle;
		}

		/// @brief Release ownership without closing
		/// @return Raw handle the caller is now responsible for
		[[nodiscard]]
		auto release() noexcept -> ::HANDLE {
			return std::exchange(mHandle, invalid());
		}

		/// @brief Close the current handle and optionally adopt a new one
		/// @param handle New handle to own (default: none)
		void reset(const ::HANDLE handle = invalid()) noexcept {
			if (valid()) {
				::CloseHandle(mHandle);
			}
			mHandle = handle;
		}

	};


	/// @brief Handle whose invalid sentinel is nullptr (e.g. ::OpenProcess())
	using scoped_handle = unique_handle<handle_sentinel::null>;
	/// @brief Handle whose invalid sentinel is INVALID_HANDLE_VALUE (e.g. snapshots)
	using scoped_file_handle = unique_handle<handle_sentinel::invalid_handle_value>;


	/// @brief RAII owner of a HMODULE loaded with ::LoadLibrary()
	class scoped_library final {

		::HMODULE mModule {nullptr}; ///< Owned module


	public:

		/// @brief Empty module
		constexpr scoped_library() noexcept = default;
		/// @brief Adopt a raw module handle
		/// @param module Raw module to own
		constexpr explicit scoped_library(const ::HMODULE module) noexcept :
			mModule {module} {}

		// Move-only
		scoped_library(const scoped_library&)			= delete;
		auto operator=(const scoped_library&) -> scoped_library&	= delete;

		/// @brief Move constructor
		/// @param other Module to steal from
		constexpr scoped_library(scoped_library&& other) noexcept :
			mModule {std::exchange(other.mModule, nullptr)} {}

		/// @brief Move assignment
		/// @param other Module to steal from
		/// @return Reference to this
		constexpr auto operator=(scoped_library&& other) noexcept -> scoped_library& {
			if (this != &other) {
				if (nullptr != mModule) {
					::FreeLibrary(mModule);
				}
				mModule = std::exchange(other.mModule, nullptr);
			}
			return *this;
		}

		/// @brief Destructor (frees the module)
		~scoped_library() noexcept {
			if (nullptr != mModule) {
				::FreeLibrary(mModule);
			}
		}

		/// @brief Load a library by name
		/// @param name Library name passed to ::LoadLibraryW()
		/// @return Owning wrapper (invalid on failure)
		[[nodiscard]]
		static auto load(const std::wstring_view name) noexcept -> scoped_library {
			return scoped_library {::LoadLibraryW(name.data())};
		}

		/// @brief Whether a valid module is held
		[[nodiscard]]
		constexpr auto valid() const noexcept -> bool {
			return nullptr != mModule;
		}

		/// @brief Raw module handle
		[[nodiscard]]
		constexpr auto get() const noexcept -> ::HMODULE {
			return mModule;
		}

		/// @brief Typed lookup of an exported symbol
		/// @tparam F Function pointer type of the export
		/// @param symbol Export name (ANSI, as required by ::GetProcAddress())
		/// @return Function pointer, or nullptr if not found
		template<class F>
		requires std::is_pointer_v<F>
		[[nodiscard]]
		auto proc(const std::string_view symbol) const noexcept -> F {
			return reinterpret_cast<F>(::GetProcAddress(mModule, symbol.data()));
		}

	};


	/// @brief Scope guard running a callable on destruction (RAII for cleanup)
	/// @tparam F Callable type invoked once at scope exit
	template<std::invocable F>
	class scope_exit final {

		F    mFunc;		   ///< Deferred action
		bool mActive {true}; ///< Whether the action still fires


	public:

		/// @brief Defer a callable
		/// @param func Action to run at scope exit
		constexpr explicit scope_exit(F func) noexcept :
			mFunc {std::move(func)} {}

		// Non-copyable, non-movable (used as a local guard only)
		scope_exit(const scope_exit&)			= delete;
		scope_exit(scope_exit&&)			= delete;
		auto operator=(const scope_exit&) -> scope_exit&	= delete;
		auto operator=(scope_exit&&) -> scope_exit&		= delete;

		/// @brief Destructor (runs the deferred action unless dismissed)
		~scope_exit() noexcept {
			if (mActive) {
				mFunc();
			}
		}

		/// @brief Cancel the deferred action
		constexpr void dismiss() noexcept {
			mActive = false;
		}

	};


} // namespace sst
