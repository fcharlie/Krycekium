//
//
//
#include "stdafx.h"
#include <CommCtrl.h>
#include <Mmsystem.h>
#include <Msi.h>
#include <PathCch.h>
#include <Prsht.h>
#include <Shellapi.h>
#include <Shlwapi.h>
#include <d2d1helper.h>
#include <future>
#include <thread>
#include <vector>
#include "KrycekiumUI.h"
#include "MessageWindow.h"
#include "Resource.h"

//#include <exception>
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "ComDlg32.Lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Msi.lib")

#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

#define WS_NORESIZEWINDOW                                                      \
  (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN | WS_MINIMIZEBOX)

template <class Interface>
inline void SafeRelease(Interface **ppInterfaceToRelease) {
  if (*ppInterfaceToRelease != NULL) {
    (*ppInterfaceToRelease)->Release();

    (*ppInterfaceToRelease) = NULL;
  }
}

int RectHeight(RECT Rect) { return Rect.bottom - Rect.top; }

int RectWidth(RECT Rect) { return Rect.right - Rect.left; }

/*
 * Resources Initialize and Release
 */
class CDPI {
public:
  CDPI() {
    m_nScaleFactor = 0;
    m_nScaleFactorSDA = 0;
    m_Awareness = PROCESS_DPI_UNAWARE;
  }

  int Scale(int x) {
    // DPI Unaware:  Return the input value with no scaling.
    // These apps are always virtualized to 96 DPI and scaled by the system for
    // the DPI of the monitor where shown.
    if (m_Awareness == PROCESS_DPI_UNAWARE) {
      return x;
    }

    // System DPI Aware:  Return the input value scaled by the factor determined
    // by the system DPI when the app was launched. These apps render themselves
    // according to the DPI of the display where they are launched, and they
    // expect that scaling to remain constant for all displays on the system.
    // These apps are scaled up or down when moved to a display with a different
    // DPI from the system DPI.
    if (m_Awareness == PROCESS_SYSTEM_DPI_AWARE) {
      return MulDiv(x, m_nScaleFactorSDA, 100);
    }

    // Per-Monitor DPI Aware:  Return the input value scaled by the factor for
    // the display which contains most of the window. These apps render
    // themselves for any DPI, and re-render when the DPI changes (as indicated
    // by the WM_DPICHANGED window message).
    return MulDiv(x, m_nScaleFactor, 100);
  }

  UINT GetScale() {
    if (m_Awareness == PROCESS_DPI_UNAWARE) {
      return 100;
    }

    if (m_Awareness == PROCESS_SYSTEM_DPI_AWARE) {
      return m_nScaleFactorSDA;
    }

    return m_nScaleFactor;
  }

  void SetScale(__in UINT iDPI) {
    m_nScaleFactor = MulDiv(iDPI, 100, 96);
    if (m_nScaleFactorSDA == 0) {
      m_nScaleFactorSDA = m_nScaleFactor; // Save the first scale factor, which
                                          // is all that SDA apps know about
    }
    return;
  }

  PROCESS_DPI_AWARENESS GetAwareness() {
    HANDLE hProcess;
    hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, GetCurrentProcessId());
    GetProcessDpiAwareness(hProcess, &m_Awareness);
    return m_Awareness;
  }

  void SetAwareness(PROCESS_DPI_AWARENESS awareness) {
    HRESULT hr = E_FAIL;
    hr = SetProcessDpiAwareness(awareness);
    auto l = E_INVALIDARG;
    if (hr == S_OK) {
      m_Awareness = awareness;
    } else {
      MessageBoxW(NULL, L"SetProcessDpiAwareness Error", L"Error", MB_OK);
    }
    return;
  }

  // Scale rectangle from raw pixels to relative pixels.
  void ScaleRect(__inout RECT *pRect) {
    pRect->left = Scale(pRect->left);
    pRect->right = Scale(pRect->right);
    pRect->top = Scale(pRect->top);
    pRect->bottom = Scale(pRect->bottom);
  }

  // Scale Point from raw pixels to relative pixels.
  void ScalePoint(__inout POINT *pPoint) {
    pPoint->x = Scale(pPoint->x);
    pPoint->y = Scale(pPoint->y);
  }

