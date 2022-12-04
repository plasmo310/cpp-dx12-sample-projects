#include "Win32Application.h"

void Win32Application::Run(DXApplication* dxApp, HINSTANCE hInstance)
{
	// ウィンドウクラス生成
	WNDCLASSEX windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = _T("DXSampleClass");
	RegisterClassEx(&windowClass);

	// ウィンドウサイズの調整
	RECT windowRect = { 0, 0, dxApp->GetWindowWidth(), dxApp->GetWindowHeight() };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(
		windowClass.lpszClassName,
		dxApp->GetTitle(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	// アプリケーション初期化
	dxApp->OnInit(hwnd);

	// ウィンドウ表示
	ShowWindow(hwnd, SW_SHOW);

	// メインループ
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// アプリケーション更新
		dxApp->OnUpdate();
		dxApp->OnRender();
	}

	// アプリケーション終了
	dxApp->OnDestroy();

	// クラスを登録解除する
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

LRESULT CALLBACK Win32Application::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hwnd, message, wparam, lparam);
	}
	return 0;
}
