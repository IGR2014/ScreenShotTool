#pragma once

//#include <windows.h>


class DLLInjector {

	private:

		LPCWSTR			injectingDLLName;		// Name of DLL to inject
		DWORD			processID;				// Process ID
		DWORD			dllHandle;				// Injected DLL handle


	public:

		// Default constructor
		DLLInjector();
		// Constructor from process name and dll name
		DLLInjector(const LPCWSTR, LPCWSTR);
		// Constructor from process ID and dll name
		DLLInjector(const DWORD&, LPCWSTR);

		// Set process to inject by ID
		BOOL			setProcessByID(const DWORD&);
		// Set process to inject by name
		BOOL			setProcessByName(LPCWSTR);

		// Set DLL name for injection
		BOOL			setDLLByName(LPCWSTR);

		// Inject DLL to process
		BOOL			inject();
		// Eject DLL from process
		BOOL			eject();

		// Destructor
		~DLLInjector();

};
