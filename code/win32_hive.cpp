/*
    Author: Tanner Amador
    Date: 5/28/2021

    Note: using ANSI Encoding: must use Windows function with 'A' Suffix
    EX: WNDCLASSA instead WNDCLASS
    Technically this does not give an error if using Microsofts Macros,
    but Visual Studio Code will flag many of them as an error, which
    makes me sad, so specify WNDCLASS A.
*/

#include <windows.h>
#include <stdint.h>


typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

//Function Prototypes
LRESULT CALLBACK WindowProc(HWND windowHandle, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void ResizeDIBSection(int width, int height);
static void WinUpdateWindow(HDC DeviceContext, RECT WindowRect, int x, int y, int width, int height);
static void RenderGradient(int XOffset, int YOffset);


//Global variables
static bool running;  //Global for now. 
static BITMAPINFO bitmapInfo;
static void *bitmapMemory;
static int bitmapWidth;
static int bitmapHeight;

/* 
    WinMain is the entry point for windows called by the C Runtime library
*/
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

        int XOffset = 0;
        int YOffset = 0;
        if(windowHandle)
        {
            MSG message;
            running = true;
            while(running)
            {
                while(PeekMessage(&message,0,0,0, PM_REMOVE))
                {
                    //These two functions translates the message
                    //and sends it to our callback WindowProc
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }
                RenderGradient(XOffset++, YOffset++);
                HDC deviceContext = GetDC(windowHandle);
                
                RECT rect;
                GetClientRect(windowHandle, &rect);

                int windowWidth = rect.right - rect.left;
                int windowHeight = rect.bottom - rect.top;
                WinUpdateWindow(deviceContext, rect, 0, 0, windowWidth, windowHeight);
                ReleaseDC(windowHandle, deviceContext);
            
                
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
LRESULT CALLBACK WindowProc(HWND windowHandle, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch(uMsg)
    {
        case WM_SIZE:
        {
            RECT rect;
            GetClientRect(windowHandle, &rect);
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
            HDC deviceContext = BeginPaint(windowHandle, &paint);
            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
            int width = paint.rcPaint.right - paint.rcPaint.left;
            int height = paint.rcPaint.bottom - paint.rcPaint.top;

            RECT rect;
            GetClientRect(windowHandle, &rect);
            
            WinUpdateWindow(deviceContext, rect, x, y, width, height);
            
            EndPaint(windowHandle, &paint);

        } break;
        default:
        {
            //Let Windows handle any messages we don't want to handle.
            result = DefWindowProc(windowHandle, uMsg, wParam, lParam); //Default Window Processing
        } break;
    }

    return result;
}


/*
    This function creates the bitmap we need to display, called everytime the 
    window is resized. 
*/
static void ResizeDIBSection(int width, int height)
{
    if(bitmapMemory)
    {
        VirtualFree(bitmapMemory, 0, MEM_RELEASE);
    }

    bitmapWidth = width;
    bitmapHeight = height;
    //Create a bitmap info struct for StretchDIBits
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = bitmapWidth;
    bitmapInfo.bmiHeader.biHeight = -bitmapHeight;  //Negative to indicate a topdown window.
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    //Allocate a bitmap for our resized DIB Section
    int bytesPerPixel = 4;
    int bitmapMemorySize = (4*bitmapWidth*bitmapHeight);
    /*
        We could use malloc, but malloc goes through the C Runtime library to
        be platform independent. On windows, Malloc would eventually just
        call VirtualAlloc anyways, so we just use forog the misdirection
        and call VirtualAlloc ourselves. 
    */
    bitmapMemory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE );
    RenderGradient(0,0);
}


/*
*/
static void WinUpdateWindow(HDC DeviceContext, RECT WindowRect,  int x, int y, int width, int height)
{

    /*
        Pitch - Value 
    */
    int windowWidth = WindowRect.right - WindowRect.bottom;
    int windowHeight = WindowRect.bottom - WindowRect.top;

    StretchDIBits(DeviceContext,/* x, y , width, height, 
                                 x, y, width, height, */
                                0, 0, bitmapWidth, bitmapHeight,
                                0, 0, windowWidth, windowHeight,
                                bitmapMemory, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}


static void RenderGradient(int XOffset, int YOffset)
{
    
    uint8 *row = (uint8*)bitmapMemory;
    int pitch = 4*bitmapWidth;
    for(int Y = 0; Y < bitmapHeight; Y++)
    {
        uint32 *pixel = (uint32*)row;
        for(int X = 0; X < bitmapWidth; X++)
        {
            /*
                Note:            0  1  2  3                                
                Pixel in memory: 00 00 00 00
                                 BB GG RR xx
            */
            uint8 blue = (uint8)X + XOffset ;
            uint8 green = (uint8)Y + YOffset;
            uint8 red = 0;

            *pixel = (uint32) blue  | (uint8) green << 8 | (uint8)red;
            pixel++;
        }
        row += pitch;
    }
}

static void paintWindow()
{

}