//
//
//
#include "stdafx.h"
#include "KrycekiumUI.h"
#include <d2d1helper.h>
#include <Prsht.h>
#include <CommCtrl.h>
#include <Shlwapi.h>
#include <Shellapi.h>
#include <PathCch.h>
#include <Mmsystem.h>
#include <vector>
#include <thread>
#include "Resource.h"

//#include <exception>
#pragma comment(lib,"Comctl32.lib")
#pragma comment(lib,"ComDlg32.Lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib,"dwrite.lib")
#pragma comment(lib,"shcore.lib")
#pragma comment(lib,"Winmm.lib")

#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

#define WS_NORESIZEWINDOW (WS_OVERLAPPED     | \
                             WS_CAPTION        | \
                             WS_SYSMENU        | \
                             WS_MINIMIZEBOX )

template<class Interface>
inline void
SafeRelease(
Interface **ppInterfaceToRelease
)
{
	if (*ppInterfaceToRelease != NULL) {
		(*ppInterfaceToRelease)->Release();

		(*ppInterfaceToRelease) = NULL;
	}
}

int RectHeight(RECT Rect)
{
	return Rect.bottom - Rect.top;
}

int RectWidth(RECT Rect)
{
	return Rect.right - Rect.left;
}

class CDPI {
public:
	CDPI()
	{
		m_nScaleFactor = 0;
		m_nScaleFactorSDA = 0;
		m_Awareness = PROCESS_DPI_UNAWARE;
	}

	int  Scale(int x)
	{
		// DPI Unaware:  Return the input value with no scaling.
		// These apps are always virtualized to 96 DPI and scaled by the system for the DPI of the monitor where shown.
		if (m_Awareness == PROCESS_DPI_UNAWARE) {
			return x;
		}

		// System DPI Aware:  Return the input value scaled by the factor determined by the system DPI when the app was launched.
		// These apps render themselves according to the DPI of the display where they are launched, and they expect that scaling
		// to remain constant for all displays on the system.
		// These apps are scaled up or down when moved to a display with a different DPI from the system DPI.
		if (m_Awareness == PROCESS_SYSTEM_DPI_AWARE) {
			return MulDiv(x, m_nScaleFactorSDA, 100);
		}

		// Per-Monitor DPI Aware:  Return the input value scaled by the factor for the display which contains most of the window.
		// These apps render themselves for any DPI, and re-render when the DPI changes (as indicated by the WM_DPICHANGED window message).
		return MulDiv(x, m_nScaleFactor, 100);
	}

	UINT GetScale()
	{
		if (m_Awareness == PROCESS_DPI_UNAWARE) {
			return 100;
		}

		if (m_Awareness == PROCESS_SYSTEM_DPI_AWARE) {
			return m_nScaleFactorSDA;
		}

		return m_nScaleFactor;
	}

	void SetScale(__in UINT iDPI)
	{
		m_nScaleFactor = MulDiv(iDPI, 100, 96);
		if (m_nScaleFactorSDA == 0) {
			m_nScaleFactorSDA = m_nScaleFactor;  // Save the first scale factor, which is all that SDA apps know about
		}
		return;
	}

	PROCESS_DPI_AWARENESS GetAwareness()
	{
		HANDLE hProcess;
		hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, GetCurrentProcessId());
		GetProcessDpiAwareness(hProcess, &m_Awareness);
		return m_Awareness;
	}

	void SetAwareness(PROCESS_DPI_AWARENESS awareness)
	{
		HRESULT hr = E_FAIL;
		hr = SetProcessDpiAwareness(awareness);
		auto l = E_INVALIDARG;
		if (hr == S_OK) {
			m_Awareness = awareness;
		} else {
			MessageBox(NULL, (LPCWSTR)L"SetProcessDpiAwareness Error", (LPCWSTR)L"Error", MB_OK);
		}
		return;
	}

	// Scale rectangle from raw pixels to relative pixels.
	void ScaleRect(__inout RECT *pRect)
	{
		pRect->left = Scale(pRect->left);
		pRect->right = Scale(pRect->right);
		pRect->top = Scale(pRect->top);
		pRect->bottom = Scale(pRect->bottom);
	}

	// Scale Point from raw pixels to relative pixels.
	void ScalePoint(__inout POINT *pPoint)
	{
		pPoint->x = Scale(pPoint->x);
		pPoint->y = Scale(pPoint->y);
	}

