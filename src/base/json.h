#ifndef __BASE_JSON__
#define __BASE_JSON__

#include "base/types.h"
#include "base/string.h"

class File;

namespace json
{

class Value
{
  enum {itNull, itString, itInteger, itDouble, itObject, itArray, itTrue, itFalse};
  uint8 m_type;
  union
  {
    uint8 m_data[8];
    void* m_ptr;
  };
protected:
  Value(uint8 type)
    : m_type(type)
  {}
public:
  Value()
    : m_type(itNull)
  {}
  static Value* newNull();
  static Value* newInteger(int data);
  static Value* newNumber(double data);
  static Value* newString(char const* data);
  static Value* newBoolean(bool data);
  static Value* newObject();
  static Value* newArray();

  ~Value();

  enum {tNull, tString, tNumber, tObject, tArray, tBoolean};
  uint8 type() const;

  // tBoolean
  bool getBoolean() const;
  void setBoolean(bool data);

  // tString
  char const* getString() const;
  void setString(char const* data);

  // tNumber
  bool isInteger() const;
  int getInteger() const;
  double getNumber() const;
  void setInteger(int data);
  void setNumber(double data);

  // tObject
  bool has(char const* name) const;
  Value const* get(char const* name) const;
  Value* get(char const* name);
  void insert(char const* name, Value* data);
  void remove(char const* name);

  // tArray
  uint32 length() const;
  Value const* at(uint32 i) const;
  Value* at(uint32 i);
  void insert(uint32 i, Value* data);
  void append(Value* data);
  void remove(uint32 i);

  uint32 enumStart() const;
  uint32 enumNext(uint32 cur) const;
  String enumGetKey(uint32 cur) const;
  Value const* enumGetValue(uint32 cur) const;
  Value* enumGetValue(uint32 cur);
  void enumSetValue(uint32 cur, Value* data);

  static Value* parse(File* file);
  void write(File* file);
};

}

#endif // __BASE_JSON__
