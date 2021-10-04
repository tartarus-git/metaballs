#pragma once

#include <Windows.h>
#include "debugOutput.h"
#include <thread>

LRESULT CALLBACK windowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void setWindowSize(unsigned int windowWidth, unsigned int windowHeight);
bool isAlive = true;
void graphicsLoop(HWND hWnd);

#ifdef UNICODE
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, wchar_t* lpCmdLine, int nCmdShow) {
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* lpCmdLine, int nCmdShow) {
#endif
	debuglogger::out << "program started" << debuglogger::endl;

	debuglogger::out << "setting up window..." << debuglogger::endl;
	WNDCLASS windowClass = { };																																			// Create WNDCLASS struct for later window creation.
	windowClass.lpfnWndProc = windowProc;
	windowClass.hInstance = hInstance;
	windowClass.lpszClassName = TEXT(WINDOW_CLASS_NAME);
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);

	if (!RegisterClass(&windowClass)) {
		debuglogger::out << debuglogger::error << "failed to register window class" << debuglogger::endl;
		return EXIT_FAILURE;
	}

	HWND hWnd = CreateWindow(windowClass.lpszClassName, TEXT(WINDOW_TITLE), WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, hInstance, nullptr);
	if (hWnd == nullptr) {																																				// Check if creation of window was successful.
		debuglogger::out << debuglogger::error << "couldn't create window" << debuglogger::endl;
		return EXIT_FAILURE;
	}

	debuglogger::out << "showing window..." << debuglogger::endl;
	ShowWindow(hWnd, nCmdShow);

	RECT clientRect;
	if (!GetClientRect(hWnd, &clientRect)) {
		debuglogger::out << debuglogger::error << "failed to get client bounds" << debuglogger::endl;
		return EXIT_FAILURE;
	}
	setWindowSize(clientRect.right, clientRect.bottom);

	debuglogger::out << "starting graphics thread..." << debuglogger::endl;
	std::thread graphicsThread(graphicsLoop, hWnd);

	debuglogger::out << "running message loop..." << debuglogger::endl;
	MSG msg = { };
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	debuglogger::out << "joining with graphics thread and exiting..." << debuglogger::endl;
	isAlive = false;
	graphicsThread.join();
}
