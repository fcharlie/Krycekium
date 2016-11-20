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
#include <mutex>

#define IDC_PACKAGE_URI_EDIT 1010
#define IDC_PACKAGE_VIEW_BUTTON 1011
#define IDC_FOLDER_URI_EDIT 1012
#define IDC_FOLDER_URI_BUTTON 1013
#define IDC_PROCESS_RATE 1014
#define IDC_OPTION_BUTTON_OK 1015
#define IDC_OPTION_BUTTON_CANCEL 1016

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

struct KryceLabel {
	RECT layout;
	std::wstring text;
};

struct KryceText {
	RECT layout;
	std::wstring name;
	std::wstring value;
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
	ID2D1Factory *m_pFactory;
	ID2D1HwndRenderTarget* m_pHwndRenderTarget;
	ID2D1SolidColorBrush* m_pSolidColorBrush;
	ID2D1SolidColorBrush* m_AreaBorderBrush;

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
	std::vector<KryceLabel> label_;
	std::wstring windowTitle;
	bool requireCancel{ false };
	bool mFirstTime{ true };
	bool mEnableActionData{ false };
	bool mForwardProgress{ false };
	bool mScriptInProgress{ false };
	int mProgressTotal{ 0 };
	int mProgress{ 0 };
	int iField[4];
	int iCurPos;
	void InstallContextReset()
	{
		requireCancel = false;
		mEnableActionData = false ;
		mForwardProgress = false;
		mScriptInProgress = false;
		mProgressTotal = 0;
		mProgress = 0;
	}
	std::mutex mtx;
	Argument argument_;
	HWND hUriEdit{ nullptr };
	HWND hDirEdit{ nullptr };
	HWND hUriButton{ nullptr };
	HWND hDirButton{ nullptr };
	HWND hProgress{ nullptr };
	HWND hOK{ nullptr };
	HWND hCancel{ nullptr };

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
		COMMAND_ID_HANDLER(IDC_PACKAGE_VIEW_BUTTON,OnDiscoverPackage)
		COMMAND_ID_HANDLER(IDC_FOLDER_URI_BUTTON, OnDiscoverFolder)
		COMMAND_ID_HANDLER(IDC_OPTION_BUTTON_OK,OnStartTask)
		COMMAND_ID_HANDLER(IDC_OPTION_BUTTON_CANCEL,OnCancelTask)
	END_MSG_MAP()
	LRESULT OnCreate(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle);
	LRESULT OnDestroy(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle);
	LRESULT OnClose(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle);
	LRESULT OnSize(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle);
	LRESULT OnPaint(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandle);
	LRESULT OnDropfiles(UINT nMsg, WPARAM wParam, LPARAM lParam, BOOL & bHandled);
	LRESULT OnDiscoverPackage(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnDiscoverFolder(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnStartTask(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnCancelTask(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	int InstanllUIHandler(UINT iMessageType, LPCWSTR szMessage);
	HWND ProgressHwnd()const { return hProgress; }
	HWND MainHwnd()const { return m_hWnd; }
	const Argument &Argum()const { return argument_; }
	////
};
#endif
