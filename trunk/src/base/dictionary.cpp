#include "dictionary.h"
#include <locale>

static uint8 _mapAlNum[256];
uint8 const* DictionaryMap::alNum = _mapAlNum;
static uint8 _mapAlNumNoCase[256];
uint8 const* DictionaryMap::alNumNoCase = _mapAlNumNoCase;
static uint8 _mapAlNumSpace[256];
uint8 const* DictionaryMap::alNumSpace = _mapAlNumSpace;
static uint8 _mapPath[256];
uint8 const* DictionaryMap::pathName = _mapPath;
static uint8 _mapAsciiNoCase[256];
uint8 const* DictionaryMap::asciiNoCase = _mapAsciiNoCase;
static bool _mapInit = false;

DictionaryBase::DictionaryBase(uint8 const* _map)
{
  if (!_mapInit)
  {
    memset(_mapAlNum, 0, sizeof _mapAlNum);
    memset(_mapAlNumNoCase, 0, sizeof _mapAlNumNoCase);
    memset(_mapAlNumSpace, 0, sizeof _mapAlNumSpace);
    memset(_mapPath, 0, sizeof _mapPath);
    memset(_mapAsciiNoCase, 0, sizeof _mapAsciiNoCase);
    for (int i = 0; i < 10; i++)
    {
      uint8 c = uint8('0' + i);
      _mapAlNum[c] = _mapAlNumNoCase[c] = _mapAlNumSpace[c] = c;
    }
    for (int i = 0; i < 26; i++)
    {
      uint8 c = uint8('a' + i);
      uint8 C = uint8('A' + i);
      _mapAlNum[c] = _mapAlNumNoCase[c] = _mapAlNumSpace[c] =
        _mapAlNumNoCase[C] = _mapAlNumSpace[C] = c;
      _mapAlNum[C] = C;
    }
    for (int i = 0; i < 256; i++)
    {
      if (i == '/')
        _mapPath[i] = '\\';
      else
        _mapPath[i] = s_tolower(i);
    }
    for (int i = 32; i < 128; i++)
      _mapAsciiNoCase[i] = i;
    for (int i = 'a'; i < 'z'; i++)
      _mapAsciiNoCase[i] = i - 'a' + 'A';
    _mapInit = true;
  }
  map = _map;
}
void DictionaryBase::clear()
{
  delnode(root);
  pool.clear();
  root = newnode(0);
}

DictionaryBase::Node* DictionaryBase::baseadd(uint8 const* key)
{
  Node* cur = root;
  if (map)
  {
    while (*key)
    {
      while (*key && map[*key] == 0)
        key++;
      if (*key == 0)
        break;
      uint8 c = map[*key];
      Node* child = cur->c[c & (HashSize - 1)];
      while (child && child->str[0] != c)
        child = child->next;
      if (child == NULL)
      {
        int rem = 0;
        for (int j = 0; key[j]; j++)
          if (map[key[j]])
            rem++;
        child = newnode(rem);
        child->next = cur->c[c & (HashSize - 1)];
        child->parent = cur;
        cur->c[c & (HashSize - 1)] = child;
        rem = 0;
        for (; *key; key++)
          if (map[*key])
            child->str[rem++] = map[*key];
        child->str[rem] = 0;
        return child;
      }
      else
      {
        cur = child;
        int ni;
        for (ni = 0; cur->str[ni] && *key; ni++, key++)
        {
          while (*key && map[*key] == 0)
            key++;
          if (*key == 0)
            break;
          if (map[*key] != cur->str[ni])
            break;
        }
        if (cur->str[ni])
        {
          Node* n = newnode(ni);
          n->c[cur->str[ni] & (HashSize - 1)] = cur;
          n->parent = cur->parent;
          memcpy(n->str, cur->str, ni);
          n->str[ni] = 0;
          cur->str += ni;
          n->next = cur->next;
          cur->next = NULL;
          cur->parent = n;

          uint8 c0 = n->str[0] & (HashSize - 1);
          Node* t = n->parent->c[c0];
          if (t == cur)
            n->parent->c[c0] = n;
          else
          {
            while (t && t->next != cur)
              t = t->next;
            if (t)
              t->next = n;
          }
          cur = n;
        }
      }
    }
  }
  else
  {
    while (*key)
    {
      Node* child = cur->c[*key & (HashSize - 1)];
      while (child && child->str[0] != *key)
        child = child->next;
      if (child == NULL)
      {
        int rem = 0;
        while (key[rem])
          rem++;
        child = newnode(rem);
        child->next = cur->c[*key & (HashSize - 1)];
        child->parent = cur;
        cur->c[*key & (HashSize - 1)] = child;
        memcpy(child->str, key, rem);
        return child;
      }
      else
      {
        cur = child;
        int ni;
        for (ni = 0; cur->str[ni] && *key; ni++, key++)
          if (*key != cur->str[ni])
            break;
        if (cur->str[ni])
        {
          Node* n = newnode(ni);
          n->c[cur->str[ni] & (HashSize - 1)] = cur;
          n->parent = cur->parent;
          memcpy(n->str, cur->str, ni);
          cur->str += ni;
          n->next = cur->next;
          cur->next = NULL;
          cur->parent = n;

          uint8 c0 = n->str[0] & (HashSize - 1);
          Node* t = n->parent->c[c0];
          if (t == cur)
            n->parent->c[c0] = n;
          else
          {
            while (t && t->next != cur)
              t = t->next;
            if (t)
              t->next = n;
          }
          cur = n;
        }
      }
    }
  }
  return cur;
}
DictionaryBase::Node* DictionaryBase::basefind(uint8 const* key) const
{
  Node* cur = root;
  if (map)
  {
    while (cur && *key)
    {
      for (int ni = 0; cur->str[ni]; ni++, key++)
      {
        while (*key && map[*key] == 0)
          key++;
        if (map[*key] != cur->str[ni])
          return NULL;
      }
      while (*key && map[*key] == 0)
        key++;
      if (*key)
      {
        uint8 c = map[*key];
        cur = cur->c[c & (HashSize - 1)];
        while (cur && cur->str[0] != c)
          cur = cur->next;
      }
    }
  }
  else
  {
    while (cur && *key)
    {
      for (int ni = 0; cur->str[ni]; ni++, key++)
        if (*key != cur->str[ni])
          return NULL;
      if (*key)
      {
        cur = cur->c[*key & (HashSize - 1)];
        while (cur && cur->str[0] != *key)
          cur = cur->next;
      }
    }
  }
  return cur;
}

