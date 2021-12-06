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
#define MonitorRefreshHz 60
#define GameUpdateHz (MonitorRefreshHz / 2)
    real32 targetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

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
            soundOutput.LatencySampleCount = 2 * (soundOutput.samplesPerSecond / GameUpdateHz);
            soundOutput.safetyBytes = (soundOutput.samplesPerSecond*soundOutput.bytesPerSample / GameUpdateHz) / 3;

            Win32InitDirectSound(windowHandle, soundOutput.secondaryBufferSize, soundOutput.samplesPerSecond);
            Win32ClearBuffer(&soundOutput);
            secondarySoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
            
            int16 *Samples = (int16*) VirtualAlloc(0, soundOutput.secondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );;

            gameMemory = {};
            gameMemory.permStorageSize = Megabytes(64);
            gameMemory.permStorage = VirtualAlloc(0, gameMemory.permStorageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );

            gameMemory.tempStorageSize = Gigabytes((uint64)2);
            gameMemory.tempStorage = VirtualAlloc(0, gameMemory.tempStorageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
             

            LARGE_INTEGER lastCounter = Win32GetWallClock();
            LARGE_INTEGER flipWallClock = Win32GetWallClock();

            win32_debug_time_marker DebugTimeMarkers[15] = {0};
            int DebugTimeMarkersIndex = 0;

            
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
            
                game_buffer buffer;
                buffer.bitmapMemory = backBuffer.bitmapMemory;
                buffer.bitmapWidth = backBuffer.bitmapWidth;
                buffer.bitmapHeight = backBuffer.bitmapHeight;
                buffer.pitch = backBuffer.pitch;

                gameUpdateAndRender(&gameMemory, &buffer, newInput);

                LARGE_INTEGER AudioWallClock = Win32GetWallClock();
                real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(flipWallClock, AudioWallClock);

                DWORD PlayCursor;
                DWORD WriteCursor;
                if (secondarySoundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                {
                    /* NOTE(Tanner):

                               Here is how sound output computation works.

                               We define a safety value that is the number
                               of samples we think our game update loop
                               may vary by (let's say up to 2ms)
                       
                               When we wake up to write audio, we will look
                               and see what the play cursor position is and we
                               will forecast ahead where we think the play
                               cursor will be on the next frame boundary.

                               We will then look to see if the write cursor is
                               before that by at least our safety value.  If
                               it is, the target fill position is that frame
                               boundary plus one frame.  This gives us perfect
                               audio sync in the case of a card that has low
                               enough latency.

                               If the write cursor is _after_ that safety
                               margin, then we assume we can never sync the
                               audio perfectly, so we will write one frame's
                               worth of audio plus the safety margin's worth
                               of guard samples.
                            */
                    if (!SoundIsValid)
                    {
                        soundOutput.runningSampleIndex = WriteCursor / soundOutput.bytesPerSample;
                        SoundIsValid = true;
                    }

                    DWORD ByteToLock = ((soundOutput.runningSampleIndex * soundOutput.bytesPerSample) %
                                        soundOutput.secondaryBufferSize);

                    DWORD ExpectedSoundBytesPerFrame =
                        (soundOutput.samplesPerSecond * soundOutput.bytesPerSample) / GameUpdateHz;
                    real32 SecondsLeftUntilFlip = (targetSecondsPerFrame - FromBeginToAudioSeconds);
                    DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip / targetSecondsPerFrame) * (real32)ExpectedSoundBytesPerFrame);

                    DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedSoundBytesPerFrame;

                    DWORD SafeWriteCursor = WriteCursor;
                    if (SafeWriteCursor < PlayCursor)
                    {
                        SafeWriteCursor += soundOutput.secondaryBufferSize;
                    }
                    Assert(SafeWriteCursor >= PlayCursor);
                    SafeWriteCursor += soundOutput.safetyBytes;

                    bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);

                    DWORD TargetCursor = 0;
                    if (AudioCardIsLowLatency)
                    {
                        TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
                    }
                    else
                    {
                        TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame +
                                        soundOutput.safetyBytes);
                    }
                    TargetCursor = (TargetCursor % soundOutput.secondaryBufferSize);

                    DWORD BytesToWrite = 0;
                    if (ByteToLock > TargetCursor)
                    {
                        BytesToWrite = (soundOutput.secondaryBufferSize - ByteToLock);
                        BytesToWrite += TargetCursor;
                    }
                    else
                    {
                        BytesToWrite = TargetCursor - ByteToLock;
                    }

                    game_sound SoundBuffer = {};
                    SoundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
                    SoundBuffer.sampleCount = BytesToWrite / soundOutput.bytesPerSample;
                    SoundBuffer.samples = Samples;
                    GameGetSoundSamples(&gameMemory, &SoundBuffer);

