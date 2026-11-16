////////////////////////////////////////////////////////////////
///
///	@file
///	@brief		Shared hook-DLL globals (screenshot output directory)
///	@date		16 Nov 2026
///
///	@copyright	Copyright (c) 2017 - 2026, Igor Baklykov
///			All rights reserved.
///	@author		Igor Baklykov
///


#pragma once


// C++
#include <filesystem>


// ScreenShotTool hook code zone
namespace sst {


	/// @brief Directory screenshots are written to (set once in DllMain)
	extern std::filesystem::path gScreenDir;


} // namespace sst
