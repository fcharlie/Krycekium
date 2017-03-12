#include <D2D1.h>

#define SAFE_RELEASE(P) if(P){P->Release() ; P = NULL ;}
const int GEOMETRY_COUNT = 2;

ID2D1Factory*           g_pD2DFactory   = NULL; // Direct2D factory
ID2D1HwndRenderTarget*  g_pRenderTarget = NULL; // Render target
ID2D1SolidColorBrush*   g_pBlackBrush   = NULL; // Outline brush
ID2D1RadialGradientBrush* g_pRadialGradientBrush = NULL ; // Radial gradient brush

// 2 circle to build up a geometry group. this is the outline of the progress bar
D2D1_ELLIPSE g_Ellipse0 = D2D1::Ellipse(D2D1::Point2F(300, 300), 150, 150);
D2D1_ELLIPSE g_Ellipse1 = D2D1::Ellipse(D2D1::Point2F(300, 300), 200, 200);

D2D1_ELLIPSE g_Ellipse[GEOMETRY_COUNT] = 
{
    g_Ellipse0, 
    g_Ellipse1,
};

ID2D1EllipseGeometry* g_pEllipseArray[GEOMETRY_COUNT] = { NULL };
ID2D1GeometryGroup* g_pGeometryGroup = NULL;

VOID CreateD2DResource(HWND hWnd)
{
    if (!g_pRenderTarget)
    {
        HRESULT hr ;

        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory) ;
        if (FAILED(hr))
        {
            MessageBox(hWnd, "Create D2D factory failed!", "Error", 0) ;
            return ;
        }

        // Obtain the size of the drawing area
        RECT rc ;
        GetClientRect(hWnd, &rc) ;

        // Create a Direct2D render target
        hr = g_pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(
            hWnd, 
            D2D1::SizeU(rc.right - rc.left,rc.bottom - rc.top)
            ), 
            &g_pRenderTarget
            ) ;
        if (FAILED(hr))
        {
            MessageBox(hWnd, "Create render target failed!", "Error", 0) ;
            return ;
        }

        // Create the outline brush(black)
        hr = g_pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::Black),
            &g_pBlackBrush
            ) ;
        if (FAILED(hr))
        {
            MessageBox(hWnd, "Create outline brush(black) failed!", "Error", 0) ;
            return ;
        }

        // Define gradient stops
        D2D1_GRADIENT_STOP gradientStops[2] ;
        gradientStops[0].color = D2D1::ColorF(D2D1::ColorF::Blue) ;
        gradientStops[0].position = 0.f ;
        gradientStops[1].color = D2D1::ColorF(D2D1::ColorF::White) ;
        gradientStops[1].position = 1.f ;

        // Create gradient stops collection
        ID2D1GradientStopCollection* pGradientStops = NULL ;
        hr = g_pRenderTarget->CreateGradientStopCollection(
            gradientStops,
            2, 
            D2D1_GAMMA_2_2,
            D2D1_EXTEND_MODE_CLAMP,
            &pGradientStops
            ) ;
        if (FAILED(hr))
        {
            MessageBox(NULL, "Create gradient stops collection failed!", "Error", 0);
        }

        // Create a linear gradient brush to fill in the ellipse
        hr = g_pRenderTarget->CreateRadialGradientBrush(
            D2D1::RadialGradientBrushProperties(
            D2D1::Point2F(170, 170),
            D2D1::Point2F(0, 0),
            150,
            150),
            pGradientStops,
            &g_pRadialGradientBrush
            ) ;

        if (FAILED(hr))
        {
            MessageBox(hWnd, "Create linear gradient brush failed!", "Error", 0) ;
            return ;
        }

        // Create the 2 ellipse.
        for (int i = 0; i < GEOMETRY_COUNT; ++i)
        {
            hr = g_pD2DFactory->CreateEllipseGeometry(g_Ellipse[i], &g_pEllipseArray[i]);
            if (FAILED(hr)) 
            {
                MessageBox(hWnd, "Create Ellipse Geometry failed!", "Error", 0);
                return;
            }
        }

        // Create the geometry group, the 2 circles make up a group.
        hr = g_pD2DFactory->CreateGeometryGroup(
            D2D1_FILL_MODE_ALTERNATE,
            (ID2D1Geometry**)&g_pEllipseArray,
            ARRAYSIZE(g_pEllipseArray),
            &g_pGeometryGroup
        );
    }
}

