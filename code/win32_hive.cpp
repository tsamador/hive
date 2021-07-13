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


#include <windows.h>
#include <dsound.h>
#include <math.h>   
#include <cstdio>

#include "hive_types.h"
#include "hive.cpp"

#include "win32_hive.h"


// Global variables
static bool running;  //Global for now. 
LPDIRECTSOUNDBUFFER secondarySoundBuffer; // sound buffer
static win_32_buffer backBuffer;
static int64 globalPerfCountFrequency;

static game_memory gameMemory;



/* 
    WinMain is the entry point for windows called by the C Runtime library
*/
INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR lpCmdLine, INT nCmdShow)
{
    
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    globalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    //NOTE(Tanner): Set the windows schedulre granularity to 1ms
    // So that sleep can be more granular
    UINT DesiredSchedulerMS = 1;
    bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

    //Creating our Window Class
    WNDCLASSA WindowClass = {}; 

    ResizeDIBSection(&backBuffer, 1280, 720);

    WindowClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;  
    WindowClass.lpfnWndProc = WindowProc; 
    WindowClass.hInstance = hInstance;
    WindowClass.lpszClassName = "HiveWindowClass";
    
    //TODO(Tanner): Need to have some way of detecting our monitors refresh rate
    int monitorRefreshHz = 60;
    int gameUpdateHz = monitorRefreshHz / 2;
    real32 targetSecondsPerFrame = 1.0f / (real32)gameUpdateHz;

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

        if(windowHandle)
        {
            //NOTE(Tanner): Since we specifice CS_OWNDC, we can just get one device 
            //context and us it forever, not sharing with anyone.
            HDC deviceContext = GetDC(windowHandle);

            running = true;
        
            win32_sound_output soundOutput = {};

            soundOutput.samplesPerSecond = 48000;
            soundOutput.bytesPerSample = sizeof(int16)*2;
            soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond*soundOutput.bytesPerSample;
            soundOutput.LatencySampleCount = 4 * (soundOutput.samplesPerSecond / gameUpdateHz);
            Win32InitDirectSound(windowHandle, soundOutput.secondaryBufferSize, soundOutput.samplesPerSecond);
            Win32ClearBuffer(&soundOutput);
            secondarySoundBuffer->Play(0, 0, DSBPLAY_LOOPING);

#if 0
            // NOTE(casey): This tests the PlayCursor/WriteCursor update frequency
            // On the Handmade Hero machine, it was 480 samples.
            // NOTE(Tanner): Was 480 on my machine as well.
            while(running)
            {
                DWORD PlayCursor;
                DWORD WriteCursor;
                secondarySoundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);

                char TextBuffer[256];
                _snprintf_s(TextBuffer, sizeof(TextBuffer),
                            "PC:%u WC:%u\n", PlayCursor, WriteCursor);
                OutputDebugStringA(TextBuffer);
            }
#endif
            

            int16 *Samples = (int16*) VirtualAlloc(0, soundOutput.secondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );;

            gameMemory = {};
            gameMemory.permStorageSize = Megabytes(64);
            gameMemory.permStorage = VirtualAlloc(0, gameMemory.permStorageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );

            gameMemory.tempStorageSize = Gigabytes((uint64)2);
            gameMemory.tempStorage = VirtualAlloc(0, gameMemory.tempStorageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
             

            LARGE_INTEGER lastCounter = Win32GetWallClock();

            win32_debug_time_marker debugTimeMarkers[15] = {0};
            int debugTimeMarkersIndex = 0;

            
            DWORD lastPlayCursor;
            bool32 SoundIsValid = false;

            uint64 lastCycleCount = __rdtsc();
            
            game_input_buffer gameInputs[2] = {};

            game_input_buffer *newInput = &(gameInputs[0]);
            game_input_buffer *oldInput = &gameInputs[1]; 

            while(running)
            {
                game_controller_input *oldKeyboard = GetController(oldInput, 0);
                game_controller_input *newKeyboard = GetController(newInput, 0);

                *newKeyboard = {};
                newKeyboard->isConnected = true;

                for(int buttonIndex = 0; buttonIndex < ArrayCount(newKeyboard->buttons); buttonIndex++)
                {
                    newKeyboard->buttons[buttonIndex] = oldKeyboard->buttons[buttonIndex];
                }

                Win32ProcessPendingMessages(newKeyboard);   
            
                game_sound soundBuffer = {};
                DWORD bytesToLock;
                DWORD bytesToWrite;
                DWORD targetCursor;
                DWORD playCursor;
                DWORD writeCursor;
                //TODO(Tanner): Tighten up sound logic so that we know where we should
                // be writing to and can anticipate the time spent in the game update.
                if(SoundIsValid)
                {
                    bytesToLock = (soundOutput.runningSampleIndex*soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;
                    targetCursor = (lastPlayCursor + (soundOutput.LatencySampleCount*soundOutput.bytesPerSample))
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

                    soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond ;
                    soundBuffer.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
                    soundBuffer.samples = Samples;
                }

                game_buffer buffer;
                buffer.bitmapMemory = backBuffer.bitmapMemory;
                buffer.bitmapWidth = backBuffer.bitmapWidth;
                buffer.bitmapHeight = backBuffer.bitmapHeight;
                buffer.pitch = backBuffer.pitch;

                gameUpdateAndRender(&gameMemory, &buffer, &soundBuffer, newInput);

                if(SoundIsValid)
                {
                   Win32FillSoundBuffer(&soundOutput, bytesToLock, bytesToWrite, &soundBuffer);
                }

                LARGE_INTEGER WorkCounter = Win32GetWallClock();
                real32 WorkSecondsElapsed = Win32GetSecondsElapsed(lastCounter, WorkCounter);

                // TODO(casey): NOT TESTED YET!  PROBABLY BUGGY!!!!!
                real32 SecondsElapsedForFrame = WorkSecondsElapsed;
                if (SecondsElapsedForFrame < targetSecondsPerFrame)
                {
                    if (SleepIsGranular)
                    {
                        DWORD SleepMS = (DWORD)(1000.0f * (targetSecondsPerFrame -
                                                           SecondsElapsedForFrame));
                        if (SleepMS > 0)
                        {
                            Sleep(SleepMS);
                        }
                    }

                    real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(lastCounter,
                                                                               Win32GetWallClock());
                    //Assert(TestSecondsElapsedForFrame < targetSecondsPerFrame);

                    while (SecondsElapsedForFrame < targetSecondsPerFrame)
                    {
                        SecondsElapsedForFrame = Win32GetSecondsElapsed(lastCounter,
                                                                        Win32GetWallClock());
                    }
                }
                else
                {
                    // TODO(casey): MISSED FRAME RATE!
                    // TODO(casey): Logging
                }

                LARGE_INTEGER endCounter = Win32GetWallClock();
                real32 MSPerFrame = 1000.0f * Win32GetSecondsElapsed(lastCounter, endCounter);
                lastCounter = endCounter;

                win32_window_dim dimension = Win32GetWindowDimension(windowHandle);
#if HIVE_DEBUG
                Win32DebugSyncDisplay(&backBuffer, debugTimeMarkers, ArrayCount(debugTimeMarkers) , &soundOutput, targetCursor);
#endif
                WinDisplayBufferToWindow(deviceContext,&backBuffer, dimension.width, dimension.height);

                if(secondarySoundBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK)
                {
                    lastPlayCursor = playCursor;
                    if(!SoundIsValid)
                    {
                        soundOutput.runningSampleIndex = writeCursor / soundOutput.bytesPerSample;
                        SoundIsValid = true;
                    }
                    
                }
                //NOTE(Tanner): This is debug code
#if HIVE_DEBUG
                {
                 

                    debugTimeMarkers[debugTimeMarkersIndex].playCursor = playCursor;
                    debugTimeMarkers[debugTimeMarkersIndex].writeCursor = writeCursor;
                    debugTimeMarkersIndex++;

                    if(debugTimeMarkersIndex > ArrayCount(debugTimeMarkers))
                    {
                        debugTimeMarkersIndex = 0;
                    }
                }
#endif



#if 0 
                real32 msPerFrame = ((real32)(1000*counterElapsed) / globalPerfCountFrequency);
                real32 framesPerSecond = (real32)globalPerfCountFrequency / counterElapsed;
                real32 MCPF =  (real32)cyclesElapsed /(1000 * 1000);

                char msgBuffer[256];
                sprintf(msgBuffer, "Milliseconds/frame: %.2fms  FPS: %.2f  Cycles: %.2fMhz\n", msPerFrame, framesPerSecond, MCPF);
                OutputDebugStringA(msgBuffer);    
#endif

                game_input_buffer *temp = newInput;
                newInput = oldInput;
                oldInput = temp;
                
                uint64 endCycleCount = __rdtsc();
                uint64 cyclesElapsed = endCycleCount - lastCycleCount;
                lastCycleCount = endCycleCount;

                real64 FPS = 0.0f;
                real64 MCPF = ((real64)cyclesElapsed / (1000.0f * 1000.0f));

                char FPSBuffer[256];
                _snprintf_s(FPSBuffer, sizeof(FPSBuffer),
                            "%.02fms/f,  %.02ff/s,  %.02fmc/f\n", MSPerFrame, FPS, MCPF);
                OutputDebugStringA(FPSBuffer);
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

inline win32_window_dim Win32GetWindowDimension(HWND window)
{
    win32_window_dim result = {};

    RECT rect;
    GetClientRect(window, &rect);

    result.width = rect.right - rect.left;
    result.height = rect.bottom - rect.top;

    return result;
}


inline LARGE_INTEGER Win32GetWallClock()
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;

}

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    real32 Result = ((real32)(End.QuadPart - Start.QuadPart) /
                     (real32)globalPerfCountFrequency);
    return(Result);
}

static void Win32ProcessPendingMessages(game_controller_input *keyboard)
{
    MSG message;
    while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
    {
        switch (message.message)
        {
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYUP:
            case WM_KEYDOWN:
            {
                uint32 VKCode = (uint32)message.wParam;
                bool32 wasDown = ((message.lParam & (1 << 30)) != 0);
                bool32 isDown = ((message.lParam & (1 << 31)) == 0);

                if(wasDown != isDown)
                {
                    if(VKCode == VK_UP)
                    {
                        Win32HandleKeyInput(&keyboard->moveUp, isDown);
                    }
                    else if(VKCode == VK_DOWN)
                    {
                        Win32HandleKeyInput(&keyboard->moveDown, isDown);
                    }
                    else if(VKCode == VK_LEFT)
                    {
                        Win32HandleKeyInput(&keyboard->moveLeft, isDown);
                    }
                    else if(VKCode == VK_RIGHT)
                    {
                        Win32HandleKeyInput(&keyboard->moveRight, isDown);
                    }
                    else if(VKCode == 'W')
                    {
                        Win32HandleKeyInput(&keyboard->keyW, isDown);
                    }
                    else if(VKCode == 'D')
                    {
                        Win32HandleKeyInput(&keyboard->keyD, isDown);
                    }
                    else if(VKCode == 'S')
                    {
                        Win32HandleKeyInput(&keyboard->keyS, isDown);
                    }
                    else if(VKCode == 'D')
                    {
                        Win32HandleKeyInput(&keyboard->keyD, isDown);
                    }
                    else if(VKCode == VK_SPACE)
                    {
                        Win32HandleKeyInput(&keyboard->spaceBar, isDown);
                    }
                    
                }
                
                //Check for Alt-f4
                int32 AltKeyWasDown = (message.lParam & (1 << 29));
                if (message.wParam == (VK_F4 && AltKeyWasDown))
                {
                    running = false;
                }
            }
            break;
            default:
            {
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
            break;
        }
    }
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
            
            win32_window_dim dimension = Win32GetWindowDimension(windowHandle);
            WinDisplayBufferToWindow(deviceContext, &backBuffer, dimension.width, dimension.height);
            
            EndPaint(windowHandle, &paint);

        } break;
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYUP:
        case WM_KEYDOWN:
        {
            Assert(!"Keyboard Should Not get here");
        } break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        {

        }break;
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
    Buffer->bytesPerPixel = 4;
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
    Buffer->pitch = Buffer->bitmapWidth*Buffer->bytesPerPixel;
}

static void WinDisplayBufferToWindow(HDC DeviceContext, win_32_buffer *Buffer,  int width, int height)
{
    //TODO Aspect ratio correction, right now 'squashing' our bitmap
    StretchDIBits(DeviceContext,/* x, y , width, height, 
                                 x, y, width, height, */
                                0, 0, width, height,
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

static void Win32HandleKeyInput(game_button_state *newState, bool32 isDown)
{
    Assert(newState->endedDown != isDown);
    newState->endedDown = isDown;
    ++newState->halfTransitionCount;
}



debug_read_file DEBUGPlatformReadEntireFile(char* filename)
{
    debug_read_file result = {};
    HANDLE fileHandle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0 , 0 );
    if(fileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER fileSize;
        if(GetFileSizeEx(fileHandle, &fileSize))
        {
            
            uint32 fileSize32 = SafeTruncateUInt64(fileSize.QuadPart);
            result.fileContents =  VirtualAlloc(0, fileSize.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
            if(result.fileContents)
            {
                DWORD BytesRead;
                //TODO(Tanner): Define Max Int values
                if(ReadFile(fileHandle, result.fileContents, fileSize.QuadPart, &BytesRead, 0) && (fileSize32 == BytesRead))
                {
                    //Note(Tanner): File Read Successfully
                    result.readFileSize = fileSize32;
                }  
                else
                {
                    DEBUGPlatformFreeFileMemory(result.fileContents);
                    result.fileContents = 0;
                }
            }
            else
            {
                //Note(Tanner): Unable to allocate memory
            }
        }
        CloseHandle(fileHandle);
    }
    else
    {
        DWORD errorCode = GetLastError();
        // TODO(Tanner): Logging
    }
    return result;

}

void DEBUGPlatformFreeFileMemory(void* Memory)
{
    if(Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE );
    }
}

bool DEBUGPlatformWriteEntireFile(char* filename, uint64 memorySize, void* memory)
{
    bool result = false;

    HANDLE fileHandle = CreateFileA(filename, GENERIC_WRITE, 0,0, CREATE_ALWAYS, 0,0);
    if(fileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD bytesWritten;
        if(WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0))
        {
            result = (bytesWritten == memorySize);
        }
        else
        {
            //TODO(Tanner): Logging
        }
        CloseHandle(fileHandle);
    }
    else
    {
        //TODO(Tanner): Logging
    }

    return result;
}



inline void Win32DrawSoundBufferMarker(win_32_buffer *backBuffer,
                                       win32_sound_output *soundOutput,
                                       real32 C, int padX, int top, int bottom,
                                       DWORD value, uint32 color)
{
        Assert(value < soundOutput->secondaryBufferSize);
        int X = padX + (int)(C *(real32)value);
        Win32DebugDrawVertical(backBuffer, X, top, bottom, color);
}


static void Win32DebugSyncDisplay(win_32_buffer *backBuffer,win32_debug_time_marker *markers, int markerCount, win32_sound_output *soundBuffer, real32 targetSecondsPerFrame)
{
    int padX = 16;
    int padY = 16;

    int top = padY;
    int bottom = backBuffer->bitmapHeight - padY;
    real32 C = (real32)(backBuffer->bitmapWidth - 2*padX) / (real32) soundBuffer->secondaryBufferSize;

    for(int markerIndex = 0; markerIndex < markerCount; markerIndex++ )
    {
        win32_debug_time_marker *thisMarker = &markers[markerIndex];
        Win32DrawSoundBufferMarker(backBuffer, soundBuffer, C, padX, top, bottom, thisMarker->playCursor, 0xFFFFFFFF);
        Win32DrawSoundBufferMarker(backBuffer, soundBuffer, C, padX, top, bottom, thisMarker->writeCursor, 0xFFFF0000);
        //Win32DrawSoundBufferMarker(backBuffer, thisMarker->writeCursor, markerIndex, soundBuffer, C, padX, top, bottom, 0xFFFF0000);
    }
}

static void Win32DebugDrawVertical(win_32_buffer *backBuffer, int X, int top, int bottom, uint32 color)
{
    uint8 *Pixel = ((uint8 *)backBuffer->bitmapMemory +
                    X*backBuffer->bytesPerPixel +
                    top*backBuffer->pitch);
    for(int Y = top;
        Y < bottom;
        ++Y)
    {
        *(uint32 *)Pixel = color;
        Pixel += backBuffer->pitch;
    }
} 