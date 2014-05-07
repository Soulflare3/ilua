#ifndef __BASE_HASHMAP__
#define __BASE_HASHMAP__

#include "base/types.h"
#include "base/checksum.h"
#include "base/pool.h"

#include "base/string.h"
#include "base/wstring.h"
#include "base/array.h"
#include "base/pair.h"

template<class T>
class Hash
{
public:
  static uint32 hash(T const& value, uint32 initval = 0)
  {
    return hashlittle(&value, sizeof value, initval);
  }
  static int compare(T const& a, T const& b)
  {
    return memcmp(&a, &b, sizeof a);
  }
};
template<>
class Hash<String>
{
public:
  static uint32 hash(String const& value, uint32 initval = 0)
  {
    return hashlittle(value.c_str(), value.length(), initval);
  }
  static int compare(String const& a, String const& b)
  {
    return a.compare(b);
  }
};
template<>
class Hash<WideString>
{
public:
  static uint32 hash(WideString const& value, uint32 initval = 0)
  {
    return hashlittle(value.c_str(), value.length() * 2, initval);
  }
  static int compare(WideString const& a, WideString const& b)
  {
    return a.compare(b);
  }
};
template<class T>
class Hash<Array<T> >
{
public:
  static uint32 hash(Array<T> const& value, uint32 initval = 0)
  {
    for (int i = 0; i < value.length(); i++)
      initval = Hash<T>::hash(value[i], initval);
    return initval;
  }
  static int compare(Array<T> const& a, Array<T> const& b)
  {
    for (int i = 0; i < a.length() && i < b.length(); i++)
      if (int cmp = Hash<T>::compare(a[i], b[i]))
        return cmp;
    return a.length() - b.length();
  }
};
template<class A, class B>
class Hash<Pair<A, B> >
{
public:
  static uint32 hash(Pair<A, B> const& value, uint32 initval = 0)
  {
    initval = Hash<A>::hash(value.first, initval);
    return Hash<B>::hash(value.second, initval);
  }
  static int compare(Pair<A, B> const& a, Pair<A, B> const& b)
  {
    if (int cmp = Hash<A>::compare(a.first, b.first))
      return cmp;
    return Hash<B>::compare(a.second, b.second);
  }
};

class HashMapBase
{
protected:
  FixedMemoryPool pool;
  uint32 itemCount;
  uint32 tableSize;
  uint32 newItemCount;
  uint32 newTableSize;
  uint32 copied;
  void** table;
  void** newTable;

  virtual uint32 hash(void const* key) const = 0;
  virtual bool equal(void const* key1, void const* key2) const = 0;

  HashMapBase(uint32 size, uint32 tsize);
  ~HashMapBase();

  void* basefind(void const* key) const;
  void* baseadd(void const* key, bool* created);
  void* basedel(void const* key);
  uint32 baseenum(uint32 pos) const;
};

template<class Key, class Value, class HashFunc = Hash<Key> >
class HashMap : private HashMapBase
{
  struct KeyValue
  {
    Key key;
    Value value;
    KeyValue()
    {}
    KeyValue(Key const& k)
      : key(k)
    {}
    KeyValue(Key const& k, Value const& v)
      : key(k)
      , value(v)
    {}
  };
  uint32 hash(void const* key) const
  {
    return HashFunc::hash(*(Key*) key);
  }
  bool equal(void const* key1, void const* key2) const
  {
    return HashFunc::compare(*(Key*) key1, *(Key*) key2) == 0;
  }
public:
  HashMap(uint32 size = 32)
    : HashMapBase(sizeof(KeyValue), size)
  {}
  ~HashMap()
  {
    for (uint32 i = 0; i < tableSize; i++)
      if (table[i])
        ((KeyValue*) table[i])->~KeyValue();
  }

  bool has(Key const& key) const
  {
    return basefind(&key) != 0;
  }
  Value const& get(Key const& key) const
  {
    return ((KeyValue*) basefind(&key))->value;
  }
  Value& get(Key const& key)
  {
    return ((KeyValue*) basefind(&key))->value;
  }
  Value ptrget(Key const& key) const
  {
    KeyValue* kv = (KeyValue*) basefind(&key);
    return (kv ? kv->value : NULL);
  }
  void set(Key const& key, Value const& value)
  {
    bool created;
    KeyValue* ptr = (KeyValue*) baseadd(&key, &created);
    if (created)
      new(ptr) KeyValue(key, value);
    else
      ptr->value = value;
  }
  Value& create(Key const& key)
  {
    bool created;
    KeyValue* ptr = (KeyValue*) baseadd(&key, &created);
    if (created)
      new(ptr) KeyValue(key);
    return ptr->value;
  }
  void del(Key const& key)
  {
    void* ptr = basedel(&key);
    if (ptr)
    {
      ((KeyValue*) ptr)->~KeyValue();
      pool.free(ptr);
    }
  }
  void clear()
  {
    for (uint32 i = 0; i < tableSize; i++)
    {
      if (table[i] && table[i] != (void*) 0xFFFFFFFF)
      {
        ((KeyValue*) table[i])->~KeyValue();
        pool.free(table[i]);
      }
      table[i] = NULL;
    }
    delete[] newTable;
    newTable = NULL;
    itemCount = 0;
  }

  uint32 enumStart() const
  {
    return baseenum(0);
  }
  uint32 enumNext(uint32 cur) const
  {
    return baseenum(cur);
  }
  Key const& enumGetKey(uint32 cur) const
  {
    return ((KeyValue*) table[cur - 1])->key;
  }
  Value const& enumGetValue(uint32 cur) const
  {
    return ((KeyValue*) table[cur - 1])->value;
  }
  Value& enumGetValue(uint32 cur)
  {
    return ((KeyValue*) table[cur - 1])->value;
  }
  void enumSetValue(uint32 cur, Value const& value)
  {
    ((KeyValue*) table[cur - 1])->value = value;
  }
};

#endif // __BASE_HASHMAP__
