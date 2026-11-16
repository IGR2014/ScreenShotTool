////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Inline function hook (trampoline) over a raw code patch
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
#include <array>
#include <cstddef>
#include <span>
#include <vector>


// ScreenShotTool hook code zone
namespace sst {


	/// @brief Multi-byte NOP encodings (index N - 2 holds the N-byte NOP)
	///
	/// Long NOPs let the patched prologue stay a legal instruction stream after
	/// the jump is written, so the trampoline's saved bytes disassemble cleanly.
	namespace nop {

		inline constexpr std::byte ONE {std::byte {0x90}};						///< 1 byte: NOP
		inline constexpr std::array TWO {std::byte {0x66}, std::byte {0x90}};				///< 2 bytes: 66 NOP
		inline constexpr std::array THREE {std::byte {0x66}, std::byte {0x66}, std::byte {0x90}};	///< 3 bytes: 66 66 NOP
		inline constexpr std::array FOUR {std::byte {0x0F}, std::byte {0x1F}, std::byte {0x40}, std::byte {0x00}}; ///< 4 bytes
		inline constexpr std::array FIVE {std::byte {0x0F}, std::byte {0x1F}, std::byte {0x44}, std::byte {0x00}, std::byte {0x00}}; ///< 5 bytes
		inline constexpr std::array SIX {std::byte {0x66}, std::byte {0x0F}, std::byte {0x1F}, std::byte {0x44}, std::byte {0x00}, std::byte {0x00}}; ///< 6 bytes
		inline constexpr std::array SEVEN {std::byte {0x0F}, std::byte {0x1F}, std::byte {0x80}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00}}; ///< 7 bytes
		inline constexpr std::array EIGHT {std::byte {0x0F}, std::byte {0x1F}, std::byte {0x84}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00}}; ///< 8 bytes
		inline constexpr std::array NINE {std::byte {0x66}, std::byte {0x0F}, std::byte {0x1F}, std::byte {0x84}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00}}; ///< 9 bytes
		inline constexpr std::array TEN {std::byte {0x66}, std::byte {0x66}, std::byte {0x0F}, std::byte {0x1F}, std::byte {0x84}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00}, std::byte {0x00}}; ///< 10 bytes

	} // namespace nop


	/// @brief Overwrite a code range with the longest possible NOP encodings
	/// @param target Writable code bytes to pad (already made writable by the caller)
	void fillWithNOPs(std::span<std::byte> target) noexcept;


	/// @brief Inline trampoline hook of a function prologue
	///
	/// install() overwrites the first @c patchSize bytes of the target with a
	/// jump to the detour and returns a callable trampoline (saved prologue +
	/// jump back) so the original behaviour is still reachable. uninstall()
	/// restores the original bytes.
	class InlineHook final {

		::LPBYTE		mTarget {nullptr};	///< Patched function entry point
		std::vector<std::byte>	mTrampoline {};	///< Saved prologue followed by a jump back
		std::size_t		mPatchSize {0U};	///< Number of original bytes overwritten
		bool			mInstalled {false};	///< Whether the patch is currently applied


	public:

		/// @brief Default constructor (no hook installed)
		InlineHook() noexcept = default;

		// Move-only: the trampoline owns a unique code patch
		InlineHook(const InlineHook&)			= delete;
		auto operator=(const InlineHook&) -> InlineHook&	= delete;
		InlineHook(InlineHook&&) noexcept			= default;
		auto operator=(InlineHook&&) noexcept -> InlineHook&	= default;

		/// @brief Destructor (restores the original bytes if still installed)
		~InlineHook() noexcept;

		/// @brief Install the trampoline
		/// @param originalFunction Function entry point to patch
		/// @param detourFunction Replacement function to jump to
		/// @param patchSize Number of prologue bytes to overwrite (>= jump size)
		/// @return Callable trampoline address, or nullptr if already installed
		[[nodiscard]]
		auto install(::LPBYTE originalFunction, ::LPBYTE detourFunction, std::size_t patchSize) noexcept -> ::LPCVOID;
		/// @brief Uninstall the trampoline and restore the original bytes
		/// @return true if a patch was removed (false if nothing was installed)
		auto uninstall() noexcept -> bool;

		/// @brief Whether the trampoline is currently installed
		[[nodiscard]]
		auto installed() const noexcept -> bool {
			return mInstalled;
		}

	};


} // namespace sst
