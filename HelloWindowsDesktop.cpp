#include <windows.h>
#include <windowsx.h>
#include <vector>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <cmath>

// -------------------- Globals --------------------
HINSTANCE g_hInst = nullptr;                    // App instance handling the window

// window layout definitions
int topMargin = 40;                             // Leave some space at the top for the buttons

// Pan feature vars
int g_panX = 0;
int g_panY = 0;

bool g_isPanning = false;
int g_panStartMouseX = 0;
int g_panStartMouseY = 0;
int g_panStartOffsetX = 0;
int g_panStartOffsetY = 0;

// --------- Shape definition (stored in WORLD coordinates) ---------
enum Tool
{
    TOOL_LINE = 0,
    TOOL_RECT,
    TOOL_ELLIPSE,
    TOOL_MULTILINE
};

Tool g_currentTool = TOOL_LINE;

// Button IDs
#define ID_TOOL_LINE    1001
#define ID_TOOL_RECT    1002
#define ID_TOOL_ELLIPSE 1003
#define ID_TOOL_MULTILINE 1004

std::vector<POINT> g_points;

// Basic shapes can be defined only by two points and the type of the shape
struct Shape {
    Tool type;
    POINT p_init; // world coords (start)
    POINT p_end; // world coords (end)
};

std::vector<Shape> g_shapes;                    // Basic shapes
std::vector<std::vector<POINT>> g_poligons;     // Poligons

// Current drawing state (left mouse)
bool g_isDrawing = false;

// Zoom state
double g_zoom = 1.0;

// Button handles
HWND g_hBtnLine = nullptr;
HWND g_hBtnRect = nullptr;
HWND g_hBtnEllipse = nullptr;
HWND g_hBtnMultiLine = nullptr;

// -------------------- Forward declarations --------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void printConsole(const std::ostringstream& oss);
void printConsolePoints();
bool getMouseWorldCoord(LPARAM lParam, POINT& out);

// -------------------- WinMain --------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    g_hInst = hInstance;

    // windows class name
    const wchar_t CLASS_NAME[] = L"Win32GDI_ToolbarDemo";

    // Windows class attributes
    WNDCLASS wc{};
    wc.lpfnWndProc = WndProc;                           // window procedure (logic)
    wc.hInstance = hInstance;                           // App instance handling the window 
    wc.lpszClassName = CLASS_NAME;                      // window custom class name
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);        // cursor/pointer
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);      // background color handler

    // registering window
    if (!RegisterClass(&wc))
        return 0;


    HWND hwnd = CreateWindowEx(
        0,                                  // Extended window style
        CLASS_NAME,                         // pointer to the name of registered class
        L"Win32 + GDI - Toolbar Demo",      // pointer to window name
        WS_OVERLAPPEDWINDOW,                // window style
        CW_USEDEFAULT, CW_USEDEFAULT,       // horizontal and vertical window position
        800, 600,                           // width and height of window
        nullptr,                            // father window handler
        nullptr,                            // menu handler or window son ID
        hInstance,                          // App instance handling the window
        nullptr);                           // pointer to window creation data

    if (!hwnd)
        return 0;

    // Attach a console window
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    std::cout << "Hello from console!\n";

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

// -------------------- Helper: update title with current tool --------------------
void UpdateWindowTitleWithTool(HWND hwnd)
{
    const wchar_t* toolName = L"Line";
    switch (g_currentTool)
    {
        case TOOL_LINE:    toolName = L"Line";    break;
        case TOOL_RECT:    toolName = L"Rect";    break;
        case TOOL_ELLIPSE: toolName = L"Ellipse"; break;
        case TOOL_MULTILINE: toolName = L"Multi Line"; break;
    }

    wchar_t title[256];
    wsprintf(title, L"Win32 + GDI - Toolbar Demo [Tool: %s]", toolName);
    SetWindowText(hwnd, title);
}

// ---------------------- Helper: convert world vs screen coordinates ------------------
// Convert screen (client) coordinates → world coordinates
void ScreenToWorld(int sx, int sy, double& wx, double& wy)
{
    // Remove pan and top margin, then divide by zoom
    wx = (sx - g_panX) / g_zoom;
    wy = (sy - topMargin - g_panY) / g_zoom;
}

// Convert world → screen
void WorldToScreen(double wx, double wy, int& sx, int& sy)
{
    sx = (int)(wx * g_zoom) + g_panX;
    sy = (int)(wy * g_zoom) + g_panY + topMargin;
}