uint32 DictionaryBase::enumStart() const
{
  if (root->data[0])
    return uint32(root);
  else
    return enumNext(uint32(root));
}
uint32 DictionaryBase::enumNext(uint32 cur) const
{
  Node* node = (Node*) cur;
  uint8 ch = 0;
  bool newNode = false;
  while (node && (!newNode || !node->data[0]))
  {
    if (ch >= HashSize)
    {
      if (node->next)
      {
        node = node->next;
        ch = 0;
        newNode = true;
      }
      else
      {
        ch = (node->str[0] & (HashSize - 1)) + 1;
        node = node->parent;
        newNode = false;
      }
    }
    else if (node->c[ch])
    {
      node = node->c[ch];
      ch = 0;
      newNode = true;
    }
    else
      ch++;
  }
  return (uint32) node;
}
String DictionaryBase::enumGetKey(uint32 cur) const
{
  Node* node = (Node*) cur;
  String key = "";
  for (Node* node = (Node*) cur; node; node = node->parent)
    key = (char*) node->str + key;
  return key;
}

///////////////////////////////////////////

SimpleDictionary::SimpleDictionary ()
{
  nodes = NULL;
  numNodes = 0;
  maxNodes = 0;
}
SimpleDictionary::~SimpleDictionary ()
{
  delete[] nodes;
}
void SimpleDictionary::clear ()
{
  delete[] nodes;
  nodes = NULL;
  numNodes = 0;
  maxNodes = 0;
}

int SimpleDictionary::locate (String key) const
{
  int left = 0;
  int right = numNodes;
  while (left < right)
  {
    int mid = (left + right) / 2;
    if (key == nodes[mid].key)
      return mid;
    if (key < nodes[mid].key)
      right = mid;
    else
      left = mid + 1;
  }
  return -1;
}

uint32 SimpleDictionary::set (String key, uint32 value)
{
  int left = 0;
  int right = numNodes;
  while (left < right)
  {
    int mid = (left + right) / 2;
    if (key == nodes[mid].key)
    {
      uint32 result = nodes[mid].value;
      nodes[mid].value = value;
      return result;
    }
    if (key < nodes[mid].key)
      right = mid;
    else
      left = mid + 1;
  }
  if (numNodes >= maxNodes)
  {
    if (maxNodes == 0)
      maxNodes = 4;
    else
      maxNodes *= 2;
    Node* temp = new Node[maxNodes];
    for (int i = 0; i < numNodes; i++)
    {
      temp[i].key = nodes[i].key;
      temp[i].value = nodes[i].value;
    }
    delete[] nodes;
    nodes = temp;
  }
  for (int i = numNodes; i > left; i--)
  {
    nodes[i].key = nodes[i - 1].key;
    nodes[i].value = nodes[i - 1].value;
  }
  nodes[left].key = key;
  nodes[left].value = value;
  numNodes++;
  return 0;
}
uint32 SimpleDictionary::get (String key) const
{
  int pos = locate (key);
  if (pos < 0)
    return 0;
  else
    return nodes[pos].value;
}
bool SimpleDictionary::has (String key) const
{
  return locate (key) >= 0;
}
uint32 SimpleDictionary::del (String key)
{
  int pos = locate (key);
  if (pos < 0) return 0;
  uint32 result = nodes[pos].value;
  for (int i = pos + 1; i < numNodes; i++)
  {
    nodes[i - 1].key = nodes[i].key;
    nodes[i - 1].value = nodes[i].value;
  }
  numNodes--;
  return result;
}

uint32 SimpleDictionary::enumStart () const
{
  return numNodes ? 1 : 0;
}
uint32 SimpleDictionary::enumNext (uint32 cur) const
{
  return cur == numNodes ? 0 : cur + 1;
}
String SimpleDictionary::enumGetKey (uint32 cur) const
{
  return cur ? nodes[cur - 1].key : "";
}
uint32 SimpleDictionary::enumGetValue (uint32 cur) const
{
  return cur ? nodes[cur - 1].value : 0;
}
uint32 SimpleDictionary::enumSetValue (uint32 cur, uint32 value)
{
  uint32 result = 0;
  if (cur)
  {
    result = nodes[cur - 1].value;
    nodes[cur - 1].value = value;
  }
  return result;
}