VOID Render(HWND hwnd)
{
    // total angle to rotate
    static float totalAngle = 0.0f;

    // Get last time
    static DWORD lastTime = timeGetTime();

    // Get current time
    DWORD currentTime = timeGetTime();

    // Calculate time elapsed in current frame.
    float timeDelta = (float)(currentTime - lastTime) * 0.1;

    // Increase the totalAngle by the time elapsed in current frame.
    totalAngle += timeDelta;

    CreateD2DResource(hwnd) ;

    g_pRenderTarget->BeginDraw() ;

    // Clear background color to White
    g_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

    // Draw geometry group
    g_pRenderTarget->DrawGeometry(g_pGeometryGroup, g_pBlackBrush);

    // Roatate the gradient brush based on the total elapsed time
    D2D1_MATRIX_3X2_F rotMatrix = D2D1::Matrix3x2F::Rotation(totalAngle, D2D1::Point2F(300, 300));
    g_pRadialGradientBrush->SetTransform(rotMatrix);

    // Fill geometry group with the transformed brush
    g_pRenderTarget->FillGeometry(g_pGeometryGroup, g_pRadialGradientBrush);

    HRESULT hr = g_pRenderTarget->EndDraw() ;
    if (FAILED(hr))
    {
        MessageBox(NULL, "Draw failed!", "Error", 0) ;
        return ;
    }

    // Update last time to current time for next loop
    lastTime = currentTime;
}

VOID Cleanup()
{
    SAFE_RELEASE(g_pRenderTarget) ;
    SAFE_RELEASE(g_pBlackBrush) ;
    SAFE_RELEASE(g_pGeometryGroup);
    SAFE_RELEASE(g_pRadialGradientBrush);

    for (int i = 0; i < GEOMETRY_COUNT; ++i)
    {
        SAFE_RELEASE(g_pEllipseArray[i]);
        g_pEllipseArray[i] = NULL;
    }

    SAFE_RELEASE(g_pD2DFactory) ;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)   
{
    switch (message)    
    {
    case   WM_PAINT:
        Render(hwnd) ;
        return 0 ;

    case WM_KEYDOWN: 
        { 
            switch( wParam ) 
            { 
            case VK_ESCAPE: 
                SendMessage( hwnd, WM_CLOSE, 0, 0 ); 
                break ; 
            default: 
                break ; 
            } 
        } 
        break ; 

    case WM_DESTROY: 
        Cleanup(); 
        PostQuitMessage( 0 ); 
        return 0; 
    }

    return DefWindowProc (hwnd, message, wParam, lParam) ;
}

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow )
{

    WNDCLASSEX winClass ;

    winClass.lpszClassName = "Direct2D";
    winClass.cbSize        = sizeof(WNDCLASSEX);
    winClass.style         = CS_HREDRAW | CS_VREDRAW;
    winClass.lpfnWndProc   = WndProc;
    winClass.hInstance     = hInstance;
    winClass.hIcon         = NULL ;
    winClass.hIconSm       = NULL ;
    winClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    winClass.hbrBackground = NULL ;
    winClass.lpszMenuName  = NULL;
    winClass.cbClsExtra    = 0;
    winClass.cbWndExtra    = 0;

    if (!RegisterClassEx (&winClass))   
    {
        MessageBox ( NULL, TEXT( "This program requires Windows NT!" ), "error", MB_ICONERROR) ;
        return 0 ;  
    }   

    HWND hwnd = CreateWindowEx(NULL,  
        "Direct2D",                 // window class name
        "Circular Progressbar",         // window caption
        WS_OVERLAPPEDWINDOW,        // window style
        CW_USEDEFAULT,              // initial x position
        CW_USEDEFAULT,              // initial y position
        600,                        // initial x size
        600,                        // initial y size
        NULL,                       // parent window handle
        NULL,                       // window menu handle
        hInstance,                  // program instance handle
        NULL) ;                     // creation parameters

        ShowWindow (hwnd, iCmdShow) ;
        UpdateWindow (hwnd) ;

        MSG msg ;  
        ZeroMemory(&msg, sizeof(msg)) ;

        while (GetMessage (&msg, NULL, 0, 0))  
        {
            TranslateMessage (&msg) ;
            DispatchMessage (&msg) ;
        }

        return msg.wParam ;
}