private:
	UINT m_nScaleFactor;
	UINT m_nScaleFactorSDA;
	PROCESS_DPI_AWARENESS m_Awareness;
};

/*
* Resources Initialize and Release
*/

D2D1_ELLIPSE g_Ellipse0 = D2D1::Ellipse(D2D1::Point2F(300, 250), 50, 50);
D2D1_ELLIPSE g_Ellipse1 = D2D1::Ellipse(D2D1::Point2F(300, 250), 80, 80);

D2D1_ELLIPSE g_Ellipse[GEOMETRY_COUNT] =
{
	g_Ellipse0,
	g_Ellipse1,
};

MetroWindow::MetroWindow()
	:m_pFactory(nullptr),
	m_pHwndRenderTarget(nullptr),
	m_pSolidColorBrush(nullptr),
	m_PushButtonNActiveBrush(nullptr),
	m_PushButtonActiveBrush(nullptr),
	m_PushButtonClickBrush(nullptr),
	m_pBakcgroundEdgeBrush(nullptr),
	m_pWriteFactory(nullptr),
	m_pWriteTextFormat(nullptr)
{
	g_Dpi = new CDPI();
	g_Dpi->SetAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
	MetroLabel ml = { { 20, 50, 120, 75 }, L"Package:" };
	MetroButton find({ 540, 50, 640, 75 }, L"View...",
					 std::bind(&MetroWindow::KrycekiumDiscoverPackageButtonActive, this, std::placeholders::_1));
	MetroLabel ml2 = { { 20, 100, 120, 125 }, L"Folder:" };
	MetroButton folderfind({ 540, 100, 640, 125 }, L"Select",
						   std::bind(&MetroWindow::KrycekiumDiscoverFolderButtonActive, this, std::placeholders::_1));
	MetroButton taskbutton({ 540, 300, 640, 325 }, L"Run",
						   std::bind(&MetroWindow::KrycekiumTaskRun, this, std::placeholders::_1));
	MetroLabel info = { { 80, 345, 480, 370 }, L"\x263B \x2665 Copyright \x0A9 2016.Force Charlie.All Rights Reserved." };
	label_.push_back(std::move(ml));
	label_.push_back(std::move(info));
	label_.push_back(std::move(ml2));
	button_.push_back(std::move(find));
	button_.push_back(std::move(folderfind));
	button_.push_back(std::move(taskbutton));
}
MetroWindow::~MetroWindow()
{
	if (g_Dpi) {
		delete g_Dpi;
	}
	SafeRelease(&m_pWriteTextFormat);
	SafeRelease(&m_pWriteFactory);
	SafeRelease(&m_pSolidColorBrush);

	SafeRelease(&m_PushButtonNActiveBrush);
	SafeRelease(&m_PushButtonActiveBrush);
	SafeRelease(&m_PushButtonClickBrush);

	SafeRelease(&m_pBakcgroundEdgeBrush);

	SafeRelease(&m_pGeometryGroup);
	SafeRelease(&m_pRadialGradientBrush);

	for (int i = 0; i < GEOMETRY_COUNT; ++i) {
		SafeRelease(&m_pEllipseArray[i]);
		m_pEllipseArray[i] = NULL;
	}

	SafeRelease(&m_pHwndRenderTarget);
	SafeRelease(&m_pFactory);
}

LRESULT MetroWindow::InitializeWindow()
{
	HMONITOR hMonitor;
	POINT    pt;
	UINT     dpix = 0, dpiy = 0;
	HRESULT  hr = E_FAIL;

	// Get the DPI for the main monitor, and set the scaling factor
	pt.x = 1;
	pt.y = 1;
	hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
	hr = GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpix, &dpiy);

	if (hr != S_OK) {
		::MessageBox(NULL, (LPCWSTR)L"GetDpiForMonitor failed", (LPCWSTR)L"Notification", MB_OK);
		return FALSE;
	}
	g_Dpi->SetScale(dpix);
	RECT layout = { g_Dpi->Scale(100), g_Dpi->Scale(100), g_Dpi->Scale(800), g_Dpi->Scale(540) };
	Create(nullptr, layout, L"Krycekium Msi Unpacker",
		   WS_NORESIZEWINDOW,
		   WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
	return S_OK;
}


