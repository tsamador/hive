#ifndef HIVE_H
#define HIVE_H

/*

*/

struct game_buffer {  
    
    void *bitmapMemory;  
    int bitmapWidth;  
    int bitmapHeight;  
    int bytesPerPixel;   
    int pitch;  
};  

struct game_sound {
    int sampleCount;
    int samplesPerSecond;
    int16 *samples;

};



// This is the function the Platform layer will use to call into 
static void gameUpdateAndRender(game_buffer* buffer, game_sound* soundBuffer);
static void RenderGradient(game_buffer *Buffer, int XOffset, int YOffset);
static void gameOutputSound(game_sound *SoundBuffer);

#endif