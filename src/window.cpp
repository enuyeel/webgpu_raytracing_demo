#include "window.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static LRESULT CALLBACK
windowEventHandler
(
  HWND hwnd, /*Window handler*/
  UINT msg, /*Msg type*/
  WPARAM wParam, /*Addtional datas*/
  LPARAM lParam)
{
  switch (msg)
  {
    case WM_CLOSE:
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProc(hwnd, msg, wParam, lParam);
}

window::Handle window::create()
{
  WNDCLASS wndClass;
  wndClass.style = CS_OWNDC;
  wndClass.lpfnWndProc = windowEventHandler;
  wndClass.cbClsExtra = 0;
  wndClass.cbWndExtra = 0;
  wndClass.hInstance = GetModuleHandle(0);
  wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
  wndClass.hbrBackground = NULL;
  wndClass.lpszMenuName = NULL;
  wndClass.lpszClassName = TEXT("My Window Class");

  if (!RegisterClass(&wndClass))
    return 0;

  HWND hwnd = CreateWindow(
    wndClass.lpszClassName,
    TEXT("WebGPU Raytracing Demo"),
    //WS_OVERLAPPEDWINDOW, 
    //WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX,
    WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    512, 512,
    0, 0, wndClass.hInstance, 0);

  if (!hwnd)
    return 0;

  UnregisterClass(wndClass.lpszClassName, wndClass.hInstance);

  return reinterpret_cast<window::Handle>(hwnd);
}

void window::show(Handle hwnd)
{
  ShowWindow(reinterpret_cast<HWND>(hwnd), SW_SHOWDEFAULT);
}

void window::destroyWindow(Handle hwnd)
{
  DestroyWindow(reinterpret_cast<HWND>(hwnd));
}

void window::loop(Redraw func)
{
  bool running = true;
  MSG msg;

  while (running)
  {
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {

      if (msg.message == WM_QUIT) {
        running = false;
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);

    }

    //TODO:
    Sleep(2);

    if (func)
      func();
  }
}