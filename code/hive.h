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


// NOTE() Not to be included in final build
struct debug_read_file {
    uint64 readFileSize;
    void* fileContents;
};

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

    bool isInitialized;

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

//Services the platform provides to the game
debug_read_file DEBUGPlatformReadEntireFile(char* filename);
void DEBUGPlatformFreeFileMemory(void* Memory);

bool DEBUGPlatformWriteEntireFile(char* filename, uint64 memorySize, void* memory);

inline uint32
SafeTruncateUInt64(uint64 Value)
{
    // TODO(casey): Defines for maximum values
    Assert(Value <= 0xFFFFFFFF);
    uint32 Result = (uint32)Value;
    return(Result);
}


// This is the function the Platform layer will use to call into 
static void gameUpdateAndRender(game_buffer *buffer, game_sound* soundBuffer, std::vector<keyboard_input>* gameInputs);
static void RenderGradient(game_buffer *Buffer, int XOffset, int YOffset);
static void gameOutputSound(game_sound *SoundBuffer);

#endif