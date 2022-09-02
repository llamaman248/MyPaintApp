// MyPaintApp.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "MyPaintApp.h"

#define MAX_LOADSTRING 100
//((COLORREF)((((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16))|(((DWORD)(BYTE)(a))<<24)))
#define RGBA(r,g,b,a) (RGB(r,g,b) | (a << 24))
// Global Variables:
RGBQUAD* pEditingDIBSBits = 0;         // Array for bits of editing bitmap
HBITMAP hEditingDIBSection = 0;      // editing bitmap handle
HDC hdcEditingDIBS = 0;
SIZE szEditingDIBS = {};
HBITMAP hBufferDDB;              // Window screen buffer bitmap
HDC hdcBufferDDB;
HDC hdcWnd;
HBRUSH bkgBrush = CreateSolidBrush(RGB(45, 45, 45));
HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));

HANDLE oldDIBS;

HBITMAP transparentImageBackground; // Consider making this an HBRUSH

HWND hMainWindow;
RECT clientRect;

bool savedEditingImage = true;
POINT displayImageOffset = {};
POINT tempZoomImageOffset = {};
float zoomRatio = 1.0f;
POINTS lastMousePos = {};
POINT lastDragMouseClick = {};
POINT lastZoomDragMousePosition = {};
bool drawing = true;
bool dragging = false;
bool spaceDown = false;

BITMAPINFO bmpIn = {};

HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    NewImageDia(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    NewPenDia(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

void calculateZoomDragMousePos()
{
    lastZoomDragMousePosition.x = (lastMousePos.x - displayImageOffset.x - tempZoomImageOffset.x) / zoomRatio;
    lastZoomDragMousePosition.y = (lastMousePos.y - displayImageOffset.y - tempZoomImageOffset.y) / zoomRatio;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MYPAINTAPP, szWindowClass, MAX_LOADSTRING);
    
    // Register class
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MYPAINTAPP));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_MYPAINTAPP);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassExW(&wcex);

    // Perform application initialization:

    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MYPAINTAPP));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    return (int) msg.wParam;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            case ID_FILE_NEW:
                if (hEditingDIBSection)
                {
                    if (!savedEditingImage)
                    {
                        if (MessageBox(hWnd, L"The image you are editing is not saved. Delete it?", L"Image Not Saved", MB_YESNO | MB_ICONWARNING) == IDNO)
                        {
                            return 0;
                        }
                    }
                    DeleteObject(hEditingDIBSection);
                }
                {
                    SIZE szImg = {};
                    if (DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_NEWIMAGE), hWnd, NewImageDia, (LPARAM)&szImg))
                    {
                        szEditingDIBS = szImg;
                        // Clicked 'Ok'
                        if (szImg.cx < 5000 && szImg.cy < 5000) // TODO: change it so the limit is enfoced while typing
                        {
                            
                            bmpIn.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                            bmpIn.bmiHeader.biPlanes = 1;
                            bmpIn.bmiHeader.biWidth = szImg.cx;
                            bmpIn.bmiHeader.biHeight = -szImg.cy; // Top down image
                            bmpIn.bmiHeader.biBitCount = 32; // Will change to 32 to support transparency
                            bmpIn.bmiHeader.biCompression = BI_RGB;
                            bmpIn.bmiHeader.biSizeImage = 0; //(can be 0 for BI_RGB bitmaps) bmpIn.bmiHeader.biBitCount * szImg.cx * szImg.cy; // Could be incorrect
                            hEditingDIBSection = CreateDIBSection(hdcEditingDIBS, &bmpIn, /*TODO: ask mode in dialog*/DIB_RGB_COLORS, (void**)&pEditingDIBSBits, NULL, 0);
                            if (hEditingDIBSection == NULL)
                            {
                                if (GetLastError() == ERROR_INVALID_PARAMETER)
                                {
                                    MessageBox(hWnd, L"Failed. Invalid param", L"D", MB_ICONERROR);
                                }
                                MessageBox(hWnd, L"Failed. But not invalid param", L"D", MB_ICONERROR);
                            }
                            else
                            {
                                SelectObject(hdcEditingDIBS, hEditingDIBSection);
                                savedEditingImage = false;
                                InvalidateRect(hWnd, NULL, FALSE); // Maybe make it only invalidate the region of the image
                            }
                        }
                        else
                        {
                            MessageBox(hWnd, L"Image too big", L"Temp error to move to dialog", MB_ICONHAND); // ICONHAND is the same as ICONERROR
                        }
                    }
                    break;
                }
            case ID_BRUSH_NEWBRUSH:
                // GDI Pen, not brush. It says brush because I think it sounds better for my software. (Might change). Brushes are actually fills in GDI
            {
                HPEN newPen = (HPEN)DialogBox(hInst, MAKEINTRESOURCE(IDD_PEN), hWnd, NewPenDia);
                if (newPen)
                {
                    SelectObject(hdcEditingDIBS, newPen);
                    if (pen)
                        DeleteObject(pen);
                    pen = newPen;
                }
            }
                break;
            case ID_VIEW_RESETVIEW:
                displayImageOffset = {};
                tempZoomImageOffset = {};
                zoomRatio = 1.0f;
                SetStretchBltMode(hdcBufferDDB, BLACKONWHITE);
                InvalidateRect(hWnd, NULL, FALSE);
                break;
            case ID_FILE_SAVE:
            {
                
                HANDLE savingFile = CreateFile(L"image.bmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

                BITMAPFILEHEADER bmpfh = {};
                //BITMAPINFOHEADER bmiHeader = bmpIn.bmiHeader; // Do not need a normal BITMAPINFO because the color index table is not needed for RGB
                //bmiHeader.biWidth = szEditingDIBS.cx;
                //bmiHeader.biHeight = -szEditingDIBS.cy;
                //bmiHeader.biPlanes = 1;
                //bmiHeader.biBitCount = 32;
                //bmiHeader.biCompression = BI_RGB;
                //bmiHeader.biSizeImage = 0;
                //bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                /*bi.bmiColors[0].rgbRed = 255;
                bi.bmiColors[0].rgbGreen = 255;
                bi.bmiColors[0].rgbBlue = 255;
                bi.bmiColors[0].rgbReserved = 255;*/
                bmpfh.bfOffBits = sizeof(BITMAPINFOHEADER) + sizeof(BITMAPFILEHEADER);  // This one and the next one don't seem to be required
                bmpfh.bfSize = bmpfh.bfOffBits + szEditingDIBS.cx * szEditingDIBS.cy * bmpIn.bmiHeader.biBitCount;
                bmpfh.bfType = 0x4d42;  // "BM" equivalant to this, but in value not address form. Equals ('B' | ('M' << 8))

                DWORD bytesWritten = 0;

                // TODO: add error messages for if this fails
                WriteFile(savingFile, &bmpfh, sizeof(BITMAPFILEHEADER), &bytesWritten, NULL);
                WriteFile(savingFile, &bmpIn.bmiHeader, sizeof(BITMAPINFOHEADER), &bytesWritten, NULL);
                WriteFile(savingFile, pEditingDIBSBits, szEditingDIBS.cx * szEditingDIBS.cy * sizeof(RGBQUAD), &bytesWritten, NULL);
                savedEditingImage = true; // Prevents warning message when NEW is selected
            done:
                CloseHandle(savingFile);
            }
            break;
            case ID_FILE_OPEN:
            {
                if (hEditingDIBSection)
                {
                    if (!savedEditingImage)
                    {
                        if (MessageBox(hWnd, L"The image you are editing is not saved. Delete it?", L"Image Not Saved", MB_YESNO | MB_ICONWARNING) == IDNO)
                        {
                            return 0;
                        }
                    }
                    DeleteObject(hEditingDIBSection);  // Does not appear to actualy delete it. May need to select the new one first.
                }
                HANDLE savingFile = CreateFile(L"image.bmp", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (savingFile == INVALID_HANDLE_VALUE)
                {
                    MessageBox(hWnd, L"No Image is saved.", L"My Paint App", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                BITMAPFILEHEADER bmfh = {};
                DWORD bytesRead = 0;
                DWORD dwValue;
                // TODO: support multiple file formats (at least multiple bitmap formats)
                if (!ReadFile(savingFile, &bmfh, sizeof(BITMAPFILEHEADER), &bytesRead, NULL))
                {
                    MessageBox(hWnd, L"no Work", L"D", MB_OK);
                    goto doneReading;
                }
                
                
                if (!ReadFile(savingFile, &dwValue, sizeof(DWORD), &bytesRead, NULL))
                {
                    MessageBox(hWnd, L"no Work", L"D", MB_OK);
                    goto doneReading;
                }

                
                if (dwValue != sizeof(BITMAPINFOHEADER))
                {
                    MessageBox(hWnd, L"no Work. Header is the wrong size", L"D", MB_OK);
                    goto doneReading;
                }
                
                SetFilePointer(savingFile, sizeof(BITMAPFILEHEADER), NULL, FILE_BEGIN);
                bmpIn = {};
                if (!ReadFile(savingFile, &bmpIn, sizeof(BITMAPINFOHEADER), &bytesRead, NULL))
                {
                    MessageBox(hWnd, L"no Work", L"D", MB_OK);
                    goto doneReading;
                }

                if (bmpIn.bmiHeader.biCompression != BI_RGB)
                {
                    MessageBox(hWnd, L"Cannot open compressed image.", L"My Paint App", MB_OK);
                    goto doneReading;
                }
                hEditingDIBSection = CreateDIBSection(hdcEditingDIBS, &bmpIn, DIB_RGB_COLORS, (void**)&pEditingDIBSBits, NULL, 0);
                szEditingDIBS.cx = bmpIn.bmiHeader.biWidth;
                szEditingDIBS.cy = abs(bmpIn.bmiHeader.biHeight);
                if (hEditingDIBSection == NULL)
                {
                    DWORD value = GetLastError();
                    if (value == ERROR_INVALID_PARAMETER)
                    {
                        MessageBox(hWnd, L"Failed. Invalid param", L"D", MB_ICONERROR);
                    }
                    MessageBox(hWnd, L"Failed. But not invalid param", L"D", MB_ICONERROR);
                }
                if (hEditingDIBSection)
                {
                    SelectObject(hdcEditingDIBS, hEditingDIBSection); // it jumps this
                }
                SetFilePointer(savingFile, bmfh.bfOffBits, 0, FILE_BEGIN);
                if (!ReadFile(savingFile, pEditingDIBSBits, szEditingDIBS.cx * szEditingDIBS.cy * (bmpIn.bmiHeader.biBitCount / 8), &bytesRead, NULL))
                {
                    MessageBox(hWnd, L"no Work", L"D", MB_OK);
                    goto doneReading;
                }
                
                doneReading:
                CloseHandle(savingFile);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_CREATE:

        // TODO: make this all a function so when monitors get changed it can redo everything so nothing breaks
        hdcWnd = GetDC(hWnd);
        hdcBufferDDB = CreateCompatibleDC(hdcWnd);
        GetClientRect(hWnd, &clientRect);
        
        // Separate from planned function:
        hdcEditingDIBS = CreateCompatibleDC(hdcWnd);
        // Pen stuff (might happen somewhere else)
        SelectObject(hdcEditingDIBS, pen);
        break;
    case WM_PAINT:
        {
            GetClientRect(hWnd, &clientRect);
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            if (hBufferDDB)
                DeleteObject(hBufferDDB);
            hBufferDDB = CreateCompatibleBitmap(hdcWnd, clientRect.right, clientRect.bottom);
            SelectObject(hdcBufferDDB, hBufferDDB);
            // Fill of background. Consider making this not go to the buffer, but directly to the window (so when it's resized, it does not just show white while the drawing code happens)
            FillRect(hdcBufferDDB, &ps.rcPaint, bkgBrush);

            if (hEditingDIBSection)
            {
                // There is an image to edit
                // Copy image to display buffer. TODO: make the image able to display at different locations by mouse dragging controls.
                StretchBlt(hdcBufferDDB, displayImageOffset.x + tempZoomImageOffset.x, displayImageOffset.y + tempZoomImageOffset.y, szEditingDIBS.cx* zoomRatio, szEditingDIBS.cy* zoomRatio, hdcEditingDIBS, 0, 0, szEditingDIBS.cx, szEditingDIBS.cy, SRCCOPY);
                //BitBlt(hdcBufferDDB, 0, 0, clientRect.right, clientRect.bottom, hdcEditingDIBS, -displayImageOffset.x, -displayImageOffset.y, SRCCOPY);
            }
            BitBlt(hdc, 0, 0, ps.rcPaint.right, ps.rcPaint.bottom, hdcBufferDDB, 0, 0, SRCCOPY);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        drawing = !spaceDown;
        dragging = spaceDown;
        POINTS mp = MAKEPOINTS(lParam);
        lastDragMouseClick.x = mp.x - displayImageOffset.x;
        lastDragMouseClick.y = mp.y - displayImageOffset.y;
        if (hEditingDIBSection && drawing)
        {
            savedEditingImage = false; // Adds a warning for creating a NEW image (so you don't forget to SAVE)
            MoveToEx(hdcEditingDIBS, lastZoomDragMousePosition.x, lastZoomDragMousePosition.y, NULL);
            LineTo(hdcEditingDIBS, lastZoomDragMousePosition.x + 1, lastZoomDragMousePosition.y);
            MoveToEx(hdcEditingDIBS, lastZoomDragMousePosition.x, lastZoomDragMousePosition.y, NULL);
            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    case WM_LBUTTONUP:
        displayImageOffset.x += tempZoomImageOffset.x;
        displayImageOffset.y += tempZoomImageOffset.y;
        tempZoomImageOffset = {};
        break;
    case WM_MOUSEMOVE:
        lastMousePos = MAKEPOINTS(lParam);
        calculateZoomDragMousePos();
        if (dragging)
        {
            if (wParam & MK_LBUTTON && spaceDown)
            {
                // Dragging
                displayImageOffset.x = GET_X_LPARAM(lParam) - lastDragMouseClick.x;
                displayImageOffset.y = GET_Y_LPARAM(lParam) - lastDragMouseClick.y;
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        else
        if (hEditingDIBSection && (wParam & MK_LBUTTON))
        {
            // Drawing
            LineTo(hdcEditingDIBS, lastZoomDragMousePosition.x, lastZoomDragMousePosition.y);
            LineTo(hdcEditingDIBS, lastZoomDragMousePosition.x + 1, lastZoomDragMousePosition.y);
            MoveToEx(hdcEditingDIBS, lastZoomDragMousePosition.x, lastZoomDragMousePosition.y, NULL);
            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_SPACE:
            // Drag button
            drawing = false;
            spaceDown = true;
            break;
        case VK_UP:
            // Zoom in (will change to move up one pixel and have plus be zoom in)
            
            break;
        case VK_DOWN:
            // Opposite of Up
            
            break;
        }
        break;
    case WM_KEYUP:
        if (wParam == VK_SPACE)
        {
            spaceDown = false;
        }
        break;
    case WM_MOUSEWHEEL:
        calculateZoomDragMousePos();
        {
            float scrollTicks = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            if (scrollTicks < 0)
            {
                zoomRatio *= pow(0.9f, -scrollTicks);
                if (zoomRatio < 1.0f)
                {
                    SetStretchBltMode(hdcBufferDDB, HALFTONE);
                }
                tempZoomImageOffset.x -= ((float)szEditingDIBS.cx * zoomRatio - (szEditingDIBS.cx * (zoomRatio * 1.11111111f))) * ((float)lastZoomDragMousePosition.x / (float)szEditingDIBS.cx);
                tempZoomImageOffset.y -= ((float)szEditingDIBS.cy * zoomRatio - (szEditingDIBS.cy * (zoomRatio * 1.11111111f))) * ((float)lastZoomDragMousePosition.y / (float)szEditingDIBS.cy);
                
                InvalidateRect(hWnd, NULL, FALSE);
            }
            if (scrollTicks > 0)
            {
                // Possibly make it so when it is so zoomed out that the thing rounds to zero, check for that and get it off of zero by adding (because mutiplying cannot)
                zoomRatio *= pow(1.11111111f,  scrollTicks);
                if (zoomRatio >= 1.0f)
                {
                    SetStretchBltMode(hdcBufferDDB, BLACKONWHITE);
                }
                tempZoomImageOffset.x -= ((float)szEditingDIBS.cx * zoomRatio - (szEditingDIBS.cx * (zoomRatio * 0.9f))) * ((float)lastZoomDragMousePosition.x / (float)szEditingDIBS.cx);
                tempZoomImageOffset.y -= ((float)szEditingDIBS.cy * zoomRatio - (szEditingDIBS.cy * (zoomRatio * 0.9f))) * ((float)lastZoomDragMousePosition.y / (float)szEditingDIBS.cy);
                
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        
        break;
    case WM_DESTROY:
        // Check and make sure the memory is released correctly
        ReleaseDC(hWnd, hdcWnd);
        DeleteObject(hBufferDDB);
        DeleteDC(hdcBufferDDB);
        if (hEditingDIBSection)
            DeleteObject(hEditingDIBSection);
        DeleteDC(hdcEditingDIBS);
        DeleteObject(bkgBrush);
        DeleteObject(pen);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK NewPenDia(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    //UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
    {
        //COLORREF d = RGB(200, 100, 0) | (50 << 24);   // Example of adding alpha to a use of the RGB macro
        SetDlgItemInt(hDlg, IDC_EDIT_RED, 255, FALSE);
        SetDlgItemInt(hDlg, IDC_EDIT_GREEN, 255, FALSE);
        SetDlgItemInt(hDlg, IDC_EDIT_BLUE, 255, FALSE);
        SetDlgItemInt(hDlg, IDC_EDIT_OPACITY, 0, FALSE);
        SetDlgItemInt(hDlg, IDC_EDIT_WIDTH, 1, FALSE);
        SetDlgItemText(hDlg, IDC_STATIC_RED, TEXT("Red"));
        SetDlgItemText(hDlg, IDC_STATIC_GREEN, TEXT("Green"));
        SetDlgItemText(hDlg, IDC_STATIC_BLUE, TEXT("Blue"));
        SetDlgItemText(hDlg, IDC_STATIC_OPACITY, TEXT("Opacity"));
        SetDlgItemText(hDlg, IDC_STATIC_WIDTH, TEXT("Width"));
    }
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDCANCEL:
            EndDialog(hDlg, NULL);
            return (INT_PTR)TRUE;
        case IDOK:
        {
            UINT red = GetDlgItemInt(hDlg, IDC_EDIT_RED, NULL, FALSE);
            UINT green = GetDlgItemInt(hDlg, IDC_EDIT_GREEN, NULL, FALSE);
            UINT blue = GetDlgItemInt(hDlg, IDC_EDIT_BLUE, NULL, FALSE);
            if (red > 255 || blue > 255 || green > 255)
            {
                MessageBox(hDlg, L"Each color channel must be in the range 0~255", L"Value out of range", MB_OK | MB_ICONWARNING);
                return (INT_PTR)TRUE;
            }
            EndDialog(hDlg, (INT_PTR)CreatePen(BS_SOLID, GetDlgItemInt(hDlg, IDC_EDIT_WIDTH, NULL, FALSE), RGB(red, green, blue)));
            return (INT_PTR)TRUE;
        }
        }
        break;
    }
    return (INT_PTR)FALSE;
}


PSIZE pS = NULL;    // I should test mechanics of limited-scope global variables (like see how they work in multithreading to see if an object-orientied design is needed)
INT_PTR CALLBACK NewImageDia(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    //UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        SetDlgItemInt(hDlg, IDC_EDIT_WIDTH, 1920, FALSE);
        SetDlgItemInt(hDlg, IDC_EDIT_HEIGHT, 1080, FALSE);
        SetDlgItemText(hDlg, IDC_STATIC_WIDTH, TEXT("Width"));
        SetDlgItemText(hDlg, IDC_STATIC_HEIGHT, TEXT("Height"));
        pS = (PSIZE)lParam;
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
            pS = NULL;
            EndDialog(hDlg, FALSE);
            return (INT_PTR)TRUE;
        case IDOK:
            if (pS)
            {
                // TODO: limit number of characters allowed in text boxes
                pS->cx = GetDlgItemInt(hDlg, IDC_EDIT_WIDTH, NULL, FALSE);
                pS->cy = GetDlgItemInt(hDlg, IDC_EDIT_HEIGHT, NULL, FALSE);
                if (pS->cx && pS->cy)
                {
                    pS = NULL;
                    EndDialog(hDlg, TRUE);
                }
                else
                {
                    MessageBox(hDlg, L"Width and height must be greater than 0", L"Invalid Input", MB_OK | MB_ICONWARNING);
                }
                return TRUE;
            }
            MessageBox(hDlg, L"UNABLE TO RETREAVE DATA. Something went terribly wrong. Maybe you pressed \"Ok\" too fast?", L"Critical Error", MB_OK | MB_ICONERROR);
            EndDialog(hDlg, FALSE);
            return TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}