///
HRESULT MetroWindow::CreateDeviceIndependentResources()
{
	HRESULT hr = S_OK;
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pFactory);
	return hr;
}
HRESULT MetroWindow::Initialize()
{
	auto hr = CreateDeviceIndependentResources();
	FLOAT dpiX, dpiY;
	m_pFactory->GetDesktopDpi(&dpiX, &dpiY);
	return hr;
}
HRESULT MetroWindow::CreateDeviceResources()
{
	HRESULT hr = S_OK;

	if (!m_pHwndRenderTarget) {
		RECT rc;
		::GetClientRect(m_hWnd, &rc);
		D2D1_SIZE_U size = D2D1::SizeU(
			rc.right - rc.left,
			rc.bottom - rc.top
			);
		hr = m_pFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(m_hWnd, size),
			&m_pHwndRenderTarget
			);
		if (SUCCEEDED(hr)) {
			hr = m_pHwndRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::Blue),
				&m_pBakcgroundEdgeBrush
				);
		}
		D2D1_GRADIENT_STOP gradientStops[2];
		gradientStops[0].color = D2D1::ColorF(D2D1::ColorF::Cyan);
		gradientStops[0].position = 0.5f;
		gradientStops[1].color = D2D1::ColorF(D2D1::ColorF::RoyalBlue);
		gradientStops[1].position = 1.f;

		// Create gradient stops collection
		ID2D1GradientStopCollection* pGradientStops = NULL;
		hr = m_pHwndRenderTarget->CreateGradientStopCollection(
			gradientStops,
			2,
			D2D1_GAMMA_2_2,
			D2D1_EXTEND_MODE_CLAMP,
			&pGradientStops
			);
		hr = m_pHwndRenderTarget->CreateRadialGradientBrush(
			D2D1::RadialGradientBrushProperties(
			D2D1::Point2F(170, 170),
			D2D1::Point2F(0, 0),
			150,
			150),
			pGradientStops,
			&m_pRadialGradientBrush
			);
		if (SUCCEEDED(hr)) {
			hr = m_pHwndRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::Black),
				&m_pSolidColorBrush
				);
		}
		if (SUCCEEDED(hr)) {
			hr = m_pHwndRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::Orange),
				&m_PushButtonNActiveBrush
				);
		}
		if (SUCCEEDED(hr)) {
			hr = m_pHwndRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(0xFFC300),
				&m_PushButtonActiveBrush
				);
		}
		if (SUCCEEDED(hr)) {
			hr = m_pHwndRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::DarkOrange, 2.0f),
				&m_PushButtonClickBrush
				);
		}
		for (int i = 0; i < GEOMETRY_COUNT; ++i) {
			hr = m_pFactory->CreateEllipseGeometry(g_Ellipse[i], &m_pEllipseArray[i]);
			if (FAILED(hr)) {
				::MessageBoxW(m_hWnd, L"Create Ellipse Geometry failed!", L"Error", MB_OK|MB_ICONERROR);
				return S_FALSE;
			}
		}
		hr = m_pFactory->CreateGeometryGroup(
			D2D1_FILL_MODE_ALTERNATE,
			(ID2D1Geometry**)&m_pEllipseArray,
			ARRAYSIZE(m_pEllipseArray),
			&m_pGeometryGroup
			);
	}
	return hr;

}
void MetroWindow::DiscardDeviceResources()
{
	SafeRelease(&m_pSolidColorBrush);
}
HRESULT MetroWindow::OnRender()
{
	auto hr = CreateDeviceResources();
	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
							 __uuidof(IDWriteFactory),
							 reinterpret_cast<IUnknown**>(&m_pWriteFactory));
	if (hr != S_OK) return hr;
	hr = m_pWriteFactory->CreateTextFormat(
		L"Segoe UI",
		NULL,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		12.0f * 96.0f / 72.0f,
		L"zh-CN",
		&m_pWriteTextFormat);
