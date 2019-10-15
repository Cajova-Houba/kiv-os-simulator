#pragma once

#include "..\api\api.h"

#include <Windows.h>

/*
	Konverze native Win handler na KIV/OS handler.
 */
kiv_os::THandle Convert_Native_Handle(const HANDLE hnd);

/*
	Konverze KIV/OS handle na native Win handler.
 */
HANDLE Resolve_kiv_os_Handle(const kiv_os::THandle hnd);

/*
	Odstrani KIV/OS handle z interního storage.
 */
bool Remove_Handle(const kiv_os::THandle hnd);
