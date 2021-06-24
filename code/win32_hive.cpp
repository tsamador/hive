/*
    Author: Tanner Amador
    Date: 5/28/2021

    Note: using ANSI Encoding: must use Windows function with 'A' Suffix
    EX: WNDCLASSA instead WNDCLASS
    Technically this does not give an error if using Microsofts Macros,
    but Visual Studio Code will flag many of them as an error, which
    makes me sad, so specify WNDCLASS A.
*/


/*
    TODO(Tanner): THIS IS NOT A FINAL PLATFORM LAYER!!!

    - Saved game locations
    - Getting handle to our own exe file
    - Asset loading path
    - Threading (Launch a thread)
    - Raw Input
    - Sleep/TimeBeginPeriod
    - ClipCursor (Multimonitor support)
    - FullScreen support

*/

#include "hive_types.h"
#include "hive.cpp"

#include <windows.h>
#include <dsound.h>
#include <math.h>   
#include <cstdio>
#include "win32_hive.h"

// Global variables
static bool running;  //Global for now. 
LPDIRECTSOUNDBUFFER secondarySoundBuffer; // sound buffer
static win_32_buffer backBuffer;


/* 
    WinMain is the entry point for windows called by the C Runtime library
*/
INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR lpCmdLine, INT nCmdShow)
{
    
    LARGE_INTEGER cyclesPerSecondResult;
    QueryPerformanceFrequency(&cyclesPerSecondResult);
    int64 cyclesPerSecond = cyclesPerSecondResult.QuadPart;

    //Creating our Window Class
    WNDCLASSA WindowClass = {}; //Look up this class on MSDN on repeat. 

    ResizeDIBSection(&backBuffer, 1280, 720);

    WindowClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;  
    WindowClass.lpfnWndProc = WindowProc; 
    WindowClass.hInstance = hInstance;
    WindowClass.lpszClassName = "HiveWindowClass";

    //Now register the window class
    if(RegisterClassA(&WindowClass))   
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

        if(windowHandle)
        {
            MSG message;
            running = true;
            
            win32_sound_output soundOutput = {};

            soundOutput.toneHz = 256;
            soundOutput.samplesPerSecond = 48000;
            soundOutput.ToneVolume = 3000;
            soundOutput.wavePeriod = soundOutput.samplesPerSecond/soundOutput.toneHz;
            soundOutput.bytesPerSample = sizeof(int16)*2;
            soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond*soundOutput.bytesPerSample;
            soundOutput.LatencySampleCount = soundOutput.samplesPerSecond / 15;
            Win32InitDirectSound(windowHandle, soundOutput.secondaryBufferSize, soundOutput.samplesPerSecond);
            Win32ClearBuffer(&soundOutput);
            secondarySoundBuffer->Play(0, 0, DSBPLAY_LOOPING);


            int16 *Samples = (int16*) VirtualAlloc(0, soundOutput.secondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );;

            LARGE_INTEGER lastCounter;
            QueryPerformanceCounter(&lastCounter);

            uint64 lastCycleCount = __rdtsc();
            
            while(running)
            {
                
                while(PeekMessage(&message,0,0,0, PM_REMOVE))
                {
                    //These two functions translates the message 
                    //and sends it to our callback WindowProc
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }

                DWORD bytesToLock;
                DWORD bytesToWrite;
                DWORD targetCursor;
                DWORD playCursor;
                DWORD writeCursor;
                bool SoundIsValid = false;
                //TODO(Tanner): Tighten up sound logic so that we know where we should
                // be writing to and can anticipate the time spent in the game update.
                if(secondarySoundBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK)
                {
                    bytesToLock = (soundOutput.runningSampleIndex*soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;
                    targetCursor = (playCursor + (soundOutput.LatencySampleCount*soundOutput.bytesPerSample))
                                            % soundOutput.secondaryBufferSize;
                    if(bytesToLock > targetCursor)
                    {
                        bytesToWrite = (soundOutput.secondaryBufferSize - bytesToLock);
                        bytesToWrite += targetCursor;
                    }
                    else
                    {
                        bytesToWrite = targetCursor - bytesToLock;
                    }
                    SoundIsValid = true;
                }

                
                game_sound soundBuffer = {};
                soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond ;
                soundBuffer.sampleCount = bytesToWrite/ soundOutput.bytesPerSample;
                soundBuffer.samples = Samples;
                
                game_buffer buffer;
                buffer.bitmapMemory = backBuffer.bitmapMemory;
                buffer.bitmapWidth = backBuffer.bitmapWidth;
                buffer.bitmapHeight = backBuffer.bitmapHeight;
                buffer.pitch = backBuffer.pitch;
                gameUpdateAndRender(&buffer, &soundBuffer);

                if(SoundIsValid)
                {
                   Win32FillSoundBuffer(&soundOutput, bytesToLock, bytesToWrite, &soundBuffer);
                }

                HDC deviceContext = GetDC(windowHandle);
                
                RECT rect;
                GetClientRect(windowHandle, &rect);

                int windowWidth = rect.right - rect.left;
                int windowHeight = rect.bottom - rect.top;
                WinDisplayBufferToWindow(deviceContext, rect, &backBuffer, 0, 0, windowWidth, windowHeight);
                ReleaseDC(windowHandle, deviceContext);

                uint64 endCycleCount = __rdtsc();

                LARGE_INTEGER endCounter;
                QueryPerformanceCounter(&endCounter);

                //TODO: Display the value here
                uint64 cyclesElapsed = endCycleCount - lastCycleCount;
                int64 timeTaken = (endCounter.QuadPart - lastCounter.QuadPart);
                real32 msPerFrame = ((real32)(1000*timeTaken) / cyclesPerSecond);
                real32 framesPerSecond = (real32)cyclesPerSecond / timeTaken;
                real32 MCPF =  (real32)cyclesElapsed /(1000 * 1000);

                char msgBuffer[256];
                sprintf(msgBuffer, "Milliseconds/frame: %.2fms  FPS: %.2f  Cycles: %.2fMhz\n", msPerFrame, framesPerSecond, MCPF);
                OutputDebugStringA(msgBuffer);    

                lastCounter = endCounter;
                lastCycleCount = endCycleCount;
                
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
    int bitmapMemorySize = (4*Buffer->bitmapWidth*Buffer->bitmapHeight);
    /*
        We could use malloc, but malloc goes through the C Runtime library to
        be platform independent. On windows, Malloc would eventually just
        call VirtualAlloc anyways, so we just use forog the misdirection
        and call VirtualAlloc ourselves. 
    */
    Buffer->bitmapMemory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE );
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




static void Win32InitDirectSound(HWND window, int32 bufferSize, int32 samplePerSecond)
{
    // TODO(casey): Double-check that this works on XP - DirectSound8 or 7??
    LPDIRECTSOUND DirectSound;
    if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
    {
        WAVEFORMATEX WaveFormat = {};
        WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
        WaveFormat.nChannels = 2;
        WaveFormat.nSamplesPerSec = samplePerSecond;
        WaveFormat.wBitsPerSample = 16;
        WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
        WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
        WaveFormat.cbSize = 0;

        if (SUCCEEDED(DirectSound->SetCooperativeLevel(window, DSSCL_PRIORITY)))
        {
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

            // NOTE(casey): "Create" a primary buffer
            // TODO(casey): DSBCAPS_GLOBALFOCUS?
            LPDIRECTSOUNDBUFFER PrimaryBuffer;
            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
            {
                HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
                if (SUCCEEDED(Error))
                {
                    // NOTE(casey): We have finally set the format!
                    OutputDebugStringA("Primary buffer format was set.\n");
                }
                else
                {
                    // TODO(casey): Diagnostic
                }
            }
            else
            {
                // TODO(casey): Diagnostic
            }
        }
        else
        {
            // TODO(casey): Diagnostic
        }

        // TODO(casey): DSBCAPS_GETCURRENTPOSITION2
        DSBUFFERDESC BufferDescription = {};
        BufferDescription.dwSize = sizeof(BufferDescription);
        BufferDescription.dwFlags = 0;
        BufferDescription.dwBufferBytes = bufferSize;
        BufferDescription.lpwfxFormat = &WaveFormat;
        HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &secondarySoundBuffer, 0);
        if (SUCCEEDED(Error))
        {
            OutputDebugStringA("Secondary buffer created successfully.\n");
        }
    }
    else
    {
        // TODO(casey): Diagnostic
    }
}


static void Win32FillSoundBuffer(win32_sound_output* soundOutput, DWORD byteToLock, DWORD bytesToWrite, game_sound* sourceBuffer)
{
     // TODO(casey): More strenuous test!
    // TODO(casey): Switch to a sine wave
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(SUCCEEDED(secondarySoundBuffer->Lock(byteToLock, bytesToWrite,
                                             &Region1, &Region1Size,
                                             &Region2, &Region2Size,
                                             0)))
    {
        // TODO(casey): Collapse these two loops
        DWORD Region1SampleCount = Region1Size/soundOutput->bytesPerSample;
        int16 *DestSample = (int16 *)Region1;
        int16 *SourceSample = sourceBuffer->samples;
        for(DWORD SampleIndex = 0;
            SampleIndex < Region1SampleCount;
            ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++soundOutput->runningSampleIndex;
        } 

        DWORD Region2SampleCount = Region2Size/soundOutput->bytesPerSample;
        DestSample= (int16 *)Region2;
        for(DWORD SampleIndex = 0;
            SampleIndex < Region2SampleCount;
            ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++soundOutput->runningSampleIndex;
        }
        secondarySoundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
    
}

static void Win32ClearBuffer(win32_sound_output* soundOutput)
{
     // TODO(casey): More strenuous test!
    // TODO(casey): Switch to a sine wave
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(SUCCEEDED(secondarySoundBuffer->Lock(0,soundOutput->secondaryBufferSize,
                                             &Region1, &Region1Size,
                                             &Region2, &Region2Size,
                                             0)))
    {
        uint8 *DestSample = (uint8 *)Region1;
        for(DWORD byteIndex = 0;
            byteIndex < Region1Size;
            ++byteIndex)
        {
            *DestSample++ = 0;
        }

        DestSample = (uint8 *)Region2;
        for(DWORD byteIndex = 0;
            byteIndex < Region2Size;
            ++byteIndex)
        {
            *DestSample++ = 0;
        }

        secondarySoundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}