#pragma warning(disable:4244)
#pragma warning(disable:4267)
	if (SUCCEEDED(hr)) {
		m_pHwndRenderTarget->BeginDraw();
		m_pHwndRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
		m_pHwndRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f));
		//m_pWriteTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
		for (auto &label : label_) {
			if (label.text.empty())
				continue;
			m_pHwndRenderTarget->DrawTextW(label.text.c_str(),
										   label.text.size(),
										   m_pWriteTextFormat,
										   D2D1::RectF(label.layout.left, label.layout.top, label.layout.right, label.layout.bottom),
										   m_pSolidColorBrush,
										   D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL);
		}
		for (auto &it : item_) {
			if (it.name.empty() || it.value.empty())
				continue;
			m_pHwndRenderTarget->DrawTextW(it.name.c_str(),
										   it.name.size(),
										   m_pWriteTextFormat,
										   D2D1::RectF(it.layout.left, it.layout.top, it.layout.left + 135.0f, it.layout.bottom),
										   m_pSolidColorBrush,
										   D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL);
			m_pHwndRenderTarget->DrawTextW(it.value.c_str(),
										   it.value.size(),
										   m_pWriteTextFormat,
										   D2D1::RectF(it.layout.left + 140.0f, it.layout.top, it.layout.right, it.layout.bottom),
										   m_pSolidColorBrush,
										   D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL);
		}
		m_pWriteTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
		for (auto &b : button_) {
			m_pHwndRenderTarget->DrawRectangle(
				D2D1::RectF(b.layout.left, b.layout.top, b.layout.right, b.layout.bottom),
				m_pBakcgroundEdgeBrush,
				0.3f, 0);
			switch (b.status) {
				case MetroButton::kKeyDown:
				m_pHwndRenderTarget->FillRectangle(
					D2D1::RectF(b.layout.left, b.layout.top, b.layout.right, b.layout.bottom),
					m_PushButtonClickBrush);
				break;
				case MetroButton::kKeyActive:
				m_pHwndRenderTarget->FillRectangle(
					D2D1::RectF(b.layout.left, b.layout.top, b.layout.right, b.layout.bottom),
					m_PushButtonActiveBrush);
				break;
				case MetroButton::kKeyLeave:
				default:
				m_pHwndRenderTarget->FillRectangle(
					D2D1::RectF(b.layout.left, b.layout.top, b.layout.right, b.layout.bottom),
					m_PushButtonNActiveBrush);
				break;
			}
			if (!b.caption.empty()) {
				m_pHwndRenderTarget->DrawTextW(b.caption.c_str(),
											   b.caption.size(),
											   m_pWriteTextFormat,
											   D2D1::RectF(b.layout.left, b.layout.top, b.layout.right, b.layout.bottom),
											   m_pSolidColorBrush,
											   D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL);
			}
		}
		if (taskrunning_) {
			static float totalAngle = 0.0f;
			static DWORD lastTime = timeGetTime();

			// Get current time
			DWORD currentTime = timeGetTime();

			// Calculate time elapsed in current frame.
			float timeDelta = (float)(currentTime - lastTime) * 0.1;

			// Increase the totalAngle by the time elapsed in current frame.
			totalAngle += timeDelta;
			m_pHwndRenderTarget->DrawGeometry(m_pGeometryGroup, m_PushButtonClickBrush);

			// Roatate the gradient brush based on the total elapsed time
			D2D1_MATRIX_3X2_F rotMatrix = D2D1::Matrix3x2F::Rotation(totalAngle, D2D1::Point2F(300, 300));
			m_pRadialGradientBrush->SetTransform(rotMatrix);

			// Fill geometry group with the transformed brush
			m_pHwndRenderTarget->FillGeometry(m_pGeometryGroup, m_pRadialGradientBrush);

		}
		hr = m_pHwndRenderTarget->EndDraw();
	}
#pragma warning(default:4244)
#pragma warning(default:4267)	
	if (hr == D2DERR_RECREATE_TARGET) {
		hr = S_OK;
		DiscardDeviceResources();
		::InvalidateRect(m_hWnd, nullptr, FALSE);
	}
	return hr;
}
D2D1_SIZE_U MetroWindow::CalculateD2DWindowSize()
{
	RECT rc;
	::GetClientRect(m_hWnd, &rc);

	D2D1_SIZE_U d2dWindowSize = { 0 };
	d2dWindowSize.width = rc.right;
	d2dWindowSize.height = rc.bottom;

	return d2dWindowSize;
}

void MetroWindow::OnResize(
	UINT width,
	UINT height
	)
{
	if (m_pHwndRenderTarget) {
		m_pHwndRenderTarget->Resize(D2D1::SizeU(width, height));
	}
}

