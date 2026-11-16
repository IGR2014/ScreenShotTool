////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Definitions of the shared hook-DLL globals
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


// C++
#include <filesystem>
// ScreenShotTool hook
#include "Globals.hpp"
#include "Log.hpp"


// ScreenShotTool hook code zone
namespace sst {


	// Process-wide hook logger (declared in Log.hpp)
	Logger gLog {};
	// Screenshot output directory (declared in Globals.hpp)
	std::filesystem::path gScreenDir {};


} // namespace sst
