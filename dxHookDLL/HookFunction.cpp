#include <windows.h>
#include <cstdio>

#include "HookFunction.hpp"


// Fill memory at address with NOPs
VOID fillWithNOPs(LPVOID address, DWORD size) {

	// Cast address pointer to LPBYTE
	LPBYTE start = static_cast<LPBYTE>(address);

	// Fill with 10-byte NOPs while possible
	while (size >= 10) {
	
		memcpy(start, NOP10, 10);
		start += 10;
		size -= 10;
	
	}

	// Fill remaining space with 1..9-byte NOP
	switch (size) {
	
		case 9:
			// 9-byte NOP
			memcpy(start, NOP9, 9);
		break;
		case 8:
			// 8-byte NOP
			memcpy(start, NOP8, 8);
		break;
		case 7:
			// 7-byte NOP
			memcpy(start, NOP7, 7);
		break;
		case 6:
			// 6-byte NOP
			memcpy(start, NOP6, 6);
		break;
		case 5:
			// 5-byte NOP
			memcpy(start, NOP5, 5);
		break;
		case 4:
			// 4-byte NOP
			memcpy(start, NOP4, 4);
		break;
		case 3:
			// 3-byte NOP
			memcpy(start, NOP3, 3);
		break;
		case 2:
			// 2-byte NOP
			memcpy(start, NOP2, 2);
		break;
		case 1:
			// 1-byte NOP
			*start = NOP1;
		break;
		default:
			// No remaining bytes to fill with NOPs (when size % 10 = 0)
		break;
	
	}

}


// C-tor
funcHook::funcHook() : targetFunction(NULL),
					   jumpFragment(NULL),
					   savedFragmentSize(0),
					   trampolined(FALSE) {}

// Install function trampoline
LPCVOID funcHook::install(LPBYTE originalFunction,
						  LPBYTE trampolineFunction,
						  const DWORD &trampolineSize) {

	// If trampoline was not previously installed
	if (trampolined == FALSE) {

		// Save original/trampolined function pointer
		targetFunction = originalFunction;
		// Save trampolined fragment size
		savedFragmentSize = trampolineSize;

		// Jump instruction size
		DWORD jumpSize;

#ifdef _WIN64

		// Jump size in x64
		jumpSize = 16;

#else

		// Jump size in x86
		jumpSize = 5;

#endif

		// Allocate memory for jump fragment
		jumpFragment = new BYTE[trampolineSize + jumpSize];

		// Set policy to read/write function memory
		DWORD virtualProtectBackup;
		VirtualProtect(originalFunction,
					   trampolineSize,
					   PAGE_READWRITE,
					   &virtualProtectBackup);

		// Copy function fragment to save it
		memcpy(jumpFragment, originalFunction, trampolineSize);
		LPBYTE jumpPosition = jumpFragment + trampolineSize;

#ifdef _WIN64

		// 0x00:	PUSH	RAX
		jumpPosition[0] = 0x50;
		// 0x01:	MOVABS	RAX, x64_address
		jumpPosition[1] = 0x48;
		jumpPosition[2] = 0xB8;
		*(DWORD64*)(jumpPosition + 3) = (DWORD64)originalFunction + trampolineSize;
		// 0x0B:	XCHG	QWORD PTR [RSP], RAX
		jumpPosition[11] = 0x48;
		jumpPosition[12] = 0x87;
		jumpPosition[13] = 0x04;
		jumpPosition[14] = 0x24;
		// 0x0F:	RET
		jumpPosition[15] = 0xC3;

		// 0x00:	PUSH	RAX
		originalFunction[0] = 0x50;
		// 0x01:	MOVABS	RAX, x64_address
		originalFunction[1] = 0x48;
		originalFunction[2] = 0xB8;
		*(DWORD64*)(originalFunction + 3) = (DWORD64)trampolineFunction;
		// 0x0B:	XCHG	QWORD PTR [RSP], RAX
		originalFunction[11] = 0x48;
		originalFunction[12] = 0x87;
		originalFunction[13] = 0x04;
		originalFunction[14] = 0x24;
		// 0x0F:	RET
		originalFunction[15] = 0xC3;

#else

		// 0x00:	JMP	x86_address_offset
		jumpPosition[0] = 0xE9;
		*(DWORD*)(jumpPosition + 1) = (DWORD)(originalFunction + trampolineSize - jumpPosition) - jumpSize;

		// 0x00:	JMP	x86_address_offset
		originalFunction[0] = 0xE9;
		*(DWORD*)(originalFunction + 1) = (DWORD)(trampolineFunction - originalFunction) - jumpSize;

#endif

		// Fill some function space with NOPs
		fillWithNOPs(&originalFunction[jumpSize], trampolineSize - jumpSize);

		// Return previous policy for function memory
		VirtualProtect(originalFunction,
					   trampolineSize,
					   virtualProtectBackup,
					   &virtualProtectBackup);

		// Set falg
		trampolined = TRUE;

		// Return function address
		return jumpPosition - trampolineSize;
	
	}

	// Return invalid address
	return NULL;

}

// Uninstall function trampoline
BOOL funcHook::uninstall() {

	// If function still trampolined
	if (trampolined == TRUE) {

		// Set policy to read/write function memory
		DWORD virtualProtectBackup;
		VirtualProtect(targetFunction,
					   savedFragmentSize,
					   PAGE_READWRITE,
					   &virtualProtectBackup);

		// Restore function space
		memcpy(targetFunction, jumpFragment, savedFragmentSize);

		// Return previous policy for function memory
		VirtualProtect(targetFunction,
					   savedFragmentSize,
					   virtualProtectBackup,
					   &virtualProtectBackup);

		// Clean up
		delete [] jumpFragment;
		savedFragmentSize = 0;
		targetFunction = NULL;

		// Reset flag
		trampolined = FALSE;

		// Trampoline uninstalled
		return TRUE;
	
	}

	// Trampoline was not installed
	return FALSE;

}

// D-tor
funcHook::~funcHook() {

	// If function is still trampolined
	if (trampolined = TRUE) {

		// Uninstall trampoline
		uninstall();
	
	}

}