/*
*  Message Action Function
*/
LRESULT MetroWindow::OnCreate(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle)
{
	auto hr = Initialize();
	if (hr != S_OK) {
		::MessageBoxW(nullptr, L"Initialize() failed", L"Fatal error", MB_OK | MB_ICONSTOP);
		std::terminate();
		return S_FALSE;
	}
	HICON hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_KRYCEKIUM));
	SetIcon(hIcon, TRUE);
	ChangeWindowMessageFilter(WM_DROPFILES, MSGFLT_ADD);
	ChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_ADD);
	ChangeWindowMessageFilter(0x0049, MSGFLT_ADD);
	::DragAcceptFiles(m_hWnd, TRUE);
	DWORD dwEditEx = WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | \
		WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE;
	DWORD dwEdit = WS_CHILDWINDOW | WS_CLIPSIBLINGS | WS_VISIBLE | \
		WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL;
	HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	LOGFONT logFont = { 0 };
	GetObject(hFont, sizeof(logFont), &logFont);
	DeleteObject(hFont);
	hFont = NULL;
	logFont.lfHeight = 19;
	logFont.lfWeight = FW_NORMAL;
	wcscpy_s(logFont.lfFaceName, L"Segoe UI");
	hFont = CreateFontIndirect(&logFont);

	HWND hEdit = CreateWindowExW(dwEditEx, WC_EDITW,
								 L"",
								 dwEdit, 90, 50, 435, 27,
								 m_hWnd,
								 HMENU(IDC_PACKAGE_URI_EDIT),
								 HINST_THISCOMPONENT,
								 NULL);
	HWND hEdit2 = CreateWindowExW(dwEditEx, WC_EDITW,
								 L"",
								 dwEdit, 90, 100, 435, 27,
								 m_hWnd,
								 HMENU(IDC_FOLDER_URI_EDIT),
								 HINST_THISCOMPONENT,
								 NULL);
	//::SetWindowFont(hEdit, hFont, TRUE);
	::SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
	::SendMessage(hEdit2, WM_SETFONT, (WPARAM)hFont, TRUE);
	return S_OK;
}
LRESULT MetroWindow::OnDestroy(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle)
{
	PostQuitMessage(0);
	return S_OK;
}
LRESULT MetroWindow::OnClose(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle)
{
	::DestroyWindow(m_hWnd);
	return S_OK;
}
LRESULT MetroWindow::OnSize(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle)
{
	UINT width = LOWORD(lParam);
	UINT height = HIWORD(lParam);
	OnResize(width, height);
	return S_OK;
}
LRESULT MetroWindow::OnPaint(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle)
{
	LRESULT hr = S_OK;
	PAINTSTRUCT ps;
	BeginPaint(&ps);
	/// if auto return OnRender(),CPU usage is too high
	hr = OnRender();
	EndPaint(&ps);
	return hr;
}

LRESULT MetroWindow::OnDropfiles(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL & bHandled)
{
	const LPCWSTR PackageSubffix[] = { L".msi", L".msp"};
	HDROP hDrop = (HDROP)wParam;
	UINT nfilecounts = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
	WCHAR dropfile_name[MAX_PATH];
	std::vector<std::wstring> filelist;
	for (UINT i = 0; i < nfilecounts; i++) {
		DragQueryFileW(hDrop, i, dropfile_name, MAX_PATH);
		if (PathFindSuffixArrayW(dropfile_name, PackageSubffix, ARRAYSIZE(PackageSubffix))) {
			filelist.push_back(dropfile_name);
		}
		if (!filelist.empty()) {
			::SetWindowTextW(::GetDlgItem(m_hWnd, IDC_PACKAGE_URI_EDIT), filelist[0].c_str());
			//KcycekiumFolderSet(filelist[0]);
		}
	}
	DragFinish(hDrop);
	::InvalidateRect(m_hWnd, NULL, TRUE);
	return S_OK;
}

LRESULT MetroWindow::OnLButtonUP(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle)
{
	POINT pt;
	GetCursorPos(&pt);
	ScreenToClient(&pt);
	for (auto &b : button_) {
		if (pt.x >= b.layout.left
			&& pt.x <= b.layout.right
			&&pt.y >= b.layout.top
			&&pt.y <= b.layout.bottom
			) {
			b.callback(L"this is debug message");
			b.status = MetroButton::kKeyActive;
			break;
		}
		b.status = MetroButton::kKeyLeave;
	}
	::InvalidateRect(m_hWnd, nullptr, TRUE);
	return 0;
}
LRESULT MetroWindow::OnLButtonDown(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle)
{
	POINT pt;
	GetCursorPos(&pt);
	ScreenToClient(&pt);
	for (auto &b : button_) {
		if (pt.x >= b.layout.left
			&& pt.x <= b.layout.right
			&&pt.y >= b.layout.top
			&&pt.y <= b.layout.bottom
			) {
			b.status = MetroButton::kKeyDown;
			break;
		}
		b.status = MetroButton::kKeyLeave;
	}
	::InvalidateRect(m_hWnd, nullptr, FALSE);
	return 0;
}

