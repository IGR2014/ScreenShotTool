////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Inline function hook (trampoline) implementation
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


// Windows
#include <windows.h>
// C++
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
// ScreenShotTool hook
#include "InlineHook.hpp"


// ScreenShotTool hook code zone
namespace sst {


	// Size in bytes of the absolute jump written into a prologue
#ifdef _WIN64
	// x64: push rax; movabs rax, addr; xchg [rsp], rax; ret
	constexpr std::size_t JUMP_SIZE {16U};
#else
	// x86: jmp rel32
	constexpr std::size_t JUMP_SIZE {5U};
#endif


	// Overwrite a code range with the longest possible NOP encodings
	void fillWithNOPs(const std::span<std::byte> target) noexcept {
		auto rest {target};
		// Lay down 10-byte NOPs while they fit
		while (rest.size() >= nop::TEN.size()) {
			std::ranges::copy(nop::TEN, rest.begin());
			rest = rest.subspan(nop::TEN.size());
		}
		// Fill the 1..9-byte remainder with a single NOP of the exact width
		switch (rest.size()) {
			case 9U: std::ranges::copy(nop::NINE, rest.begin()); break;
			case 8U: std::ranges::copy(nop::EIGHT, rest.begin()); break;
			case 7U: std::ranges::copy(nop::SEVEN, rest.begin()); break;
			case 6U: std::ranges::copy(nop::SIX, rest.begin()); break;
			case 5U: std::ranges::copy(nop::FIVE, rest.begin()); break;
			case 4U: std::ranges::copy(nop::FOUR, rest.begin()); break;
			case 3U: std::ranges::copy(nop::THREE, rest.begin()); break;
			case 2U: std::ranges::copy(nop::TWO, rest.begin()); break;
			case 1U: rest[0] = nop::ONE; break;
			default: break; // Nothing left to pad
		}
	}


	// Write an absolute jump to @c destination at @c where (arch-specific encoding)
	static void writeJump([[maybe_unused]] std::byte* const where, [[maybe_unused]] const void* const destination) noexcept {
#ifdef _WIN64
		// push rax
		where[0]  = std::byte {0x50};
		// movabs rax, destination
		where[1]  = std::byte {0x48};
		where[2]  = std::byte {0xB8};
		const auto addr {std::bit_cast<std::uint64_t>(destination)};
		std::memcpy(where + 3, &addr, sizeof(addr));
		// xchg qword ptr [rsp], rax
		where[11] = std::byte {0x48};
		where[12] = std::byte {0x87};
		where[13] = std::byte {0x04};
		where[14] = std::byte {0x24};
		// ret
		where[15] = std::byte {0xC3};
#else
		// jmp rel32 (relative to the instruction after the 5-byte jump)
		where[0] = std::byte {0xE9};
		const auto from   {std::bit_cast<std::uintptr_t>(where) + JUMP_SIZE};
		const auto to     {std::bit_cast<std::uintptr_t>(destination)};
		const auto rel32  {static_cast<std::uint32_t>(to - from)};
		std::memcpy(where + 1, &rel32, sizeof(rel32));
#endif
	}


	// Destructor (restores the original bytes if still installed)
	InlineHook::~InlineHook() noexcept {
		if (mInstalled) {
			static_cast<void>(uninstall());
		}
	}

	// Install the trampoline
	auto InlineHook::install(const ::LPBYTE originalFunction, const ::LPBYTE detourFunction, const std::size_t patchSize) noexcept -> ::LPCVOID {
		// Refuse to double-install or to patch fewer bytes than a jump needs
		if (mInstalled || (patchSize < JUMP_SIZE) || (nullptr == originalFunction)) {
			return nullptr;
		}

		mTarget	   = originalFunction;
		mPatchSize = patchSize;

		auto* const target {std::bit_cast<std::byte*>(originalFunction)};

		// Trampoline = saved prologue, then a jump back to the rest of the original
		mTrampoline.assign(patchSize + JUMP_SIZE, nop::ONE);
		std::memcpy(mTrampoline.data(), target, patchSize);
		writeJump(mTrampoline.data() + patchSize, target + patchSize);

		// Make the prologue writable, patch it, then restore its protection
		::DWORD previous {0UL};
		if (FALSE == ::VirtualProtect(originalFunction, patchSize, PAGE_EXECUTE_READWRITE, &previous)) {
			mTrampoline.clear();
			return nullptr;
		}
		// Jump to the detour, then NOP-pad the leftover prologue bytes
		writeJump(target, detourFunction);
		fillWithNOPs(std::span {target + JUMP_SIZE, patchSize - JUMP_SIZE});
		static_cast<void>(::VirtualProtect(originalFunction, patchSize, previous, &previous));

		// Let the CPU drop any stale copies of the rewritten bytes
		static_cast<void>(::FlushInstructionCache(::GetCurrentProcess(), originalFunction, patchSize));

		mInstalled = true;
		// Trampoline is executable: it holds original code plus a jump back
		return mTrampoline.data();
	}

	// Uninstall the trampoline and restore the original bytes
	auto InlineHook::uninstall() noexcept -> bool {
		if (!mInstalled) {
			return false;
		}

		// Make the prologue writable, restore the saved bytes, restore protection
		::DWORD previous {0UL};
		if (FALSE != ::VirtualProtect(mTarget, mPatchSize, PAGE_EXECUTE_READWRITE, &previous)) {
			std::memcpy(mTarget, mTrampoline.data(), mPatchSize);
			static_cast<void>(::VirtualProtect(mTarget, mPatchSize, previous, &previous));
			static_cast<void>(::FlushInstructionCache(::GetCurrentProcess(), mTarget, mPatchSize));
		}

		mTrampoline.clear();
		mPatchSize = 0U;
		mTarget	   = nullptr;
		mInstalled = false;

		return true;
	}


} // namespace sst
