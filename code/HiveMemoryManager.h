

/*
    TODO(Tanner): Probably want to turn this into a singleton at some point.
*/

struct MemoryBlock
{
    union data{
        int i;
        float f;
        char c;
    };
    MemoryBlock* next;
};

struct HiveMemoryManager 
{
    //Instance variables
    MemoryBlock* head;
    int numBlocks;

    //functions
    MemoryBlock* GetMemoryBlock();
    void ReleaseMemoryBlock(MemoryBlock* item);

};




