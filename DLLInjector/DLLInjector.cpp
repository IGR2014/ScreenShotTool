#include <windows.h>
#include <TlHelp32.h>

#include "DLLInjector.hpp"


// Default constructor
DLLInjector::DLLInjector() {}

// Constructor from process name and dll name
DLLInjector::DLLInjector(const LPCWSTR procName,
						 LPCWSTR dllName) {

	// Set process
	setProcessByName(procName);
	// Set DLL
	setDLLByName(dllName);

}

// Constructor from process ID and dll name
DLLInjector::DLLInjector(const DWORD &procID,
						 LPCWSTR dllName) {

	// Set process
	setProcessByID(procID);
	// Set DLL
	setDLLByName(dllName);

}

// Set process to inject by ID
BOOL DLLInjector::setProcessByID(const DWORD &procID) {

	// Check if process ID is not 0
	if (procID == 0) {

		// Error
		return FALSE;
	
	}

	// Save process ID
	processID = procID;

	// Exit sucess
	return TRUE;

}

// Set process to inject by name
BOOL DLLInjector::setProcessByName(LPCWSTR procName) {

	// Process entry struct
	PROCESSENTRY32 processEntry = {0};

	// Create process snapshot
	HANDLE snapHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	// If unable to create process snapshot
	if (snapHandle == INVALID_HANDLE_VALUE) {
		
		// Return ID not found
		processID = 0;
		// Error
		return FALSE;
	
	}
	
	// Start looking for process
	processEntry.dwSize = sizeof(PROCESSENTRY32);
	Process32First(snapHandle, &processEntry);
	
	// Search process
	while (Process32Next(snapHandle, &processEntry)) {
		
		// If process found
		if (wcscmp(processEntry.szExeFile, procName) == 0) {
			
			// Close handle
			CloseHandle(snapHandle);
			// Set process by ID
			setProcessByID(processEntry.th32ProcessID);
			// Success
			return TRUE;
		
		}
	
	};

	// Close handle
	CloseHandle(snapHandle);

	// Error
	return FALSE;

}

// Set DLL name for injection
BOOL DLLInjector::setDLLByName(LPCWSTR dllName) {

	// Save pointer to dll name
	injectingDLLName = dllName;

	// Success
	return TRUE;

}

// Inject DLL to process
BOOL DLLInjector::inject() {

	// Process handle
	HANDLE hTargetProcess;

	// Check if process ID is not 0
	if (processID == 0) {

		// Error
		return FALSE;
	
	}

	// Get process handle
	hTargetProcess = OpenProcess(PROCESS_ALL_ACCESS |
								 PROCESS_VM_OPERATION |
								 PROCESS_VM_READ |
								 PROCESS_VM_WRITE,
								 FALSE,
								 processID);

	// Check if process exists
	if (hTargetProcess == INVALID_HANDLE_VALUE) {
	
		// Return error
		return FALSE;
	
	}

	// Allocate memory in process for DLL path
	LPVOID dllNameString = (LPVOID)VirtualAllocEx(hTargetProcess,
												  NULL,
												  wcslen(injectingDLLName) * sizeof(WCHAR),
												  MEM_RESERVE |
												  MEM_COMMIT,
												  PAGE_READWRITE);

	// Write to process memory DLL path
	BOOL write = WriteProcessMemory(hTargetProcess,
									dllNameString,
									injectingDLLName,
									wcslen(injectingDLLName) * sizeof(WCHAR),
									NULL);

	// Check if write succeed
	if (write == FALSE) {
	
		// Return error
		return FALSE;
	
	}

	// Create remote thread in target process
	HANDLE hThread = CreateRemoteThread(hTargetProcess,
										NULL,
										0,
										(LPTHREAD_START_ROUTINE)LoadLibraryW,
										dllNameString,
										0,
										NULL);
	// Check if thread created
	if (hThread == INVALID_HANDLE_VALUE) {
	
		// Return error
		return FALSE;

	}

	// Wait for thread
	WaitForSingleObject(hThread, INFINITE);

	// Get LoadLibrary exit code
	GetExitCodeThread(hThread, &dllHandle);

	// Close thread handle
	CloseHandle(hThread);

	// Free virtual memory
	VirtualFreeEx(hTargetProcess,
				  dllNameString,
				  0,
				  MEM_FREE);

	// Close process handle
	CloseHandle(hTargetProcess);

	// Return sucess
	return TRUE;

}

// Eject DLL from process
BOOL DLLInjector::eject() {

	// Process handle
	HANDLE hTargetProcess;

	// Check if process ID is not 0
	if (processID == 0) {

		// Error
		return FALSE;
	
	}

	// Get process handle
	hTargetProcess = OpenProcess(PROCESS_ALL_ACCESS |
								 PROCESS_VM_OPERATION |
								 PROCESS_VM_READ |
								 PROCESS_VM_WRITE,
								 FALSE,
								 processID);

	// Check if process exists
	if (hTargetProcess == INVALID_HANDLE_VALUE) {
	
		// Return error
		return FALSE;
	
	}

	// Create thread to free dll
	HANDLE hThread = CreateRemoteThread(hTargetProcess,
										NULL,
										0,
										(LPTHREAD_START_ROUTINE)FreeLibrary,
										(HANDLE)dllHandle,
										0,
										NULL);
	// Check if thread created
	if (hThread == INVALID_HANDLE_VALUE) {
	
		// Return error
		return FALSE;

	}

	// Wait for thread
	WaitForSingleObject(hThread, INFINITE);

	// Get FreeLibrary exit code
	BOOL bResult;
	GetExitCodeThread(hThread, (LPDWORD)&bResult);

	// Close thread handle
	CloseHandle(hThread);

	// Close process handle
	CloseHandle(hTargetProcess);

	return bResult;

}

// Destructor
DLLInjector::~DLLInjector() {

	// Cleanup
	processID = 0;

	injectingDLLName = NULL;

}