// -------------------- WndProc --------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            // Create 4 "toolbar" buttons at the top
            const int btnWidth = 80;
            const int btnHeight = 30;
            const int y = 0;

            // Group them as radio-style push buttons so only one is active
            g_hBtnLine = CreateWindowEx(
                0, L"BUTTON", L"Line",
                WS_CHILD | WS_VISIBLE | BS_PUSHLIKE | BS_AUTORADIOBUTTON | WS_GROUP,
                0, y, btnWidth, btnHeight,
                hwnd,
                (HMENU)ID_TOOL_LINE,
                g_hInst,
                nullptr);

            g_hBtnRect = CreateWindowEx(
                0, L"BUTTON", L"Rect",
                WS_CHILD | WS_VISIBLE | BS_PUSHLIKE | BS_AUTORADIOBUTTON,
                btnWidth, y, btnWidth, btnHeight,
                hwnd,
                (HMENU)ID_TOOL_RECT,
                g_hInst,
                nullptr);

            g_hBtnEllipse = CreateWindowEx(
                0, L"BUTTON", L"Ellipse",
                WS_CHILD | WS_VISIBLE | BS_PUSHLIKE | BS_AUTORADIOBUTTON,
                2 * btnWidth, y, btnWidth, btnHeight,
                hwnd,
                (HMENU)ID_TOOL_ELLIPSE,
                g_hInst,
                nullptr);

            g_hBtnMultiLine = CreateWindowEx(
                0, L"BUTTON", L"MultiLine",
                WS_CHILD | WS_VISIBLE | BS_PUSHLIKE | BS_AUTORADIOBUTTON,
                3 * btnWidth, y, btnWidth, btnHeight,
                hwnd,
                (HMENU)ID_TOOL_MULTILINE,
                g_hInst,
                nullptr);

            // Set default checked button
            SendMessage(g_hBtnLine, BM_SETCHECK, BST_CHECKED, 0);

            UpdateWindowTitleWithTool(hwnd);
            return 0;
        }

        case WM_COMMAND:
        {
            const int wmId = LOWORD(wParam);
            const int wmEvent = HIWORD(wParam);

            if (wmEvent == BN_CLICKED)
            {
                switch (wmId)
                {
                case ID_TOOL_LINE:
                    g_currentTool = TOOL_LINE;
                    UpdateWindowTitleWithTool(hwnd);
                    InvalidateRect(hwnd, nullptr, TRUE);
                    break;

                case ID_TOOL_RECT:
                    g_currentTool = TOOL_RECT;
                    UpdateWindowTitleWithTool(hwnd);
                    InvalidateRect(hwnd, nullptr, TRUE);
                    break;

                case ID_TOOL_ELLIPSE:
                    g_currentTool = TOOL_ELLIPSE;
                    UpdateWindowTitleWithTool(hwnd);
                    InvalidateRect(hwnd, nullptr, TRUE);
                    break;
                case ID_TOOL_MULTILINE:
                    g_currentTool = TOOL_MULTILINE;
                    UpdateWindowTitleWithTool(hwnd);
                    InvalidateRect(hwnd, nullptr, TRUE);
                    break;
                }
            }

            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == 'E') {
                // Start drawing a new shape
                g_isDrawing = true;
            }
            return 0;
        }

        case WM_KEYUP: {
            if (wParam == 'E') {
                // End drawing a new shape
                //ReleaseCapture();                     // Release mouse capture
                g_isDrawing = false;

                if (g_points.size() == 0)
                    return 0;

                if (g_points.size() == 1) {
                    POINT tmp_p = g_points[0];
                    g_points.push_back(tmp_p);
                }

                if (g_currentTool == TOOL_MULTILINE) {
                    g_poligons.push_back(g_points);
                }
                else {
                    Shape s{};
                    s.type = g_currentTool;
                    s.p_init = g_points[0];
                    s.p_end = g_points[1];

                    g_shapes.push_back(s);
                }
                g_points.clear();
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            if (g_isDrawing) {
                std::ostringstream oss;

                POINT tmp_pnt;
                if (!getMouseWorldCoord(lParam, tmp_pnt)) {
                    return 0;
                }

                if (g_currentTool == TOOL_MULTILINE) {
                    g_points.push_back(tmp_pnt);
                }
                else {
                    if (g_points.size() <= 1) {
                        g_points.push_back(tmp_pnt);
                    }
                    else {
                        g_points[0] = g_points[1];
                        g_points[1] = tmp_pnt;
                    }
                }
                printConsolePoints();
                InvalidateRect(hwnd, nullptr, TRUE);
            }

            //SetCapture(hwnd); // capture mouse while drawing (keep getting mouse move events while dragging)
            return 0;
        }

        case WM_RBUTTONDOWN:
        {
            // Start panning
            g_isPanning = true;
            SetCapture(hwnd); // capture mouse until button is released

            g_panStartMouseX = GET_X_LPARAM(lParam);
            g_panStartMouseY = GET_Y_LPARAM(lParam);

            g_panStartOffsetX = g_panX;
            g_panStartOffsetY = g_panY;

            return 0;
        }

        case WM_LBUTTONUP:
        {
            // Implement mouse left click button release trigger
            return 0;
        }

        case WM_RBUTTONUP:
        {
            if (g_isPanning)
            {
                g_isPanning = false;
                ReleaseCapture();
            }
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            if (g_isPanning)
            {
                int sx = GET_X_LPARAM(lParam);
                int sy = GET_Y_LPARAM(lParam);

                int dx = sx - g_panStartMouseX;
                int dy = sy - g_panStartMouseY;

                g_panX = g_panStartOffsetX + dx;
                g_panY = g_panStartOffsetY + dy;

                InvalidateRect(hwnd, nullptr, TRUE);
            }

            return 0;
        }

        case WM_MOUSEWHEEL:
        {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam); // positive = wheel up
            if (delta != 0)
            {
                // zoom factor per wheel notch
                double factor = (delta > 0) ? 1.1 : (1.0 / 1.1);

                // Mouse position in screen coords
                POINT pt;
                pt.x = GET_X_LPARAM(lParam);
                pt.y = GET_Y_LPARAM(lParam);

                // Convert to client coords
                ScreenToClient(hwnd, &pt);
                int clientX = pt.x;
                int clientY = pt.y;

                // Optional: ignore zoom if over toolbar
                if (clientY < topMargin)
                    return 0;

                // World coordinates of the point under the cursor BEFORE zoom
                double worldX = (clientX - g_panX) / g_zoom;
                double worldY = (clientY - topMargin - g_panY) / g_zoom;

                // Apply zoom with clamping
                double newZoom = g_zoom * factor;
                if (newZoom < 0.1) newZoom = 0.1;
                if (newZoom > 10.0) newZoom = 10.0;

                // Adjust pan so that the same world point stays under the cursor
                g_panX = (int)(clientX - worldX * newZoom);
                g_panY = (int)(clientY - topMargin - worldY * newZoom);

                g_zoom = newZoom;

                UpdateWindowTitleWithTool(hwnd);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // ---- Draw all stored shapes ----
            HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 255));
            HBRUSH hBr = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
            HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH oldBr = (HBRUSH)SelectObject(hdc, hBr);

            // draw shapes
            for (const Shape& s : g_shapes)
            {
                int sx1, sy1, sx2, sy2;
                WorldToScreen(s.p_init.x, s.p_init.y, sx1, sy1);
                WorldToScreen(s.p_end.x, s.p_end.y, sx2, sy2);

                switch (s.type)
                {
                case TOOL_LINE:
                    MoveToEx(hdc, sx1, sy1, nullptr);
                    LineTo(hdc, sx2, sy2);
                    break;

                case TOOL_RECT:
                    Rectangle(hdc, sx1, sy1, sx2, sy2);
                    break;

                case TOOL_ELLIPSE:
                    Ellipse(hdc, sx1, sy1, sx2, sy2);
                    break;
                }
            }

            // draw polygons
            for (const std::vector<POINT>& polyWorld : g_poligons) {
                if (polyWorld.size() < 2)
                    continue;

                // build a screen-space copy
                std::vector<POINT> polyScreen;
                polyScreen.resize(polyWorld.size());

                for (size_t i = 0; i < polyWorld.size(); ++i)
                {
                    int sx, sy;
                    WorldToScreen(polyWorld[i].x, polyWorld[i].y, sx, sy);
                    polyScreen[i].x = sx;
                    polyScreen[i].y = sy;
                }

                Polygon(hdc, polyScreen.data(), static_cast<int>(polyScreen.size()));
            }

            // ---- Draw current in-progress shape (preview) ----
            if (g_isDrawing && g_points.size() > 1)
            {
                int sx1, sy1, sx2, sy2;
                WorldToScreen(g_points[0].x, g_points[0].y, sx1, sy1);
                WorldToScreen(g_points[1].x, g_points[1].y, sx2, sy2);

                switch (g_currentTool)
                {
                case TOOL_LINE:
                    MoveToEx(hdc, sx1, sy1, nullptr);
                    LineTo(hdc, sx2, sy2);
                    break;

                case TOOL_RECT:
                    Rectangle(hdc, sx1, sy1, sx2, sy2);
                    break;

                case TOOL_ELLIPSE:
                    Ellipse(hdc, sx1, sy1, sx2, sy2);
                    break;
                case TOOL_MULTILINE:
                    std::vector<POINT> polyScreen;
                    polyScreen.resize(g_points.size());

                    for (size_t i = 0; i < g_points.size(); ++i)
                    {
                        int sx, sy;
                        WorldToScreen(g_points[i].x, g_points[i].y, sx, sy);
                        polyScreen[i].x = sx;
                        polyScreen[i].y = sy;
                    }

                    Polygon(hdc, polyScreen.data(), static_cast<int>(polyScreen.size()));
                    break;
                }
            }

            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBr);
            DeleteObject(hPen);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void printConsole(const std::ostringstream& oss) {
    std::string msg = oss.str();
    std::cout << msg;
}

void printConsolePoints() {
    std::ostringstream oss;
    oss << "----- g_points list -----\n";
    for (int i = 0; i < g_points.size(); i++) {
        oss << "Point " << i << ": x->" << g_points[i].x << ", y->" << g_points[i].y << "\n";
    }
    printConsole(oss);
}

bool getMouseWorldCoord(LPARAM lParam, POINT& out) {
    int sx = GET_X_LPARAM(lParam);
    int sy = GET_Y_LPARAM(lParam);

    std::ostringstream oss;
    oss << "Screen coord: " << sx << " " << sy << "\n";
    printConsole(oss);

    // Ignore clicks on toolbar area
    if (sy < topMargin) {
        return false;
    }

    double currentX, currentY;

    ScreenToWorld(sx, sy, currentX, currentY);

    out.x = currentX;
    out.y = currentY;

    return true;
}