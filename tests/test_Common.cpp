////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Tests for Common.hpp RAII helpers and the FrameCapture concept
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


// ScreenShotTool common
#include <common/Common.hpp>
// ScreenShotTool hook
#include <dxHookDLL/Capture.hpp>
#include <dxHookDLL/Direct3D9Capture.hpp>
#include <dxHookDLL/Direct3D10Capture.hpp>
#include <dxHookDLL/Direct3D11Capture.hpp>
#include <dxHookDLL/OpenGLCapture.hpp>
// Tests
#include "TestFramework.hpp"


// The three capture classes must satisfy the FrameCapture concept that the
// shared thread driver is constrained on (compile-time contract).
static_assert(sst::FrameCapture<sst::Direct3D9Capture>);
static_assert(sst::FrameCapture<sst::Direct3D10Capture>);
static_assert(sst::FrameCapture<sst::Direct3D11Capture>);
static_assert(sst::FrameCapture<sst::OpenGLCapture>);


auto wmain() -> int {

	// scope_exit runs its action exactly once at scope end
	{
		int runs {0};
		{
			const sst::scope_exit guard {[&runs]() noexcept {
				++runs;
			}};
			CHECK(0 == runs); // not yet fired
		}
		CHECK(1 == runs); // fired on scope exit
	}

	// dismiss() cancels the action
	{
		int runs {0};
		{
			sst::scope_exit guard {[&runs]() noexcept {
				++runs;
			}};
			guard.dismiss();
		}
		CHECK(0 == runs);
	}

	// unique_handle: empty by default, release() hands back the raw value
	{
		sst::scoped_handle empty {};
		CHECK(!empty.valid());
		CHECK(!static_cast<bool>(empty));

		auto* const raw {reinterpret_cast<::HANDLE>(static_cast<std::uintptr_t>(0x1234U))};
		sst::scoped_handle held {raw};
		CHECK(held.valid());
		CHECK(held.get() == raw);
		CHECK(held.release() == raw); // ownership handed off (no CloseHandle)
		CHECK(!held.valid());	      // now empty
	}

	// unique_handle: move transfers ownership and empties the source
	{
		auto* const raw {reinterpret_cast<::HANDLE>(static_cast<std::uintptr_t>(0x5678U))};
		sst::scoped_handle source {raw};
		sst::scoped_handle sink {std::move(source)};
		CHECK(sink.get() == raw);
		CHECK(!source.valid());
		CHECK(sink.release() == raw); // avoid CloseHandle on a bogus value
	}

	return sst::test::report("Common.RAII+FrameCapture");
}
