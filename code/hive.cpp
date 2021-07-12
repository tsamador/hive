
#include <math.h> // Eventually Implement our own Sine function 
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

static void gameOutputSound(game_sound *SoundBuffer, int toneHz)
{
    static real32 tSine;
    int16 toneVolume = 3000;
    int wavePeriod = SoundBuffer->samplesPerSecond/toneHz;

    int16 *SampleOut = SoundBuffer->samples;
    for (int SampleIndex = 0;
         SampleIndex < SoundBuffer->sampleCount;
         ++SampleIndex)
    {
        real32 SineValue = sinf(tSine);
        int16 SampleValue = (int16)(SineValue * toneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        tSine += 2.0f * Pi32 * 1.0f / (real32)wavePeriod;
    }
}
/*
    TODO(Tanner) Will probably want to turn this into an event based system.
*/
static void gameHandleInput(game_input_buffer* gameInputs, int &xOffset, int &yOffset, int &toneHz)
{
    for(int ControllerIndex = 0; ControllerIndex < ArrayCount(gameInputs->Controllers); ControllerIndex++)
    {
        game_controller_input *controller = GetController(gameInputs, ControllerIndex);

        if(controller->moveLeft.endedDown)
        {
            xOffset -= 1;
        }

        if(controller->moveRight.endedDown)
        {
            xOffset += 1;
        }

        if(controller->moveUp.endedDown)
        {
            yOffset += 1;
        }

        if(controller->moveDown.endedDown)
        {
            yOffset -= 1;
        }

        if(controller->keyW.endedDown)
        {
            toneHz += 10;
        }

        if(controller->keyS.endedDown)
        {
            toneHz -= 10;
        }

        if(controller->keyD.endedDown)
        {
            yOffset -= 1;
        }
        if(controller->keyA.endedDown)
        {
            yOffset -= 1;
        }
        if(controller->spaceBar.endedDown)
        {
            yOffset = 0;
            xOffset = 0;
            toneHz = 512;
        }
    }
}

static void gameUpdateAndRender(game_memory* gameMemory, game_buffer* buffer, game_sound* soundBuffer, game_input_buffer* gameInputs)
{
    Assert(sizeof(game_state) <= gameMemory->permStorageSize);
    
    //TODO(Tanner): Will need a more sophisticated divide of memory here
    static game_state* gameState = (game_state*) gameMemory->permStorage;

    //TODO(Tanner): Probably want a better init check here.
    if(!gameMemory->isInitialized)
    {   
        
        //char* filename = "C:\\work\\hive\\data\\test";
        char* filename = __FILE__;
        
        debug_read_file file = DEBUGPlatformReadEntireFile(filename);

        if(file.fileContents)
        {
            DEBUGPlatformWriteEntireFile("test.out", file.readFileSize, file.fileContents);
            DEBUGPlatformFreeFileMemory(file.fileContents);
        }
    
        gameState->toneHz = 512;

        gameMemory->isInitialized = true;
    }

    //TODO(Tanner): Allow sample offsets here for more robust platform options
    gameHandleInput(gameInputs, gameState->xOffset, gameState->yOffset, gameState->toneHz);
    gameOutputSound(soundBuffer, gameState->toneHz);
    RenderGradient(buffer, gameState->xOffset, gameState->yOffset);
}