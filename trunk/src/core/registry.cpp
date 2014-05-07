#include <windows.h>
#include "base/file.h"
#include "base/string.h"
#include "base/utils.h"

#include "registry.h"

Config cfg;

Config::Config()
{
  File* file = File::wopen(WideString::buildFullName(getAppPath(), L"config.cfg"), File::READ);
  if (file)
  {
    uint32 count = file->read32();
    for (uint32 i = 0; i < count; i++)
    {
      String name = file->readString();
      uint32 size = file->read32();
      if (!items.has(name))
      {
        ItemData& id = items.create(name);
        id.size = size;
        id.ptr = new uint8[size];
        file->read(id.ptr, id.size);
      }
      else
        file->seek(size, SEEK_CUR);
    }
    delete file;
  }
}
Config::~Config()
{
  File* file = File::wopen(WideString::buildFullName(getAppPath(), L"config.cfg"), File::REWRITE);
  if (file)
  {
    uint32 count = 0;
    file->write32(count);
    for (uint32 cur = items.enumStart(); cur; cur = items.enumNext(cur))
    {
      file->writeString(items.enumGetKey(cur));
      ItemData& id = items.enumGetValue(cur);
      file->write32(id.size);
      file->write(id.ptr, id.size);
      count++;
    }
    file->seek(0, SEEK_SET);
    file->write32(count);
    delete file;
  }
}
void* Config::getitem(char const* name, int size, bool create)
{
  if (items.has(name))
  {
    ItemData& id = items.get(name);
    if (id.size >= size)
      return id.ptr;
    else if (create)
    {
      delete[] id.ptr;
      id.ptr = new uint8[size];
      id.size = size;
      return id.ptr;
    }
    else
      return NULL;
  }
  else if (create)
  {
    ItemData& id = items.create(name);
    id.size = size;
    id.ptr = new uint8[size];
    return id.ptr;
  }
  else
    return NULL;
}
