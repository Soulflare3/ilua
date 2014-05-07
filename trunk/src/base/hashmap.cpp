#include "hashmap.h"
#include <string.h>

static void* DELETED = (void*) 0xFFFFFFFF;

HashMapBase::HashMapBase(uint32 size, uint32 tsize)
  : pool(size)
  , itemCount(0)
  , tableSize(tsize)
  , newItemCount(0)
  , newTableSize(0)
  , copied(0)
{
  table = new void*[tableSize];
  memset(table, 0, sizeof(void*) * tableSize);
  newTable = NULL;
}
HashMapBase::~HashMapBase()
{
  delete[] table;
  delete[] newTable;
}

void* HashMapBase::basefind(void const* key) const
{
  uint32 hv = hash(key) & (tableSize - 1);
  while (table[hv])
  {
    if (table[hv] != DELETED && equal(table[hv], key))
      return table[hv];
    hv = (hv + 1) & (tableSize - 1);
  }
  return NULL;
}
void* HashMapBase::baseadd(void const* key, bool* created)
{
  uint32 hv = hash(key) & (tableSize - 1);
  while (table[hv] && table[hv] != DELETED)
  {
    if (equal(table[hv], key))
    {
      *created = false;
      return table[hv];
    }
    hv = (hv + 1) & (tableSize - 1);
  }
  *created = true;
  void* result = pool.alloc();
  if (newTable == NULL && itemCount >= tableSize / 2)
  {
    newTableSize = tableSize * 2;
    newTable = new void*[newTableSize];
    memset(newTable, 0, sizeof(void*) * newTableSize);
    newItemCount = 0;
    copied = 0;
  }
  if (newTable)
  {
    for (uint32 nc = 0; nc < 6 && copied < tableSize; nc++, copied++)
    {
      if (table[copied] && table[copied] != DELETED)
      {
        uint32 nhv = hash(table[copied]) & (newTableSize - 1);
        while (newTable[nhv] && newTable[nhv] != DELETED)
          nhv = (nhv + 1) & (newTableSize - 1);
        newItemCount++;
        newTable[nhv] = table[copied];
      }
    }
    if (hv < copied)
    {
      uint32 nhv = hash(key) & (newTableSize - 1);
      while (newTable[nhv] && newTable[nhv] != DELETED)
        nhv = (nhv + 1) & (newTableSize - 1);
      newItemCount++;
      newTable[nhv] = result;
    }
    if (copied == tableSize)
    {
      delete[] table;
      table = newTable;
      tableSize = newTableSize;
      itemCount = newItemCount;
      newTable = NULL;
    }
    else
    {
      table[hv] = result;
      itemCount++;
    }
  }
  else
  {
    table[hv] = result;
    itemCount++;
  }
  return result;
}
void* HashMapBase::basedel(void const* key)
{
  uint32 hv = hash(key) & (tableSize - 1);
  while (table[hv])
  {
    if (table[hv] != DELETED && equal(table[hv], key))
    {
      void* result = table[hv];
      table[hv] = DELETED;
      if (newTable && hv < copied)
      {
        uint32 nhv = hash(key) & (newTableSize - 1);
        while (newTable[nhv])
        {
          if (newTable[nhv] != DELETED && equal(newTable[nhv], key))
          {
            newTable[nhv] = DELETED;
            break;
          }
          nhv = (nhv + 1) & (newTableSize - 1);
        }
      }
      return result;
    }
    hv = (hv + 1) & (tableSize - 1);
  }
  return NULL;
}
uint32 HashMapBase::baseenum(uint32 pos) const
{
  while (pos < tableSize)
    if (table[pos++] && table[pos - 1] != DELETED)
      return pos;
  return 0;
}