private:
  UINT m_nScaleFactor;
  UINT m_nScaleFactorSDA;
  PROCESS_DPI_AWARENESS m_Awareness;
};

MetroWindow::MetroWindow()
    : m_pFactory(nullptr), m_pHwndRenderTarget(nullptr),
      m_pSolidColorBrush(nullptr), m_AreaBorderBrush(nullptr),
      m_pWriteFactory(nullptr), m_pWriteTextFormat(nullptr) {
  KryceLabel ml = {{30, 50, 120, 75}, L"Package\t\xD83D\xDCE6:"};
  KryceLabel ml2 = {{30, 100, 120, 125}, L"Folder\t\xD83D\xDCC1:"};
  label_.push_back(std::move(ml));
  label_.push_back(std::move(ml2));
  info = {
      {125, 345, 600, 370},
      L"\xD83D\xDE0B \x2764 Copyright \x0A9 2018, Force Charlie. All Rights "
      L"Reserved."};
  dpi_ = std::make_unique<CDPI>();
  dpi_->SetAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
}
MetroWindow::~MetroWindow() {
  SafeRelease(&m_pWriteTextFormat);
  SafeRelease(&m_pWriteFactory);
  SafeRelease(&m_pSolidColorBrush);

  SafeRelease(&m_AreaBorderBrush);
  SafeRelease(&m_pHwndRenderTarget);
  SafeRelease(&m_pFactory);
  if (hFont != nullptr) {
    DeleteFont(hFont);
  }
}

LRESULT MetroWindow::InitializeWindow() {
  HMONITOR hMonitor;
  POINT pt;
  UINT dpix = 0, dpiy = 0;
  HRESULT hr = E_FAIL;

  // Get the DPI for the main monitor, and set the scaling factor
  pt.x = 1;
  pt.y = 1;
  hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  hr = GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpix, &dpiy);

  if (hr != S_OK) {
    ::MessageBox(NULL, (LPCWSTR)L"GetDpiForMonitor failed",
                 (LPCWSTR)L"Notification", MB_OK);
    return FALSE;
  }
  dpi_->SetScale(dpix);
  RECT layout = {dpi_->Scale(100), dpi_->Scale(100), dpi_->Scale(800),
                 dpi_->Scale(540)};
  windowTitle.assign(L"Krycekium Installer");
  Create(nullptr, layout, windowTitle.c_str(), WS_NORESIZEWINDOW,
         WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
  return S_OK;
}

