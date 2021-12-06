

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
    MemoryBlock* head;

};


