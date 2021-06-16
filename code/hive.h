/*

*/



struct game_buffer {  
    
    void *bitmapMemory;  
    int bitmapWidth;  
    int bitmapHeight;  
    int bytesPerPixel;   
    int pitch;  
};  

// This is the function the Platform layer will use to call into 
void gameUpdateAndRender(game_buffer* buffer);