///
HRESULT MetroWindow::CreateDeviceIndependentResources() {
  HRESULT hr = S_OK;
  hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pFactory);
  if (SUCCEEDED(hr)) {
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                             __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown **>(&m_pWriteFactory));
    if (SUCCEEDED(hr)) {
      hr = m_pWriteFactory->CreateTextFormat(
          L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL,
          DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
          12.0f * 96.0f / 72.0f, L"zh-CN", &m_pWriteTextFormat);
    }
  }
  return hr;
}
HRESULT MetroWindow::Initialize() {
  auto hr = CreateDeviceIndependentResources();
  return hr;
}
HRESULT MetroWindow::CreateDeviceResources() {
  HRESULT hr = S_OK;

  if (!m_pHwndRenderTarget) {
    RECT rc;
    ::GetClientRect(m_hWnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    hr = m_pFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_hWnd, size), &m_pHwndRenderTarget);
    if (SUCCEEDED(hr)) {
      hr = m_pHwndRenderTarget->CreateSolidColorBrush(
          D2D1::ColorF(D2D1::ColorF::Black), &m_pSolidColorBrush);
    }
    if (SUCCEEDED(hr)) {
      hr = m_pHwndRenderTarget->CreateSolidColorBrush(
          D2D1::ColorF(D2D1::ColorF::Orange), &m_AreaBorderBrush);
    }
  }
  return hr;
}
void MetroWindow::DiscardDeviceResources() {
  SafeRelease(&m_pSolidColorBrush);
  SafeRelease(&m_AreaBorderBrush);
}
HRESULT MetroWindow::OnRender() {
  auto hr = CreateDeviceResources();
  if (SUCCEEDED(hr)) {
#pragma warning(disable : 4244)
#pragma warning(disable : 4267)
    if (SUCCEEDED(hr)) {
      auto size = m_pHwndRenderTarget->GetSize();
      m_pHwndRenderTarget->BeginDraw();
      m_pHwndRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
      m_pHwndRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f));
      m_pHwndRenderTarget->DrawRectangle(
          D2D1::RectF(20, 10, size.width - 20, 155), m_AreaBorderBrush, 1.0);
      m_pHwndRenderTarget->DrawRectangle(
          D2D1::RectF(20, 155, size.width - 20, size.height - 20),
          m_AreaBorderBrush, 1.0);
      // D2D1_DRAW_TEXT_OPTIONS_NONE
      for (auto &label : label_) {
        if (label.text.empty())
          continue;
        m_pHwndRenderTarget->DrawTextW(
            label.text.c_str(), label.text.size(), m_pWriteTextFormat,
            D2D1::RectF(label.layout.left, label.layout.top, label.layout.right,
                        label.layout.bottom),
            m_pSolidColorBrush, D2D1_DRAW_TEXT_OPTIONS_NONE,
            DWRITE_MEASURING_MODE_NATURAL);
      }
      if (info.text.size()) {
        m_pHwndRenderTarget->DrawTextW(
            info.text.c_str(), info.text.size(), m_pWriteTextFormat,
            D2D1::RectF(info.layout.left, info.layout.top, info.layout.right,
                        info.layout.bottom),
            m_pSolidColorBrush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
            DWRITE_MEASURING_MODE_NATURAL);
      }
      hr = m_pHwndRenderTarget->EndDraw();
    }
    if (hr == D2DERR_RECREATE_TARGET) {
      hr = S_OK;
      DiscardDeviceResources();
      //::InvalidateRect(m_hWnd, nullptr, FALSE);
    }
#pragma warning(default : 4244)
#pragma warning(default : 4267)
  }
  return hr;
}
D2D1_SIZE_U MetroWindow::CalculateD2DWindowSize() {
  RECT rc;
  ::GetClientRect(m_hWnd, &rc);

  D2D1_SIZE_U d2dWindowSize = {0};
  d2dWindowSize.width = rc.right;
  d2dWindowSize.height = rc.bottom;

  return d2dWindowSize;
}

void MetroWindow::OnResize(UINT width, UINT height) {
  if (m_pHwndRenderTarget) {
    m_pHwndRenderTarget->Resize(D2D1::SizeU(width, height));
  }
}

/*
 *  Message Action Function
 */
#define WINDOWEXSTYLE                                                          \
  WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_NOPARENTNOTIFY
#define EDITBOXSTYLE                                                           \
  WS_CHILDWINDOW | WS_CLIPSIBLINGS | WS_VISIBLE | WS_TABSTOP | ES_LEFT |       \
      ES_AUTOHSCROLL
#define PUSHBUTTONSTYLE                                                        \
  BS_PUSHBUTTON | BS_TEXT | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE
