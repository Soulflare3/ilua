#include "pool.h"
#include <stdlib.h>

struct FixedMemoryPool::MemoryChunk
{
  MemoryChunk* next;
  MemoryChunk* nextFree;

  uint8* begin;
  uint8* end;
  uint8* firstFree;
  uint8* firstUnused;
  uint32 itemSize;

  MemoryChunk(uint32 size, uint32 count);
  ~MemoryChunk();

  bool hasSpace()
  {
    return firstFree || firstUnused < end;
  }
  bool contains(uint8* ptr)
  {
    return ptr >= begin && ptr < end;
  }
  uint8* alloc();
  void free(uint8* ptr);
};

FixedMemoryPool::MemoryChunk::MemoryChunk(uint32 size, uint32 count)
{
  next = NULL;
  nextFree = NULL;

  itemSize = size;

  begin = new uint8[size * count];
  end = begin + size * count;
  firstFree = NULL;
  firstUnused = begin;
}
FixedMemoryPool::MemoryChunk::~MemoryChunk()
{
  delete[] begin;
}

uint8* FixedMemoryPool::MemoryChunk::alloc()
{
  if (firstFree)
  {
    uint8* result = firstFree;
    firstFree = *(uint8**) firstFree;
    return result;
  }
  else if (firstUnused < end)
  {
    uint8* result = firstUnused;
    firstUnused += itemSize;
    return result;
  }
  return NULL;
}
void FixedMemoryPool::MemoryChunk::free(uint8* ptr)
{
  *(uint8**) ptr = firstFree;
  firstFree = ptr;
}

FixedMemoryPool::FixedMemoryPool(uint32 itemSize, uint32 poolGrow)
{
  this->itemSize = itemSize;
  chunkSize = poolGrow / itemSize;
  chunks = NULL;
  freeChunks = NULL;
}
FixedMemoryPool::~FixedMemoryPool()
{
  clear();
}

void* FixedMemoryPool::alloc()
{
  if (freeChunks == NULL)
  {
    freeChunks = new MemoryChunk(itemSize, chunkSize);
    freeChunks->next = chunks;
    chunks = freeChunks;
  }
  void* ptr = freeChunks->alloc();
  if (!freeChunks->hasSpace())
    freeChunks = freeChunks->nextFree;
  return ptr;
}
void FixedMemoryPool::free(void* ptr)
{
  for (MemoryChunk* chunk = chunks; chunk; chunk = chunk->next)
  {
    if (chunk->contains((uint8*) ptr))
    {
      if (!chunk->hasSpace())
      {
        chunk->nextFree = freeChunks;
        freeChunks = chunk;
      }
      chunk->free((uint8*) ptr);
      return;
    }
  }
}
void FixedMemoryPool::clear()
{
  while (chunks)
  {
    MemoryChunk* next = chunks->next;
    delete chunks;
    chunks = next;
  }
  freeChunks = NULL;
}

//////////////////////////////////////////////

struct MemoryPool::MemoryChunk
{
  uint8* ptr;
  uint32 left;
  MemoryChunk* next;
  uint8* alloc(uint32 size)
  {
    if (size <= left)
    {
      uint8* result = ptr;
      ptr += size;
      left -= size;
      return result;
    }
    return NULL;
  }
};

MemoryPool::MemoryPool(uint32 chunk)
{
  chunkSize = chunk;
  chunks = NULL;
}
MemoryPool::~MemoryPool()
{
  while (chunks)
  {
    MemoryChunk* next = chunks->next;
    free(chunks);
    chunks = next;
  }
}

void* MemoryPool::alloc(uint32 size)
{
  void* result = NULL;
  if (chunks)
    result = chunks->alloc(size);
  if (result == NULL)
  {
    MemoryChunk* chunk = (MemoryChunk*) malloc(sizeof(MemoryChunk) + chunkSize);
    chunk->ptr = (uint8*)(chunk + 1);
    chunk->left = chunkSize;
    chunk->next = chunks;
    chunks = chunk;
    result = chunks->alloc(size);
  }
  return result;
}
void MemoryPool::clear()
{
  while (chunks)
  {
    MemoryChunk* next = chunks->next;
    free(chunks);
    chunks = next;
  }
}
