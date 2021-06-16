
#include "hive.h"
#include "hive_types.h"

static void RenderGradient(game_buffer *Buffer, int XOffset, int YOffset)
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


void gameUpdateAndRender(game_buffer * buffer)
{
    RenderGradient(buffer, 0, 0);
}