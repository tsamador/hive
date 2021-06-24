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
    int toneHz;
    int samplesPerSecond;
    int16 ToneVolume;
    uint32 runningSampleIndex ;
    int wavePeriod;
    int bytesPerSample;
    int secondaryBufferSize;
    real32 tSine;
    int LatencySampleCount;
};


LRESULT CALLBACK WindowProc(HWND windowHandle, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void ResizeDIBSection(win_32_buffer *Buffer, int width, int height);
static void WinDisplayBufferToWindow(HDC DeviceContext, RECT WindowRect, win_32_buffer *Buffer, int x, int y, int width, int height);
static void RenderGradient(win_32_buffer *Buffer,int XOffset, int YOffset);
static void Win32InitDirectSound(HWND window, int32 bufferSize, int32 samplePerSecond);
static void Win32FillSoundBuffer(win32_sound_output* soundOutput, DWORD bytesToLock, DWORD bytesToWrite, game_sound* sourceBuffer);
static void Win32ClearBuffer(win32_sound_output* soundOutput);