#define PROGRESSRATESTYLE WS_CHILDWINDOW | WS_CLIPSIBLINGS | WS_VISIBLE
LRESULT MetroWindow::OnCreate(UINT nMsg, WPARAM wParam, LPARAM lParam,
                              BOOL &bHandle) {
  auto hr = Initialize();
  if (hr != S_OK) {
    ::MessageBoxW(nullptr, L"Initialize() failed", L"Fatal error",
                  MB_OK | MB_ICONSTOP);
    std::terminate();
    return S_FALSE;
  }
  HICON hIcon =
      LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_KRYCEKIUM));
  SetIcon(hIcon, TRUE);
  ChangeWindowMessageFilter(WM_DROPFILES, MSGFLT_ADD);
  ChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_ADD);
  ChangeWindowMessageFilter(0x0049, MSGFLT_ADD);
  ::DragAcceptFiles(m_hWnd, TRUE);

  hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
  LOGFONT logFont = {0};
  GetObjectW(hFont, sizeof(logFont), &logFont);
  DeleteObject(hFont);
  hFont = nullptr;

  logFont.lfHeight = dpi_->Scale(20);
  logFont.lfWeight = FW_NORMAL;
  wcscpy_s(logFont.lfFaceName, L"Segoe UI");
  hFont = CreateFontIndirectW(&logFont);
  auto LambdaCreateWindow = [&](LPCWSTR lpClassName, LPCWSTR lpWindowName,
                                DWORD dwStyle, int X, int Y, int nWidth,
                                int nHeight, HMENU hMenu) -> HWND {
    auto hw = CreateWindowExW(WINDOWEXSTYLE, lpClassName, lpWindowName, dwStyle,
                              dpi_->Scale(X), dpi_->Scale(Y),
                              dpi_->Scale(nWidth), dpi_->Scale(nHeight), m_hWnd,
                              hMenu, HINST_THISCOMPONENT, nullptr);
    if (hw) {
      ::SendMessageW(hw, WM_SETFONT, (WPARAM)hFont, lParam);
    }
    return hw;
  };
  auto LambdaCreateWindowEdge = [&](LPCWSTR lpClassName, LPCWSTR lpWindowName,
                                    DWORD dwStyle, int X, int Y, int nWidth,
                                    int nHeight, HMENU hMenu) -> HWND {
    auto hw = CreateWindowExW(
        WINDOWEXSTYLE | WS_EX_CLIENTEDGE, lpClassName, lpWindowName, dwStyle,
        dpi_->Scale(X), dpi_->Scale(Y), dpi_->Scale(nWidth),
        dpi_->Scale(nHeight), m_hWnd, hMenu, HINST_THISCOMPONENT, nullptr);
    if (hw) {
      ::SendMessageW(hw, WM_SETFONT, (WPARAM)hFont, lParam);
    }
    return hw;
  };
  HMENU hSystemMenu = ::GetSystemMenu(m_hWnd, FALSE);
  InsertMenuW(hSystemMenu, SC_CLOSE, MF_ENABLED, IDM_KRYCEKIUM_ABOUT,
              L"About Krycekium\tAlt+F1");
  hUriEdit = LambdaCreateWindowEdge(WC_EDITW, L"", EDITBOXSTYLE, 125, 50, 420,
                                    27, HMENU(IDC_PACKAGE_URI_EDIT));
  hDirEdit = LambdaCreateWindowEdge(WC_EDITW, L"", EDITBOXSTYLE, 125, 100, 420,
                                    27, HMENU(IDC_FOLDER_URI_EDIT));
  hUriButton = LambdaCreateWindow(WC_BUTTONW, L"View...", PUSHBUTTONSTYLE, 560,
                                  50, 90, 27, HMENU(IDC_PACKAGE_VIEW_BUTTON));
  hDirButton =
      LambdaCreateWindow(WC_BUTTONW, L"Folder...", PUSHBUTTONSTYLE, 560, 100,
                         90, 27, HMENU(IDC_FOLDER_URI_BUTTON));
  hProgress = LambdaCreateWindow(PROGRESS_CLASSW, L"", PROGRESSRATESTYLE, 125,
                                 180, 420, 27, HMENU(IDC_PROCESS_RATE));
  hOK = LambdaCreateWindow(WC_BUTTONW, L"Start", PUSHBUTTONSTYLE, 125, 270, 205,
                           30, HMENU(IDC_OPTION_BUTTON_OK));
  hCancel = LambdaCreateWindow(WC_BUTTONW, L"Cancel", PUSHBUTTONSTYLE, 340, 270,
                               205, 30, HMENU(IDC_OPTION_BUTTON_CANCEL));
  {
    int numArgc = 0;
    auto Argv = CommandLineToArgvW(GetCommandLineW(), &numArgc);
    if (Argv) {
      if (numArgc >= 2 && PathFileExistsW(Argv[1])) {
        ::SetWindowTextW(hUriEdit, Argv[1]);
      }
      LocalFree(Argv);
    }
  }
  return S_OK;
}
LRESULT MetroWindow::OnDestroy(UINT nMsg, WPARAM wParam, LPARAM lParam,
                               BOOL &bHandle) {
  PostQuitMessage(0);
  return S_OK;
}
LRESULT MetroWindow::OnClose(UINT nMsg, WPARAM wParam, LPARAM lParam,
                             BOOL &bHandle) {
  ::DestroyWindow(m_hWnd);
  return S_OK;
}
LRESULT MetroWindow::OnSize(UINT nMsg, WPARAM wParam, LPARAM lParam,
                            BOOL &bHandle) {
  UINT width = LOWORD(lParam);
  UINT height = HIWORD(lParam);
  OnResize(width, height);
  return S_OK;
}
LRESULT MetroWindow::OnPaint(UINT nMsg, WPARAM wParam, LPARAM lParam,
                             BOOL &bHandle) {
  OnRender();
  ValidateRect(NULL);
  return S_OK;
  // LRESULT hr = S_OK;
  // PAINTSTRUCT ps;
  // BeginPaint(&ps);
  ///// if auto return OnRender(),CPU usage is too high
  // hr = OnRender();
  // EndPaint(&ps);
  ////ValidateRect(NULL);
  // return hr;
}

