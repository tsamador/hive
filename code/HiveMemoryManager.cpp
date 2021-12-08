#include "HiveMemoryManager.h"
#include <stdio.h>

// NOTE(Tanner): Maybe pass in a type here, set it as an instance variable of a memory block???
// That's wasting a little bit of storage but may be a good solution for other problems.
Memoryblock* HiveMemoryManager::GetMemoryBlock()
{
   MemoryBlock* temp = _head;
   _head = _head->next;

   return temp;

}

void HiveMemoryManager::ReleaseMemoryBlock(MemoryBlock* temp)
{  
   temp->next = _head;
   _head = temp;
}

void HiveMemoryManager::HiveMemoryManager()
{
   printf("Size of the memory node struct: %d", (sizeof)MemoryBlock );
}


//The size of this class should be divisible by the 
//Node size;
void HiveMemoryManager::Init(int size)
{
   //Alloc
}