LRESULT MetroWindow::KrycekiumDiscoverPackageButtonActive(const wchar_t *debugMessage)
{
	std::wstring packagefile;
	if (KrycekiumDiscoverWindow(m_hWnd, packagefile, L"Get Microsoft Installer Package")) {
		::SetWindowTextW(GetDlgItem(IDC_PACKAGE_URI_EDIT), packagefile.c_str());
		//KcycekiumFolderSet(packagefile);
	}
	return S_OK;
}

LRESULT MetroWindow::KrycekiumDiscoverFolderButtonActive(const wchar_t *debugMessage)
{
	std::wstring folder;
	if (KrycekiumFolderOpenWindow(m_hWnd, folder, L"")) {
		::SetWindowTextW(::GetDlgItem(m_hWnd, IDC_FOLDER_URI_EDIT), folder.c_str());
	}
	return S_OK;
}

LRESULT MetroWindow::KrycekiumTaskRun(const wchar_t *debugMessage)
{
	if (taskrunning_)
		return S_FALSE;
	//MsgWaitForMultipleObjects
	WCHAR packfile[4096];
	WCHAR folder[4096];
	auto len=::GetWindowTextW(::GetDlgItem(m_hWnd, IDC_PACKAGE_URI_EDIT), packfile, 4096);
	if (len == 0)
		return S_FALSE;
	auto len2=::GetWindowTextW(::GetDlgItem(m_hWnd, IDC_FOLDER_URI_EDIT), folder, 4096);
	if (len2 == 0)
		return S_FALSE;
	argument_.packfile.assign(packfile, len);
	argument_.folder.assign(folder, len);
	std::thread([&]{
		std::vector<wchar_t> Array(PATHCCH_MAX_CCH);
		auto cmdline = Array.data();
		const wchar_t *format = LR"(msiexec /a "%s" /qn TARGETDIR="%s")";
		swprintf_s(cmdline, Array.capacity(), format, packfile, folder);
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		SecureZeroMemory(&si, sizeof(si));
		SecureZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);
		taskrunning_ = true;
		PostMessageW(WM_PAINT, 0, 0);
		auto result = CreateProcessW(nullptr, cmdline, nullptr, nullptr, FALSE,
									 CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
									 nullptr, nullptr, &si, &pi);
		WCHAR error[4096];
		if (!result) {
			taskrunning_ = false;
			LPVOID lpMessageBuf;
			FormatMessageW(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				nullptr,
				GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPWSTR)&lpMessageBuf, 0, nullptr);
			//FormatMessageW()
			swprintf_s(error, L"ERROR: %s", lpMessageBuf);
			LocalFree(lpMessageBuf);
			MessageBoxW(error, L"Invoke msiexec broken", MB_OK | MB_ICONERROR);
		} else {
			WaitForSingleObject(pi.hProcess, INFINITE);
			DWORD dwExit;
			GetExitCodeProcess(pi.hProcess, &dwExit);
			if (dwExit != 0) {
				swprintf_s(error, L"msiexec exit with %d", dwExit);
				MessageBoxW(error, L"Execute msiexec broken", MB_OK | MB_ICONERROR);
			} else {
				MessageBoxW(L"Unpack file success !", L"Unpack success", MB_OK|MB_ICONINFORMATION);
			}
			this->UnsetTaskIsRunning();
			::InvalidateRect(this->m_hWnd, nullptr, FALSE);
		}
	}).detach();
	return S_OK;
}


void MetroWindow::KcycekiumFolderSet(const std::wstring &package)
{
	std::wstring folder = package;
	PathRemoveFileSpecW(&folder[0]);
	::SetWindowTextW(GetDlgItem(IDC_FOLDER_URI_EDIT), &folder[0]);
	//PathRemoveFileSpecW()
}
