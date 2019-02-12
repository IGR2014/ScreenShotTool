#pragma once


// 1 byte NOP instruction
static const BYTE NOP1		= 0x90;
// 2 bytes 66 NOP instruction
static const BYTE NOP2[2]	= {0x66, 0x90};
// 3 bytes 66 66 NOP instruction
static const BYTE NOP3[3]	= {0x66, 0x66, 0x90};
// 4 bytes NOP DWORD ptr [EAX + 0x00] instruction
static const BYTE NOP4[4]	= {0x0F, 0x1F, 0x40, 0x00};
// 5 bytes NOP DWORD ptr [EAX + EAX * 1 + 0x00] instruction
static const BYTE NOP5[5]	= {0x0F, 0x1F, 0x44, 0x00, 0x00};
// 6 bytes 66 NOP DWORD ptr [EAX + EAX * 1 + 0x00]
static const BYTE NOP6[6]	= {0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00};
// 7 bytes NOP DWORD ptr [EAX + 0x00000000] instruction
static const BYTE NOP7[7]	= {0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00};
// 8 bytes NOP DWORD ptr [EAX + EAX * 1 + 0x00000000] instruction
static const BYTE NOP8[8]	= {0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00};
// 9 bytes 66 NOP DWORD ptr [EAX + EAX * 1 + 0x00000000] instruction
static const BYTE NOP9[9]	= {0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00};
// 10 bytes 66 66 NOP DWORD ptr [EAX + EAX * 1 + 0x00000000] instruction
static const BYTE NOP10[10]	= {0x66, 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00};


// Fill memory at address with NOPs
VOID fillWithNOPs(LPVOID, DWORD);


// Hook (create trampoline) function
LPCVOID HookFunction(LPBYTE, LPBYTE, const DWORD&);


// Function trampolining functionality class
class funcHook {

	private:

		LPBYTE		targetFunction;			// Original hooked function
		LPBYTE		jumpFragment;			// Jump fragment
		DWORD		savedFragmentSize;		// Size of jump fragment
		BOOL		trampolined;			// Function trampoline installed flag


	public:

		// C-tor
		funcHook();

		// Install function trampoline
		LPCVOID		install(LPBYTE, LPBYTE, const DWORD&);
		// Uninstall function trampoline
		BOOL		uninstall();

		// D-tor
		~funcHook();

};