#if HANDMADE_INTERNAL
                    win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
                    Marker->OutputPlayCursor = PlayCursor;
                    Marker->OutputWriteCursor = WriteCursor;
                    Marker->OutputLocation = ByteToLock;
                    Marker->OutputByteCount = BytesToWrite;
                    Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;

                    DWORD UnwrappedWriteCursor = WriteCursor;
                    if (UnwrappedWriteCursor < PlayCursor)
                    {
                        UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
                    }
                    AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
                    AudioLatencySeconds =
                        (((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) /
                         (real32)SoundOutput.SamplesPerSecond);

                    char TextBuffer[256];
                    _snprintf_s(TextBuffer, sizeof(TextBuffer),
                                "BTL:%u TC:%u BTW:%u - PC:%u WC:%u DELTA:%u (%fs)\n",
                                ByteToLock, TargetCursor, BytesToWrite,
                                PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
                    OutputDebugStringA(TextBuffer);
#endif
                    Win32FillSoundBuffer(&soundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                }
                else
                {
                    SoundIsValid = false;
                }

                LARGE_INTEGER WorkCounter = Win32GetWallClock();
                real32 WorkSecondsElapsed = Win32GetSecondsElapsed(lastCounter, WorkCounter);

                // TODO(Tanner): NOT TESTED YET!  PROBABLY BUGGY!!!!!
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
                    // TODO(Tanner): MISSED FRAME RATE!
                    // TODO(Tanner): Logging
                }

                LARGE_INTEGER endCounter = Win32GetWallClock();
                real32 MSPerFrame = 1000.0f * Win32GetSecondsElapsed(lastCounter, endCounter);
                lastCounter = endCounter;

                win32_window_dim dimension = Win32GetWindowDimension(windowHandle);
#if HIVE_DEBUG
                Win32DebugSyncDisplay(&backBuffer, ArrayCount(DebugTimeMarkers), DebugTimeMarkers,
                                              DebugTimeMarkersIndex - 1, &soundOutput, targetSecondsPerFrame);
#endif
                WinDisplayBufferToWindow(deviceContext,&backBuffer, dimension.width, dimension.height);

                flipWallClock = Win32GetWallClock();

                //NOTE(Tanner): This is debug code
#if HIVE_DEBUG
                {
                    DWORD PlayCursor;
                    DWORD WriteCursor;
                    if (secondarySoundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                    {
                        Assert(DebugTimeMarkersIndex < ArrayCount(DebugTimeMarkers));
                        win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkersIndex];
                        Marker->FlipPlayCursor = PlayCursor;
                        Marker->FlipWriteCursor = WriteCursor;
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
#if HIVE_DEBUG
                DebugTimeMarkersIndex++;
                if(DebugTimeMarkersIndex == ArrayCount(DebugTimeMarkers))
                {
                    DebugTimeMarkersIndex = 0;
                }
#endif
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

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    real32 Result = ((real32)(End.QuadPart - Start.QuadPart) /
                     (real32)globalPerfCountFrequency);
    return Result;
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

            LPDIRECTSOUNDBUFFER PrimaryBuffer;
            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
            {
                HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
                if (SUCCEEDED(Error))
                {
                    OutputDebugStringA("Primary buffer format was set.\n");
                }
                else
                {
                    // TODO(Tanner): Diagnostic
                }
            }
            else
            {
                // TODO(Tanner): Diagnostic
            }
        }
        else
        {
            // TODO(Tanner): Diagnostic
        }

        // TODO(Tanner): DSBCAPS_GETCURRENTPOSITION2
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
        // TODO(Tanner): Diagnostic
    }
}


