#ifndef WIN32HIVE_H
#define WIN32HIVE_H

#include <windows.h>
#include <dsound.h>
#include <math.h>   //Will eventually replace these
#include <cstdio>

#include "hive_types.h"

struct win_32_buffer {  
    BITMAPINFO bitmapInfo;  
    void *bitmapMemory;  
    int bitmapWidth;  
    int bitmapHeight;   
    int pitch;  
    int bytesPerPixel;
};  

struct win32_sound_output {
    int samplesPerSecond;
    uint32 runningSampleIndex ;
    int bytesPerSample;
    int secondaryBufferSize;
    int LatencySampleCount;
    DWORD safetyBytes;
};

struct win32_window_dim {
    int width;
    int height;
};

struct win32_debug_time_marker
{
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputLocation;
    DWORD OutputByteCount;
    DWORD ExpectedFlipPlayCursor;
    DWORD FlipPlayCursor;
    DWORD FlipWriteCursor;
};


LRESULT CALLBACK WindowProc(HWND windowHandle, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void ResizeDIBSection(win_32_buffer *Buffer, int width, int height);
static void WinDisplayBufferToWindow(HDC DeviceContext, win_32_buffer *Buffer,  int width, int height);
static void Win32InitDirectSound(HWND window, int32 bufferSize, int32 samplePerSecond);
static void Win32FillSoundBuffer(win32_sound_output* soundOutput, DWORD bytesToLock, DWORD bytesToWrite, game_sound* sourceBuffer);
static void Win32ClearBuffer(win32_sound_output* soundOutput);
static void Win32HandleKeyInput(game_input_buffer* gameInputs,WPARAM keycode, LPARAM prevState);
static void Win32ProcessPendingMessages(game_controller_input *newKeyboard);
static void Win32HandleKeyInput(game_button_state *newState, bool32 isDown);
inline LARGE_INTEGER Win32GetWallClock();
inline real32 Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end);
inline win32_window_dim Win32GetWindowDimension(HWND window);
static void Win32DebugSyncDisplay(win_32_buffer *Backbuffer, int MarkerCount, win32_debug_time_marker *Markers, int CurrentMarkerIndex,win32_sound_output *SoundOutput, real32 TargetSecondsPerFrame);
static void Win32DebugDrawVertical(win_32_buffer *backBuffer, int X, int top, int bottom, uint32 color);

#endif