LRESULT MetroWindow::OnDpiChanged(UINT nMsg, WPARAM wParam, LPARAM lParam,
                                  BOOL &bHandle) {
  HMONITOR hMonitor;
  POINT pt;
  UINT dpix = 0, dpiy = 0;
  HRESULT hr = E_FAIL;

  // Get the DPI for the main monitor, and set the scaling factor
  pt.x = 1;
  pt.y = 1;
  hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  hr = GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpix, &dpiy);

  if (hr != S_OK) {
    ::MessageBox(NULL, (LPCWSTR)L"GetDpiForMonitor failed",
                 (LPCWSTR)L"Notification", MB_OK);
    return FALSE;
  }
  dpi_->SetScale(dpix);
  LOGFONTW logFont = {0};
  GetObjectW(hFont, sizeof(logFont), &logFont);
  DeleteObject(hFont);
  hFont = nullptr;
  logFont.lfHeight = dpi_->Scale(19);
  logFont.lfWeight = FW_NORMAL;
  wcscpy_s(logFont.lfFaceName, L"Segoe UI");
  hFont = CreateFontIndirectW(&logFont);
  auto UpdateWindowPos = [&](HWND hWnd) {
    RECT rect;
    ::GetClientRect(hWnd, &rect);
    ::SetWindowPos(hWnd, NULL, dpi_->Scale(rect.left), dpi_->Scale(rect.top),
                   dpi_->Scale(rect.right - rect.left),
                   dpi_->Scale(rect.bottom - rect.top),
                   SWP_NOZORDER | SWP_NOACTIVATE);
    ::SendMessageW(hWnd, WM_SETFONT, (WPARAM)hFont, lParam);
  };
  UpdateWindowPos(hUriEdit);
  UpdateWindowPos(hDirEdit);
  UpdateWindowPos(hUriButton);
  UpdateWindowPos(hDirButton);
  UpdateWindowPos(hProgress);
  UpdateWindowPos(hOK);
  UpdateWindowPos(hCancel);
  return S_OK;
}

LRESULT MetroWindow::OnDisplayChange(UINT nMsg, WPARAM wParam, LPARAM lParam,
                                     BOOL &bHandled) {
  ::InvalidateRect(m_hWnd, NULL, FALSE);
  return S_OK;
}

LRESULT MetroWindow::OnDropfiles(UINT nMsg, WPARAM wParam, LPARAM lParam,
                                 BOOL &bHandled) {
  const LPCWSTR PackageSubffix[] = {L".msi", L".msp"};
  HDROP hDrop = (HDROP)wParam;
  UINT nfilecounts = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
  WCHAR dropfile_name[MAX_PATH];
  std::vector<std::wstring> filelist;
  for (UINT i = 0; i < nfilecounts; i++) {
    DragQueryFileW(hDrop, i, dropfile_name, MAX_PATH);
    if (PathFindSuffixArrayW(dropfile_name, PackageSubffix,
                             ARRAYSIZE(PackageSubffix))) {
      filelist.push_back(dropfile_name);
    }
    if (!filelist.empty()) {
      ::SetWindowTextW(::GetDlgItem(m_hWnd, IDC_PACKAGE_URI_EDIT),
                       filelist[0].c_str());
    }
  }
  DragFinish(hDrop);
  return S_OK;
}

