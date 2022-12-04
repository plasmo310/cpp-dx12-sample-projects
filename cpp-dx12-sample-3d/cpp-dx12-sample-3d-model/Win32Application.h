#pragma once
#include <windows.h>
#include <tchar.h>
#include "DXApplication.h"

class Win32Application
{
public:
	static void Run(DXApplication* dxApp, HINSTANCE hInstance);

private:
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
};
