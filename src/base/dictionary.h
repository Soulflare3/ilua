#ifndef __BASE_DICTIONARY_H__
#define __BASE_DICTIONARY_H__

#include "base/types.h"
#include "base/string.h"
#include "base/pool.h"


class DictionaryMap
{
public:
  static uint8 const* alNum;
  static uint8 const* alNumNoCase;
  static uint8 const* alNumSpace;
  static uint8 const* pathName;
  static uint8 const* asciiNoCase;
};

class DictionaryBase
{
protected:
  MemoryPool pool;
  enum {HashSize = 8};
  struct Node
  {
    Node* parent;
    Node* next;
    uint8* str;
    Node* c[HashSize];
    uint8 data[1];
  };
  uint8 const* map;
  virtual Node* newnode(int len) = NULL;
  virtual void delnode(Node* n) = NULL;
  Node* baseadd(uint8 const* key);
  Node* basefind(uint8 const* key) const;
  
  Node* root;
public:
  DictionaryBase(uint8 const* map);

  uint32 enumStart() const;
  uint32 enumNext(uint32 cur) const;
  String enumGetKey(uint32 cur) const;

  void clear();
};

template<class T>
class Dictionary : public DictionaryBase
{
  Node* newnode(int len)
  {
    uint32 size = sizeof(Node) + sizeof(T) + len + 1;
    Node* n = (Node*) pool.alloc(size);
    memset(n, 0, size);
    n->str = ((uint8*) (n + 1)) + sizeof(T);
    return n;
  }
  void delnode(Node* n)
  {
    for (int i = 0; i < HashSize; i++)
    {
      while (n->c[i])
      {
        Node* next = n->c[i]->next;
        delnode(n->c[i]);
        n->c[i] = next;
      }
    }
    if (n->data[0])
      ((T*) (n->data + 1))->~T();
  }
  T* nil;

public:
  Dictionary(uint8 const* map = NULL)
    : DictionaryBase(map)
  {
    root = newnode(0);
    nil = (T*) malloc(sizeof(T));
    memset(nil, 0, sizeof(T));
    new(nil) T;
  }
  ~Dictionary()
  {
    delnode(root);
    nil->~T();
    free(nil);
  }
  void set(char const* key, T const& value)
  {
    Node* n = baseadd((uint8 const*) key);
    if (!n->data[0])
    {
      n->data[0] = 1;
      new(n->data + 1) T(value);
    }
    else
      *((T*) (n->data + 1)) = value;
  }
  T& create(char const* key)
  {
    Node* n = baseadd((uint8 const*) key);
    if (!n->data[0])
    {
      n->data[0] = 1;
      new(n->data + 1) T;
    }
    return *((T*) (n->data + 1));
  }
  T const& get(char const* key) const
  {
    Node* n = basefind((uint8 const*) key);
    return (n && n->data[0]) ? *((T*) (n->data + 1)) : *nil;
  }
  T& get (char const* key)
  {
    Node* n = basefind((uint8 const*) key);
    return (n && n->data[0]) ? *((T*) (n->data + 1)) : *nil;
  }
  T const* getptr (char const* key) const
  {
    Node* n = basefind((uint8 const*) key);
    return (n && n->data[0] ? ((T*) (n->data + 1)) : NULL);
  }
  T* getptr (char const* key)
  {
    Node* n = basefind((uint8 const*) key);
    return (n && n->data[0] ? ((T*) (n->data + 1)) : NULL);
  }
  bool has(char const* key) const
  {
    Node* n = basefind((uint8 const*) key);
    return (n && n->data[0]);
  }
  void del(char const* key)
  {
    Node* n = basefind((uint8 const*) key);
    if (n && n->data[0])
    {
      ((T*) (n->data + 1))->~T();
      n->data[0] = NULL;
    }
  }

  T const& enumGetValue(uint32 cur) const
  {
    Node* n = (Node*) cur;
    return *((T*) (n->data + 1));
  }
  T& enumGetValue(uint32 cur)
  {
    Node* n = (Node*) cur;
    return *((T*) (n->data + 1));
  }
  void enumSetValue(uint32 cur, T const& value)
  {
    Node* n = (Node*) cur;
    *((T*) (n->data + 1)) = value;
  }
};

class SimpleDictionary
{
  struct Node
  {
    String key;
    uint32 value;
  };
  Node* nodes;
  int numNodes;
  int maxNodes;
  int locate (String key) const;
public:
  SimpleDictionary ();
  ~SimpleDictionary ();

  uint32 set (String key, uint32 value);
  uint32 get (String key) const;
  bool has (String key) const;
  uint32 del (String key);

  uint32 enumStart () const;
  uint32 enumNext (uint32 cur) const;
  String enumGetKey (uint32 cur) const;
  uint32 enumGetValue (uint32 cur) const;
  uint32 enumSetValue (uint32 cur, uint32 value);

  void clear ();
};

#endif // __BASE_DICTIONARY_H__
