//
//
//
//

#ifndef KRYCEKIUM_UI_H
#define KRYCEKIUM_UI_H

#include <atlbase.h>
#include <atlwin.h>
#include <atlctl.h>
#include <string>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <vector>
#include <functional>

#define IDC_PACKAGE_URI_EDIT 1010
#define IDC_PACKAGE_VIEW_BUTTON 1011
#define IDC_FOLDER_URI_EDIT 1012

bool KrycekiumDiscoverWindow(
	HWND hParent,
	std::wstring &filename,
	const wchar_t *pszWindowTitle);

bool KrycekiumFolderOpenWindow(
	HWND hParent,
	std::wstring &folder,
	const wchar_t *pszWindowTitle);

#define KRYCEKIUM_UI_MAINWINDOW _T("Krycekium.Render.UI.Window")
typedef CWinTraits<WS_OVERLAPPEDWINDOW, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE> CMetroWindowTraits;

struct MetroTitle {
	RECT layout;
	std::wstring title;
};

struct MetroLabel {
	RECT layout;
	std::wstring text;
};

struct MetroTextItem {
	RECT layout;
	std::wstring name;
	std::wstring value;
};

struct MetroTextArea {
	RECT layout;
	D2D1::ColorF backend;
	std::wstring text;
};

struct MetroButton {
	enum {
		kKeyLeave = 0,
		kKeyActive=1,
		kKeyDown,
		kButtonDisable
	};
	MetroButton(RECT layout_, const wchar_t *caption, std::function<LRESULT(const wchar_t *)> c)
		:layout(layout_), 
		caption(caption),
		callback(c)
	{
		status = kKeyLeave;
	}
	std::function<LRESULT(const wchar_t *)> callback;
	int status;
	RECT layout;
	std::wstring caption;
};
class CDPI;
#define GEOMETRY_COUNT 2
class MetroWindow :public CWindowImpl<MetroWindow, CWindow, CMetroWindowTraits> {
public:
	struct Argument {
		std::wstring packfile;
		std::wstring folder;
	};
private:
	CDPI *g_Dpi;
	ID2D1Factory *m_pFactory;
	ID2D1HwndRenderTarget* m_pHwndRenderTarget;
	ID2D1SolidColorBrush* m_pSolidColorBrush;
	//// Button Solid Color Brush
	ID2D1SolidColorBrush* m_PushButtonNActiveBrush;
	ID2D1SolidColorBrush* m_PushButtonActiveBrush;
	ID2D1SolidColorBrush* m_PushButtonClickBrush;

	ID2D1SolidColorBrush *m_pBakcgroundEdgeBrush;

	ID2D1RadialGradientBrush *m_pRadialGradientBrush; ///// Radial Cycle
	ID2D1EllipseGeometry* m_pEllipseArray[GEOMETRY_COUNT];
	ID2D1GeometryGroup* m_pGeometryGroup;

	IDWriteTextFormat* m_pWriteTextFormat;
	IDWriteFactory* m_pWriteFactory;

	HRESULT CreateDeviceIndependentResources();
	HRESULT Initialize();
	HRESULT CreateDeviceResources();
	void DiscardDeviceResources();
	HRESULT OnRender();
	D2D1_SIZE_U CalculateD2DWindowSize();
	void KcycekiumFolderSet(const std::wstring &package);
	void OnResize(
		UINT width,
		UINT height
		);
	std::vector<MetroLabel> label_;
	std::vector<MetroButton> button_;
	std::vector<MetroTextItem> item_;
	bool taskrunning_ = false;
	Argument argument_;
public:
	MetroWindow();
	~MetroWindow();
	LRESULT InitializeWindow();
	DECLARE_WND_CLASS(KRYCEKIUM_UI_MAINWINDOW)
	BEGIN_MSG_MAP(MetroWindow)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_DROPFILES, OnDropfiles)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUP)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
	END_MSG_MAP()
	LRESULT OnCreate(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle);
	LRESULT OnDestroy(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle);
	LRESULT OnClose(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle);
	LRESULT OnSize(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle);
	LRESULT OnPaint(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle);
	LRESULT OnDropfiles(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL & bHandled);
	LRESULT OnLButtonUP(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle);
	LRESULT OnLButtonDown(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle);
	LRESULT KrycekiumDiscoverPackageButtonActive(const wchar_t *debugMessage);
	LRESULT KrycekiumDiscoverFolderButtonActive(const wchar_t *debugMessage);
	LRESULT KrycekiumTaskRun(const wchar_t *debugMessage);
	bool IsTaskRunning()const { return taskrunning_; }
	void UnsetTaskIsRunning() { taskrunning_ = false; }
	const Argument &Argum()const { return argument_; }
	////
};
#endif
