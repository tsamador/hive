
#include <math.h>
#include "hive.h"
#include "hive_types.h"

static void RenderGradient(game_buffer *Buffer, int XOffset, int YOffset)
{

    uint8 *row = (uint8 *)Buffer->bitmapMemory;
    Buffer->pitch = 4 * Buffer->bitmapWidth;
    for (int Y = 0; Y < Buffer->bitmapHeight; Y++)
    {
        uint32 *pixel = (uint32 *)row;
        for (int X = 0; X < Buffer->bitmapWidth; X++)
        {
            /*
                Note:            0  1  2  3                                
                Pixel in memory: 00 00 00 00
                                 BB GG RR xx
            */
            uint8 blue = (uint8)X + XOffset;
            uint8 green = (uint8)Y + YOffset;
            uint8 red = 0;

            *pixel = (uint32)blue | (uint8)green << 8 | (uint8)red;
            pixel++;
        }
        row += Buffer->pitch;
    }
}

static void gameOutputSound(game_sound *SoundBuffer)
{
    static real32 tSine;
    int16 toneVolume = 3000;
    int ToneHz = 256;
    int wavePeriod = SoundBuffer->samplesPerSecond/ToneHz;

    int16 *SampleOut = SoundBuffer->samples;
    for (int SampleIndex = 0;
         SampleIndex < SoundBuffer->sampleCount;
         ++SampleIndex)
    {
        // TODO(casey): Draw this out for people
        real32 SineValue = sinf(tSine);
        int16 SampleValue = (int16)(SineValue * toneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        tSine += 2.0f * Pi32 * 1.0f / (real32)wavePeriod;
    }
}

void gameUpdateAndRender(game_buffer *buffer, game_sound* soundBuffer)
{
    //TODO(Tanner): Allow sample offsets here for more robust platform options
    gameOutputSound(soundBuffer);
    RenderGradient(buffer, 0, 0);
}