LRESULT MetroWindow::OnKrycekiumAbout(WORD wNotifyCode, WORD wID, HWND hWndCtl,
                                      BOOL &bHandled) {
  MessageWindowEx(m_hWnd, L"About Krycekium Installer",
                  L"Prerelease: 1.0.0.0\nCopyright \xA9 2018, Force Charlie. "
                  L"All Rights Reserved.",
                  L"For more information about this tool.\nVisit: <a "
                  L"href=\"http://forcemz.net/\">forcemz.net</a>",
                  kAboutWindow);
  return S_OK;
}

LRESULT MetroWindow::OnDiscoverPackage(WORD wNotifyCode, WORD wID, HWND hWndCtl,
                                       BOOL &bHandled) {
  std::wstring packagefile;
  if (KrycekiumDiscoverWindow(m_hWnd, packagefile,
                              L"Get Microsoft Installer Package")) {
    ::SetWindowTextW(GetDlgItem(IDC_PACKAGE_URI_EDIT), packagefile.c_str());
  }
  return S_OK;
}

LRESULT MetroWindow::OnDiscoverFolder(WORD wNotifyCode, WORD wID, HWND hWndCtl,
                                      BOOL &bHandled) {
  std::wstring folder;
  if (KrycekiumFolderOpenWindow(m_hWnd, folder, L"")) {
    ::SetWindowTextW(::GetDlgItem(m_hWnd, IDC_FOLDER_URI_EDIT), folder.c_str());
  }
  return S_OK;
}

INT CALLBACK InstallUICallback(LPVOID pvContext, UINT iMessageType,
                               LPCWSTR szMessage) {
  return reinterpret_cast<MetroWindow *>(pvContext)->InstanllUIHandler(
      iMessageType, szMessage);
}

inline bool FolderIsEmpty(const std::wstring &dir) {
  std::wstring dir_(dir);
  dir_.append(L"\\*.*");
  WIN32_FIND_DATAW fdata;
  auto hFind = FindFirstFileW(dir_.c_str(), &fdata);
  if (hFind == INVALID_HANDLE_VALUE)
    return true;
  BOOL ret = true;
  while (ret) {
    if (wcscmp(fdata.cFileName, L".") != 0 &&
        wcscmp(fdata.cFileName, L"..") != 0) {
      FindClose(hFind);
      return false;
    }
    ret = FindNextFileW(hFind, &fdata);
  }
  FindClose(hFind);
  return true;
}

