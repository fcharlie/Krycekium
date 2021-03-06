// Krycekium.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <CommCtrl.h>
#include <commdlg.h>
#include <VersionHelpers.h>
#include <Objbase.h>
#include "Krycekium.h"
#include "KrycekiumUI.h"

class DotComInitialize {
public:
  DotComInitialize() { CoInitialize(NULL); }
  ~DotComInitialize() { CoUninitialize(); }
};

int MetroWindowRunLoop() {
  INITCOMMONCONTROLSEX info = {sizeof(INITCOMMONCONTROLSEX),
                               ICC_TREEVIEW_CLASSES | ICC_COOL_CLASSES |
                                   ICC_LISTVIEW_CLASSES};
  InitCommonControlsEx(&info);
  MetroWindow window;
  MSG msg;
  window.InitializeWindow();
  window.ShowWindow(SW_SHOW);
  window.UpdateWindow();
  while (GetMessage(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return 0;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
                    _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
  if (!IsWindows10OrGreater()) {
    MessageBoxW(nullptr, L"You need at least Windows 10",
                L"Please upgrade Your OS to Windows 10", MB_OK | MB_ICONERROR);
    return -1;
  }
  DotComInitialize dot;
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
  return MetroWindowRunLoop();
}