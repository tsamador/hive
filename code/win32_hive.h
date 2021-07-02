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
};  

struct win32_sound_output {
    int samplesPerSecond;
    uint32 runningSampleIndex ;
    int bytesPerSample;
    int secondaryBufferSize;
    int LatencySampleCount;
};


LRESULT CALLBACK WindowProc(HWND windowHandle, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void ResizeDIBSection(win_32_buffer *Buffer, int width, int height);
static void WinDisplayBufferToWindow(HDC DeviceContext, RECT WindowRect, win_32_buffer *Buffer, int x, int y, int width, int height);
static void Win32InitDirectSound(HWND window, int32 bufferSize, int32 samplePerSecond);
static void Win32FillSoundBuffer(win32_sound_output* soundOutput, DWORD bytesToLock, DWORD bytesToWrite, game_sound* sourceBuffer);
static void Win32ClearBuffer(win32_sound_output* soundOutput);
static void Win32HandleKeyInput(game_input_buffer gameInputs,WPARAM keycode, LPARAM prevState);

#endif