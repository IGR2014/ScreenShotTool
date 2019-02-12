#include <Windows.h>

#include "DLLInjector.hpp"


int wmain(int argc, WCHAR* argv[]) {

	WCHAR dllPath[MAX_PATH] = {};
	WCHAR targetID[MAX_PATH] = {};
	DWORD targetIDValue = 0;

	for (int i = 1; i < argc; ++i) {

		if (argv[i][0] == L'-') {

			switch(argv[i][1]) {
		
				case L't':
					MessageBox(NULL, L"TEST", L"TEST", MB_OK);
				break;
				case L'd':
					++i;
					wcscpy_s(dllPath, MAX_PATH, argv[i]);
				break;
				case L'f':
					++i;
					wcscpy_s(targetID, MAX_PATH, argv[i]);
					targetIDValue = _wtoi(targetID);
				break;
				default:
				break;
		
			}

		}
	
	}

	if (wcslen(dllPath) == 0 ||
		targetIDValue == 0) {
	
		return 0;
	
	}

	DLLInjector injector;

	if (!injector.setProcessByID(targetIDValue)) {
							
		MessageBox(NULL, L"Процесс не существует!", L"Ошибка! :(", MB_OK);
		return 0;

	}
	if (!injector.setDLLByName(dllPath)) {

		MessageBox(NULL, L"DLL не найдена!", L"Ошибка! :(", MB_OK);
		return 0;
							
	}
	if (!injector.inject()) {
							
		MessageBox(NULL, L"Ошибка внедрения!", L"Ошибка! :(", MB_OK);
							
	}

	return 0;

}
