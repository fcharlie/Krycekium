#include "stdafx.h"
#include <ShlObj.h>
#include <Uxtheme.h>
#include <atlbase.h>
#include <atlcoll.h>
#include <atlsimpstr.h>
#include <atlstr.h>
#include <atlwin.h>
#include <string>
#include <strsafe.h>
#include <tchar.h>
#pragma warning(disable : 4091)
#include <Shlwapi.h>

using namespace ATL;

typedef COMDLG_FILTERSPEC FilterSpec;

const FilterSpec filterSpec[] = {
    {L"Windows Installer Package (*.msi;*.msp)", L"*.msi;*.msp"},
    {L"All Files (*.*)", L"*.*"}};

void ReportErrorMessage(LPCWSTR pszFunction, HRESULT hr) {
  wchar_t szBuffer[65535] = {0};
  if (SUCCEEDED(StringCchPrintf(
          szBuffer, ARRAYSIZE(szBuffer),
          L"Call: %s Failed w/hr 0x%08lx ,Please Checker Error Code!",
          pszFunction, hr))) {
    int nB = 0;
    TaskDialog(nullptr, GetModuleHandle(nullptr), L"Failed information",
               L"Call Function Failed:", szBuffer, TDCBF_OK_BUTTON,
               TD_ERROR_ICON, &nB);
  }
}
bool KrycekiumDiscoverWindow(HWND hParent, std::wstring &filename,
                             const wchar_t *pszWindowTitle) {
  HRESULT hr = S_OK;
  IFileOpenDialog *pWindow = nullptr;
  IShellItem *pItem = nullptr;
  PWSTR pwszFilePath = nullptr;
  if (CoCreateInstance(__uuidof(FileOpenDialog), NULL, CLSCTX_INPROC_SERVER,
                       IID_PPV_ARGS(&pWindow)) != S_OK) {
    ReportErrorMessage(L"FileOpenWindowProvider", hr);
    return false;
  }
  hr = pWindow->SetFileTypes(sizeof(filterSpec) / sizeof(filterSpec[0]),
                             filterSpec);
  hr = pWindow->SetFileTypeIndex(1);
  hr = pWindow->SetTitle(pszWindowTitle ? pszWindowTitle
                                        : L"Open File Provider");
  hr = pWindow->Show(hParent);
  if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
    // User cancelled.
    hr = S_OK;
    goto done;
  }
  if (FAILED(hr)) {
    goto done;
  }
  hr = pWindow->GetResult(&pItem);
  if (FAILED(hr))
    goto done;
  hr = pItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &pwszFilePath);
  if (FAILED(hr)) {
    goto done;
  }
  filename.assign(pwszFilePath);
done:
  if (pwszFilePath) {
    CoTaskMemFree(pwszFilePath);
  }
  if (pItem) {
    pItem->Release();
  }
  if (pWindow) {
    pWindow->Release();
  }
  return hr == S_OK;
}

bool KrycekiumFolderOpenWindow(HWND hParent, std::wstring &folder,
                               const wchar_t *pszWindowTitle) {
  IFileDialog *pfd = nullptr;
  HRESULT hr = S_OK;
  bool bRet = false;
  if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL,
                                 CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
    DWORD dwOptions;
    if (!SUCCEEDED(pfd->GetOptions(&dwOptions))) {
      return false;
    }
    pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
    pfd->SetTitle(pszWindowTitle ? pszWindowTitle : L"Open Folder");
    if (SUCCEEDED(pfd->Show(hParent))) {
      IShellItem *psi;
      if (SUCCEEDED(pfd->GetResult(&psi))) {
        PWSTR pwsz = NULL;
        hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pwsz);
        if (SUCCEEDED(hr)) {
          folder = pwsz;
          bRet = true;
          CoTaskMemFree(pwsz);
        }
        psi->Release();
      }
    }
    pfd->Release();
  }
  return bRet;
}