LRESULT MetroWindow::OnStartTask(WORD wNotifyCode, WORD wID, HWND hWndCtl,
                                 BOOL &bHandled) {
  if (!mtx.try_lock()) {
    MessageBeep(MB_ICONHAND);
    return S_FALSE;
  } else {
    mtx.unlock();
  }
  WCHAR packfile[4096];
  WCHAR folder[4096];
  auto len = ::GetWindowTextW(::GetDlgItem(m_hWnd, IDC_PACKAGE_URI_EDIT),
                              packfile, 4096);
  if (len == 0) {
    MessageWindowEx(m_hWnd, L"Package Path Empty", L"Please Input Package Path",
                    nullptr, kFatalWindow);
    return S_FALSE;
  }
  auto len2 =
      ::GetWindowTextW(::GetDlgItem(m_hWnd, IDC_FOLDER_URI_EDIT), folder, 4096);
  if (len2 == 0) {
    MessageWindowEx(m_hWnd, L"Folder Path Empty", L"Please Input Folder Path",
                    nullptr, kFatalWindow);
    return S_FALSE;
  }

  argument_.packfile.assign(packfile, len);
  argument_.folder.assign(folder, len);
  InstallContextReset();
  std::wstring title(windowTitle);
  title.append(L" (Running...)");
  SetWindowTextW(title.c_str());
  std::thread([&, this] {
    std::lock_guard<std::mutex> lock(this->mtx);
    auto fileName = PathFindFileNameW(argument_.packfile.c_str());
    WCHAR FileName[256];
    wcscpy_s(FileName, fileName);
    std::wstring dupfolder(folder);
    PathRemoveExtensionW(FileName);
    if (!FolderIsEmpty(dupfolder)) {
      dupfolder.append(L"\\").append(FileName);
    }
    if (!PathFileExistsW(dupfolder.c_str())) {
      if (!CreateDirectoryW(dupfolder.c_str(), nullptr)) {
        ErrorMessage error(GetLastError());
        /// Error
        MessageWindowEx(m_hWnd, L"Error", error.message(), nullptr,
                        kFatalWindow);
        return;
      }
    }
    std::vector<wchar_t> Command(PATHCCH_MAX_CCH);
    swprintf_s(Command.data(), Command.capacity(),
               L"ACTION=ADMIN TARGETDIR=\"%s\"", dupfolder.c_str());
    MsiSetInternalUI(
        INSTALLUILEVEL(INSTALLUILEVEL_NONE | INSTALLUILEVEL_SOURCERESONLY),
        NULL);

    MsiSetExternalUIW(
        InstallUICallback,
        INSTALLLOGMODE_PROGRESS | INSTALLLOGMODE_FATALEXIT |
            INSTALLLOGMODE_ERROR | INSTALLLOGMODE_WARNING |
            INSTALLLOGMODE_USER | INSTALLLOGMODE_INFO |
            INSTALLLOGMODE_RESOLVESOURCE | INSTALLLOGMODE_OUTOFDISKSPACE |
            INSTALLLOGMODE_ACTIONSTART | INSTALLLOGMODE_ACTIONDATA |
            INSTALLLOGMODE_COMMONDATA | INSTALLLOGMODE_PROGRESS |
            INSTALLLOGMODE_INITIALIZE | INSTALLLOGMODE_TERMINATE |
            INSTALLLOGMODE_SHOWDIALOG,
        this);
    auto err = MsiInstallProductW(argument_.packfile.c_str(), Command.data());
    std::wstring title2(windowTitle);
    if (err == ERROR_SUCCESS) {
      title2.append(L" (Completed)");
      this->SetWindowTextW(title2.c_str());
    } else {
      title2.append(L" (Failure)");
      this->SetWindowTextW(title2.c_str());
      ErrorMessage err(GetLastError());
      MessageWindowEx(m_hWnd, L"Failure", err.message(), nullptr, kFatalWindow);
    }
  })
      .detach();
  return S_OK;
}

LRESULT MetroWindow::OnCancelTask(WORD wNotifyCode, WORD wID, HWND hWndCtl,
                                  BOOL &bHandled) {
  requireCancel = true;
  return S_OK;
}

int FGetInteger(WCHAR *&rpch) {
  WCHAR *pchPrev = rpch;
  while (*rpch && *rpch != L' ')
    rpch++;
  *rpch = L'\0';
  int i = _wtoi(pchPrev);
  return i;
}

BOOL ParseProgressString(LPWSTR sz, int *pnFields) {
  WCHAR *pch = sz;
  if (0 == *pch)
    return FALSE; // no msg

  while (*pch != 0) {
    WCHAR chField = *pch++;
    pch++; // for ':'
    pch++; // for sp
    switch (chField) {
    case L'1': // field 1
    {
      // progress message type
      if (0 == isdigit(*pch))
        return FALSE; // blank record
      pnFields[0] = *pch++ - L'0';
      break;
    }
    case L'2': // field 2
    {
      pnFields[1] = FGetInteger(pch);
      if (pnFields[0] == 2 || pnFields[0] == 3)
        return TRUE; // done processing
      break;
    }
    case '3': // field 3
    {
      pnFields[2] = FGetInteger(pch);
      if (pnFields[0] == 1)
        return TRUE; // done processing
      break;
    }
    case '4': // field 4
    {
      pnFields[3] = FGetInteger(pch);
      return TRUE; // done processing
    }
    default: // unknown field
    {
      return FALSE;
    }
    }
    pch++; // for space (' ') between fields
  }

  return TRUE;
}

