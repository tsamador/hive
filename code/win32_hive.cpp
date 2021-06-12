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
#include <dsound.h>
#include <math.h>   //Will eventually replace these

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;
//Global variables
static bool running;  //Global for now. 
LPDIRECTSOUNDBUFFER secondarySoundBuffer; // sound buffer


struct win_32_buffer {
    BITMAPINFO bitmapInfo;
    void *bitmapMemory;
    int bitmapWidth;
    int bitmapHeight;
    int bytesPerPixel; 
    int pitch;
};

//Function Prototypes
LRESULT CALLBACK WindowProc(HWND windowHandle, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void ResizeDIBSection(win_32_buffer *Buffer, int width, int height);
static void WinDisplayBufferToWindow(HDC DeviceContext, RECT WindowRect, win_32_buffer *Buffer, int x, int y, int width, int height);
static void RenderGradient(win_32_buffer *Buffer,int XOffset, int YOffset);
static void Win32InitDirectSound(HWND window, int32 bufferSize, int32 samplePerSecond);


static win_32_buffer backBuffer;
/* 
    WinMain is the entry point for windows called by the C Runtime library
*/
INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR lpCmdLine, INT nCmdShow)
{
    
    //Creating our Window Class
    WNDCLASSA WindowClass = {}; //Look up this class on MSDN on repeat. 


    ResizeDIBSection(&backBuffer, 1280, 720);

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

        //Graphics Test
        int XOffset = 0;
        int YOffset = 0;

        //Sound test
        int toneHz = 256;
        int samplesPerSecond = 48000;
        int16 ToneVolume = 500;
        uint32 runningSampleIndex = 0;
        int wavePeriod = samplesPerSecond/toneHz;
        //int halfWavePeriod = wavePeriod/2;
        int bytesPerSample = sizeof(int16) * 2;
        int secondaryBufferSize = samplesPerSecond*bytesPerSample;
        bool soundIsPlaying = false;

        if(windowHandle)
        {

            Win32InitDirectSound(windowHandle, secondaryBufferSize, samplesPerSecond);
            

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

                

                RenderGradient(&backBuffer, XOffset++, YOffset++);


                // DirectSound output test
                DWORD playCursor;
                DWORD writeCursor;
                if(secondarySoundBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK)
                {

                    DWORD bytesToLock = runningSampleIndex*bytesPerSample % secondaryBufferSize;
                    DWORD bytesToWrite;
                    //TODO(Tanner): We need a more accurate check than ByteToLock == PlayCursor
                    if(bytesToLock == playCursor) 
                    {
                        bytesToWrite = secondaryBufferSize;
                    }
                    else if(bytesToLock > playCursor)
                    {
                        bytesToWrite = (secondaryBufferSize - bytesToLock);
                        bytesToWrite += playCursor;
                    }
                    else
                    {
                        bytesToWrite = playCursor - bytesToLock;
                    }
                    
                    VOID* regionOne;
                    DWORD regionOneSize;
                    VOID* regionTwo;
                    DWORD regionTwoSize;

                    
                    if(secondarySoundBuffer->Lock( bytesToLock, bytesToWrite, &regionOne, &regionOneSize, &regionTwo, &regionTwoSize, 0) == DS_OK)
                    {

                        int16* sampleOut = (int16*)regionOne;
                        DWORD regionOneSampleCount = regionOneSize/bytesPerSample;
                        

                        /* DAY 009 - 45 Minutes in */
                        for(DWORD SampleIndex = 0; SampleIndex < regionOneSampleCount; SampleIndex++)
                        {
                            real32 t = (real32)runningSampleIndex / (real32)wavePeriod;
                            real32 SineValue = sinf(t);
                            int16 sampleValue = (int16)(SineValue * ToneVolume);
                            *sampleOut++ = sampleValue;
                            *sampleOut++ = sampleValue;
                            runningSampleIndex++;
                        }
                        DWORD regionTwoSampleCount = regionTwoSize/bytesPerSample;
                        sampleOut = (int16*)regionTwo;
                        for(DWORD SampleIndex = 0; SampleIndex < regionTwoSampleCount; SampleIndex++)
                        {
                         
                            *sampleOut++ = sampleValue;
                            *sampleOut++ = sampleValue;
                            runningSampleIndex++;
                        }
                    }

                    secondarySoundBuffer->Unlock(&regionOne, regionOneSize, &regionTwo, regionTwoSize);
                }

                if(!soundIsPlaying)
                {
                    secondarySoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
                    soundIsPlaying = true;
                }


                HDC deviceContext = GetDC(windowHandle);
                
                RECT rect;
                GetClientRect(windowHandle, &rect);

                int windowWidth = rect.right - rect.left;
                int windowHeight = rect.bottom - rect.top;
                WinDisplayBufferToWindow(deviceContext, rect, &backBuffer, 0, 0, windowWidth, windowHeight);
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
            
            WinDisplayBufferToWindow(deviceContext, rect, &backBuffer, x, y, width, height);
            
            EndPaint(windowHandle, &paint);

        } break;
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYUP:
        case WM_KEYDOWN:
        {

            if(wParam == VK_SPACE)
            {
                OutputDebugStringA("Space pressed\n");
            }

            //Check for Alt-f4
            int32 AltKeyWasDown = (lParam & (1 << 29));
            if(wParam == VK_F4 && AltKeyWasDown)
            {
                running = false;
            }
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
static void ResizeDIBSection(win_32_buffer *Buffer, int width, int height)
{
    if(Buffer->bitmapMemory)
    {
        VirtualFree(Buffer->bitmapMemory, 0, MEM_RELEASE);
    }

    Buffer->bitmapWidth = width;
    Buffer->bitmapHeight = height;
    //Create a bitmap info struct for StretchDIBits
    Buffer->bitmapInfo.bmiHeader.biSize = sizeof(Buffer->bitmapInfo.bmiHeader);
    Buffer->bitmapInfo.bmiHeader.biWidth = Buffer->bitmapWidth;
    Buffer->bitmapInfo.bmiHeader.biHeight = -Buffer->bitmapHeight;  //Negative to indicate a topdown window.
    Buffer->bitmapInfo.bmiHeader.biPlanes = 1;
    Buffer->bitmapInfo.bmiHeader.biBitCount = 32;
    Buffer->bitmapInfo.bmiHeader.biCompression = BI_RGB;

    //Allocate a bitmap for our resized DIB Section
    Buffer->bytesPerPixel = 4;
    int bitmapMemorySize = (4*Buffer->bitmapWidth*Buffer->bitmapHeight);
    /*
        We could use malloc, but malloc goes through the C Runtime library to
        be platform independent. On windows, Malloc would eventually just
        call VirtualAlloc anyways, so we just use forog the misdirection
        and call VirtualAlloc ourselves. 
    */
    Buffer->bitmapMemory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE );
    RenderGradient(Buffer, 0,0);
}


/*
*/
static void WinDisplayBufferToWindow(HDC DeviceContext, RECT WindowRect, win_32_buffer *Buffer, int x, int y, int width, int height)
{

    /*
        Pitch - Value 
    */
    int windowWidth = WindowRect.right - WindowRect.left;
    int windowHeight = WindowRect.bottom - WindowRect.top;

    //TODO Aspect ratio correction, right now 'squashing' our bitmap

    StretchDIBits(DeviceContext,/* x, y , width, height, 
                                 x, y, width, height, */
                                0, 0, windowWidth, windowHeight,
                                0, 0, Buffer->bitmapWidth, Buffer->bitmapHeight,
                                Buffer->bitmapMemory, &(Buffer->bitmapInfo), DIB_RGB_COLORS, SRCCOPY);
}


static void RenderGradient(win_32_buffer *Buffer,int XOffset, int YOffset)
{
    
    uint8 *row = (uint8*)Buffer->bitmapMemory;
    Buffer->pitch = 4*Buffer->bitmapWidth;
    for(int Y = 0; Y < Buffer->bitmapHeight; Y++)
    {
        uint32 *pixel = (uint32*)row;
        for(int X = 0; X < Buffer->bitmapWidth; X++)
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
        row += Buffer->pitch;
    }
}


static void Win32InitDirectSound(HWND window, int32 bufferSize, int32 samplePerSecond)
{
    //Load the library 
    HMODULE dSoundLibrary = LoadLibraryA("dsound.dll");
    
    if(dSoundLibrary)
    {
        LPDIRECTSOUND DirectSound;
        if(DirectSoundCreate(0, &DirectSound,0) == DS_OK)
        {
            DirectSound->SetCooperativeLevel(window, DSSCL_PRIORITY);

            //Create the Primary Buffer

            DSBUFFERDESC bufferDesc = {};
            bufferDesc.dwSize = sizeof(DSBUFFERDESC);
            bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER; //TODO(Tanner) May eventually need to change this flags
            bufferDesc.dwBufferBytes = 0;
            bufferDesc.dwReserved = 0;
            bufferDesc.lpwfxFormat = 0;
            bufferDesc.guid3DAlgorithm = DS3DALG_DEFAULT;

            LPDIRECTSOUNDBUFFER primarySoundBuffer;
            if(DirectSound->CreateSoundBuffer(&bufferDesc,&primarySoundBuffer,0) == DS_OK)
            {
                WAVEFORMATEX waveFormat = {};
                waveFormat.wFormatTag = WAVE_FORMAT_PCM;
                waveFormat.nChannels = 2;
                waveFormat.nSamplesPerSec = samplePerSecond;
                waveFormat.wBitsPerSample = 16;
                waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8 ;
                waveFormat.nAvgBytesPerSec = (waveFormat.nBlockAlign * waveFormat.nSamplesPerSec);
                waveFormat.cbSize = 0;

                if(primarySoundBuffer->SetFormat(&waveFormat) == DS_OK)
                {

                }
                else
                {
                    //TODO Diagnostic logging
                }
            }
            else
            {
                //TODO Diagnostic logging 
            }

            //Create the Secondary Buffer
            DSBUFFERDESC bufferDescSec = {};
            bufferDescSec.dwSize = sizeof(DSBUFFERDESC);
            bufferDescSec.dwFlags = 0; //TODO May need different flags in the future
            bufferDescSec.dwBufferBytes = bufferSize;
            bufferDescSec.dwReserved = 0;
            bufferDescSec.guid3DAlgorithm = DS3DALG_DEFAULT;
            
            WAVEFORMATEX waveFormat = {};
            waveFormat.wFormatTag = WAVE_FORMAT_PCM;
            waveFormat.nChannels = 2;
            waveFormat.nSamplesPerSec = samplePerSecond;
            waveFormat.wBitsPerSample = 16;
            waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8 ;
            waveFormat.nAvgBytesPerSec = (waveFormat.nBlockAlign * waveFormat.nSamplesPerSec);
            waveFormat.cbSize = 0;

            bufferDescSec.lpwfxFormat = &waveFormat;

            
            if(DirectSound->CreateSoundBuffer(&bufferDescSec,&secondarySoundBuffer,0) == DS_OK)
            {

            }
            else
            {
                //TODO Diagnostic Logging - Secondary buffer creation failed
            }
        }
        else
        {
            //TODO Logging
        }
    }

    return ;
}






/*
                This is XInput code, may need to use it eventually inside 
                our main loop but right now think we will only be handling
                mouse/keyboard input.
                //NOTE: Should we poll more frequently
                for(DWORD i = 0; i < XUSER_MAX_COUNT; i++)
                {
                    XINPUT_STATE controllerState;
                    if(XInputGetState(i, &controllerState) == ERROR_SUCCESS) // if the controller exists
                    {
                        //NOTE: This controller is plugged in
                        //TODO: See if packet number increments to rapidly
                        XINPUT_GAMEPAD* pad = &controllerState.Gamepad;

                        pad->wButtons

                    }
                    else
                    {
                        //NOTE: The Controller is not available
                    }
                }
*/