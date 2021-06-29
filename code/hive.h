#ifndef HIVE_H
#define HIVE_H

/*

*/


#if HIVE_DEBUG
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression) 
#endif

#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) (Kilobytes(Value) * 1024)
#define Gigabytes(Value) (Megabytes(Value) * 1024)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

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

struct game_memory {
    uint64 permStorageSize;
    void* permStorage;

    uint64 tempStorageSize;
    void* tempStorage;  //NOTE(Tanner): Required to be set to 0.
};

//TODO(Tanner) This will change
struct game_state {
    int xOffset;
    int yOffset;
    int toneHz;
};


// This is the function the Platform layer will use to call into 
static void gameUpdateAndRender(game_buffer *buffer, game_sound* soundBuffer, game_input_buffer* inputs);
static void RenderGradient(game_buffer *Buffer, int XOffset, int YOffset);
static void gameOutputSound(game_sound *SoundBuffer);

#endif