int MetroWindow::InstanllUIHandler(UINT iMessageType, LPCWSTR szMessage) {
  if (mFirstTime) {
    UINT r1 = MsiSetInternalUI(INSTALLUILEVEL_BASIC, NULL);
    mFirstTime = false;
  }
  if (requireCancel)
    return IDCANCEL;
  if (!szMessage)
    return 0;
  INSTALLMESSAGE mt;
  UINT uiFlags;

  mt = (INSTALLMESSAGE)(0xFF000000 & (UINT)iMessageType);
  uiFlags = 0x00FFFFFF & iMessageType;
  switch (mt) {
  case INSTALLMESSAGE_FATALEXIT:
    MessageWindowEx(m_hWnd, L"Install Failed !", szMessage, nullptr,
                    kFatalWindow);
    return 1;
  case INSTALLMESSAGE_ERROR: {
    /* Get error message here and display it*/
    // language and caption can be obtained from common data msg
    MessageBeep(uiFlags & MB_ICONMASK);
    return ::MessageBoxEx(m_hWnd, szMessage, TEXT("Error"), uiFlags,
                          LANG_NEUTRAL);
  }
  case INSTALLMESSAGE_WARNING:
    MessageWindowEx(m_hWnd, L"Warning", szMessage, nullptr, kWarnWindow);
    return 0;
  case INSTALLMESSAGE_USER:
    /* Get user message here */
    // parse uiFlags to get Message Box Styles Flag and return appopriate value,
    // IDOK, IDYES, etc.
    return IDOK;

  case INSTALLMESSAGE_INFO:
    return IDOK;

  case INSTALLMESSAGE_FILESINUSE:
    /* Display FilesInUse dialog */
    // parse the message text to provide the names of the
    // applications that the user can close so that the
    // files are no longer in use.
    return 0;

  case INSTALLMESSAGE_RESOLVESOURCE:
    /* ALWAYS return 0 for ResolveSource */
    return 0;

  case INSTALLMESSAGE_OUTOFDISKSPACE:
    /* Get user message here */
    return IDOK;

  case INSTALLMESSAGE_ACTIONSTART:
    /* New action started, any action data is sent by this new action */
    mEnableActionData = FALSE;
    return IDOK;
    break;
  case INSTALLMESSAGE_ACTIONDATA:
    if (0 == mProgressTotal)
      return IDOK;
    if (mEnableActionData) {
      ::SendMessageW(hProgress, PBM_STEPIT, 0, 0);
    }
    return IDOK;
    break;
  case INSTALLMESSAGE_PROGRESS: {
    if (ParseProgressString(const_cast<LPWSTR>(szMessage), iField)) {
      switch (iField[0]) {
      case 0: {
        mProgressTotal = iField[1];
        if (iField[2] == 0)
          mForwardProgress = true;
        else
          mForwardProgress = false;
        mProgress = mForwardProgress ? 0 : mProgressTotal;
        ::SendMessageW(hProgress, PBM_SETRANGE32, 0, mProgressTotal);
        ::SendMessageW(hProgress, PBM_SETPOS,
                       mScriptInProgress ? mProgressTotal : mProgress, 0);
        iCurPos = 0;
        mScriptInProgress = (iField[3] == 1) ? TRUE : FALSE;
      } break;
      case 1: {
        if (iField[2]) {
          ::SendMessageW(hProgress, PBM_SETSTEP,
                         mForwardProgress ? iField[1] : -1 * iField[1], 0);
          mEnableActionData = TRUE;
        } else {
          mEnableActionData = FALSE;
        }
      } break;
      case 2: {
        if (0 == mProgressTotal)
          break;

        iCurPos += iField[1];
        ::SendMessageW(hProgress, PBM_SETPOS,
                       mForwardProgress ? iCurPos : -1 * iCurPos, 0);

      } break;
      default:
        break;
      }
    }
  }
    return IDOK;
  case INSTALLMESSAGE_INITIALIZE:
    return IDOK;

  // Sent after UI termination, no string data
  case INSTALLMESSAGE_TERMINATE:
    return IDOK;

  // Sent prior to display of authored dialog or wizard
  case INSTALLMESSAGE_SHOWDIALOG:
    return IDOK;
  default:
    break;
  }
  return 0;
}