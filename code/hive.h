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

struct game_input_buffer {
    keyboard_input key_input;   //Eventually this should be able to hold an arbitrary number of inputs, currently just a single one
    //mouse_input mouse_input;
};



// This is the function the Platform layer will use to call into 
static void gameUpdateAndRender(game_buffer *buffer, game_sound* soundBuffer, game_input_buffer* inputs);
static void RenderGradient(game_buffer *Buffer, int XOffset, int YOffset);
static void gameOutputSound(game_sound *SoundBuffer);

#endif