static void Win32FillSoundBuffer(win32_sound_output* soundOutput, DWORD byteToLock, DWORD bytesToWrite, game_sound* sourceBuffer)
{
     // TODO(Tanner): More strenuous test!
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(SUCCEEDED(secondarySoundBuffer->Lock(byteToLock, bytesToWrite,
                                             &Region1, &Region1Size,
                                             &Region2, &Region2Size,
                                             0)))
    {
        // TODO(Tanner): Collapse these two loops
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
    real32 XReal32 = (C * (real32)value);
    int X = padX + (int)XReal32;
    Win32DebugDrawVertical(backBuffer, X, top, bottom, color);
}

static void Win32DebugSyncDisplay(win_32_buffer *Backbuffer, int MarkerCount, win32_debug_time_marker *Markers, int CurrentMarkerIndex,win32_sound_output *SoundOutput, real32 TargetSecondsPerFrame)
{
    int PadX = 16;
    int PadY = 16;

    int LineHeight = 64;
    
    real32 C = (real32)(Backbuffer->bitmapWidth - 2*PadX) / (real32)SoundOutput->secondaryBufferSize;
    for(int MarkerIndex = 0;
        MarkerIndex < MarkerCount;
        ++MarkerIndex)
    {
        win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
        Assert(ThisMarker->OutputPlayCursor < SoundOutput->secondaryBufferSize);
        Assert(ThisMarker->OutputWriteCursor < SoundOutput->secondaryBufferSize);
        Assert(ThisMarker->OutputLocation < SoundOutput->secondaryBufferSize);
        Assert(ThisMarker->OutputByteCount < SoundOutput->secondaryBufferSize);
        Assert(ThisMarker->FlipPlayCursor < SoundOutput->secondaryBufferSize);
        Assert(ThisMarker->FlipWriteCursor < SoundOutput->secondaryBufferSize);

        DWORD PlayColor = 0xFFFFFFFF;
        DWORD WriteColor = 0xFFFF0000;
        DWORD ExpectedFlipColor = 0xFFFFFF00;
        DWORD PlayWindowColor = 0xFFFF00FF;

        int Top = PadY;
        int Bottom = PadY + LineHeight;
        if(MarkerIndex == CurrentMarkerIndex)
        {
            Top += LineHeight+PadY;
            Bottom += LineHeight+PadY;

            int FirstTop = Top;
            
            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputPlayCursor, PlayColor);
            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputWriteCursor, WriteColor);

            Top += LineHeight+PadY;
            Bottom += LineHeight+PadY;

            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation, PlayColor);
            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);

            Top += LineHeight+PadY;
            Bottom += LineHeight+PadY;

            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, FirstTop, Bottom, ThisMarker->ExpectedFlipPlayCursor, ExpectedFlipColor);
        }        
        
        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor, PlayColor);
        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + 480*SoundOutput->bytesPerSample, PlayWindowColor);
        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteColor);
    }
}

static void Win32DebugDrawVertical(win_32_buffer *backBuffer, int X, int top, int bottom, uint32 color)
{
    if(top <= 0)
    {
        top = 0;
    }

    if(bottom > backBuffer->bitmapHeight)
    {
        bottom = backBuffer->bitmapHeight;
    }

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