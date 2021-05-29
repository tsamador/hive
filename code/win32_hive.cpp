/*
    Author: Tanner Amador
    Date: 5/28/2021

*/

//#define UNICODE

#include <windows.h>

//Function Prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void ResizeDIBSection(int width, int height);
static void WinUpdateWindow(HDC DeviceContext, int x, int y, int width, int height);


//Global variables
static bool running;  //Global for now. 
static BITMAPINFO bitmapInfo;
static void *bitmapMemory;
static HBITMAP bitmapHandle;
static HDC bitmapDeviceContext;

//WinMain is the entry point for windows
INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR lpCmdLine, INT nCmdShow)
{
    
    //Creating our Window Class
    WNDCLASSA WindowClass = {}; //Look up this class on MSDN on repeat. 
    WindowClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;  
    WindowClass.lpfnWndProc = WindowProc; 
    WindowClass.hInstance = hInstance;
    //WindowClass.hIcon = ;
    WindowClass.lpszClassName = "HiveWindowClass";

    //Now register the window class
    if(RegisterClassA(&WindowClass))   //This returns an atom, probably not going to be used. 
    {
        HWND windowHandle = CreateWindowExA( 
                    0,
                    WindowClass.lpszClassName,
                    "Hive",
                    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                    CW_USEDEFAULT,
                    CW_USEDEFAULT,
                    CW_USEDEFAULT,
                    CW_USEDEFAULT,
                    0,
                    0,
                    hInstance,
                    0
        );

        if(windowHandle)
        {
            MSG message;
            running = true;
            while(running)
            {
                bool messageResult = GetMessage(&message,0,0,0);

                if(messageResult > 0)
                {
                    //These two functions translates the message
                    //and sends it to our callback WindowProc
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            //TODO Log if Window Creation fails
        }
    }
    else
    {
        //TODO Log if Registering class fails. 
    }

   
    return 0;
}


/*
This is the main callback function for our window, when something happens with 
our window this gets called using.
The Reason we are required to have a WindowProc Callback, is because windows
reserves the right to callback to us at anytime it decides to.
*/
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch(uMsg)
    {
        case WM_SIZE:
        {
            RECT rect;
            GetClientRect(hwnd, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            ResizeDIBSection(width, height);
            OutputDebugStringA("WM_SIZE\n");
        } break;
        case WM_DESTROY:
        {
            OutputDebugStringA("WM_DESTROY\n");
            running = false; //Placeholder for now, may be an error case 
        } break;
        case WM_CLOSE:
        {

            PostQuitMessage(0);    //If we recieve the close message, post a quit message for getMessage to receive.
                                   //Parameter is possible exit return code. 
            OutputDebugStringA("WM_CLOSE\n");
            running = false;    //Placeholder for now

        } break;
        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_SIZE\n");

        } break;
        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(hwnd, &paint);
            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
            int width = paint.rcPaint.right - paint.rcPaint.left;
            int height = paint.rcPaint.bottom - paint.rcPaint.top;

            WinUpdateWindow(deviceContext, x, y, width, height);
            
            EndPaint(hwnd, &paint);

        } break;
        default:
        {
            //Let Windows handle any messages we don't want to handle.
            result = DefWindowProc(hwnd, uMsg, wParam, lParam); //Default Window Processing
        } break;
    }

    return result;
}

static void ResizeDIBSection(int width, int height)
{
    //TODO Maybe don't free first, free after. 
    //Free our dibsection
    if(bitmapHandle)
    {
        DeleteObject(bitmapHandle);
    }
    
    //if we don't have a bitmapDeviceContext, make one.
    if(!bitmapDeviceContext)
    {
        //TODO Might have to Free this in the future
        bitmapDeviceContext = CreateCompatibleDC(0);
    }
    
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;


    //TODO BulletProof This.
    
    bitmapHandle = CreateDIBSection(bitmapDeviceContext, &bitmapInfo,
                                            DIB_RGB_COLORS,  &bitmapMemory, 0,0);
}

static void WinUpdateWindow(HDC DeviceContext, int x, int y, int width, int height)
{
    StretchDIBits(DeviceContext, x, y , width, height, 
                                 x, y, width, height,
                                bitmapMemory, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}