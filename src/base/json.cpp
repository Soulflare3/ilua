#include "json.h"
#include "base/array.h"
#include "base/dictionary.h"
#include "base/error.h"
#include "base/file.h"
#include "base/string.h"
#include "base/utf8.h"
#include <memory.h>
#include <string.h>

namespace json
{

typedef Array<Value*> v_array;
typedef Dictionary<Value*> v_object;

Value* Value::newNull()
{
  return new Value(itNull);
}
Value* Value::newInteger(int data)
{
  Value* value = new Value(itInteger);
  *(int*) value->m_data = data;
  return value;
}
Value* Value::newNumber(double data)
{
  Value* value = new Value(itDouble);
  *(double*) value->m_data = data;
  return value;
}
Value* Value::newString(char const* data)
{
  Value* value = new Value(itString);
  uint32 size = strlen(data) + 1;
  value->m_ptr = malloc(size);
  memcpy(value->m_ptr, data, size);
  return value;
}
Value* Value::newBoolean(bool data)
{
  return new Value(data ? itTrue : itFalse);
}
Value* Value::newObject()
{
  Value* value = new Value(itObject);
  value->m_ptr = new v_object();
  return value;
}
Value* Value::newArray()
{
  Value* value = new Value(itArray);
  value->m_ptr = new v_array();
  return value;
}

Value::~Value()
{
  switch (m_type)
  {
  case itString:
    free(m_ptr);
    break;
  case itObject:
    delete (v_object*) m_ptr;
    break;
  case itArray:
    delete (v_array*) m_ptr;
    break;
  }
}

  //enum {itNull, itString, itInteger, itDouble, itObject, itArray, itTrue, itFalse};
  //enum {tNull, tString, tNumber, tObject, tArray, tBoolean};
uint8 Value::type() const
{
  switch (m_type)
  {
  case itString:
    return tString;
  case itInteger:
  case itDouble:
    return tNumber;
  case itObject:
    return tObject;
  case itArray:
    return tArray;
  case itTrue:
  case itFalse:
    return tBoolean;
  default:
    return tNull;
  }
}

// tBoolean
bool Value::getBoolean() const
{
  assert(m_type == itTrue || m_type == itFalse);
  return (m_type == itTrue);
}
void Value::setBoolean(bool data)
{
  assert(m_type == itTrue || m_type == itFalse);
  m_type = (data ? itTrue : itFalse);
}

// tString
char const* Value::getString() const
{
  assert(m_type == itString);
  return (char*) m_ptr;
}
void Value::setString(char const* data)
{
  assert(m_type == itString);
  uint32 size = strlen(data) + 1;
  free(m_ptr);
  m_ptr = malloc(size);
  memcpy(m_ptr, data, size);
}

// tNumber
bool Value::isInteger() const
{
  return (m_type == itInteger);
}
int Value::getInteger() const
{
  assert(m_type == itInteger || m_type == itDouble);
  if (m_type == itInteger)
    return *(int*) m_data;
  else
    return int(*(double*) m_data);
}
double Value::getNumber() const
{
  assert(m_type == itInteger || m_type == itDouble);
  if (m_type == itInteger)
    return double(*(int*) m_data);
  else
    return *(double*) m_data;
}
void Value::setInteger(int data)
{
  assert(m_type == itInteger || m_type == itDouble);
  m_type = itInteger;
  *(int*) m_data = data;
}
void Value::setNumber(double data)
{
  assert(m_type == itInteger || m_type == itDouble);
  m_type = itDouble;
  *(double*) m_data = data;
}

// tObject
bool Value::has(char const* name) const
{
  assert(m_type == itObject);
  return ((v_object*) m_ptr)->has(name);
}
Value const* Value::get(char const* name) const
{
  assert(m_type == itObject);
  return ((v_object*) m_ptr)->get(name);
}
Value* Value::get(char const* name)
{
  assert(m_type == itObject);
  return ((v_object*) m_ptr)->get(name);
}
void Value::insert(char const* name, Value* data)
{
  assert(m_type == itObject);
  v_object* object = (v_object*) m_ptr;
  delete object->get(name);
  object->set(name, data);
}
void Value::remove(char const* name)
{
  assert(m_type == itObject);
  v_object* object = (v_object*) m_ptr;
  delete object->get(name);
  object->del(name);
}
uint32 Value::enumStart() const
{
  assert(m_type == itObject);
  return ((v_object*) m_ptr)->enumStart();
}
uint32 Value::enumNext(uint32 cur) const
{
  assert(m_type == itObject);
  return ((v_object*) m_ptr)->enumNext(cur);
}
String Value::enumGetKey(uint32 cur) const
{
  assert(m_type == itObject);
  return ((v_object*) m_ptr)->enumGetKey(cur);
}
Value const* Value::enumGetValue(uint32 cur) const
{
  assert(m_type == itObject);
  return ((v_object*) m_ptr)->enumGetValue(cur);
}
Value* Value::enumGetValue(uint32 cur)
{
  assert(m_type == itObject);
  return ((v_object*) m_ptr)->enumGetValue(cur);
}
void Value::enumSetValue(uint32 cur, Value* data)
{
  assert(m_type == itObject);
  v_object* object = (v_object*) m_ptr;
  delete object->enumGetValue(cur);
  object->enumSetValue(cur, data);
}

// tArray
uint32 Value::length() const
{
  assert(m_type == itArray);
  return ((v_array*) m_ptr)->length();
}
Value const* Value::at(uint32 i) const
{
  assert(m_type == itArray);
  return (*(v_array*) m_ptr)[i];
}
Value* Value::at(uint32 i)
{
  assert(m_type == itArray);
  return (*(v_array*) m_ptr)[i];
}
void Value::insert(uint32 i, Value* data)
{
  assert(m_type == itArray);
  ((v_array*) m_ptr)->insert(i, data);
}
void Value::append(Value* data)
{
  assert(m_type == itArray);
  ((v_array*) m_ptr)->push(data);
}
void Value::remove(uint32 i)
{
  assert(m_type == itArray);
  delete (*(v_array*) m_ptr)[i];
  ((v_array*) m_ptr)->remove(i);
}

namespace parser
{

class Parser
{
  File* file;
  int ch;
  int move()
  {
    int old = ch;
    ch = file->getc();
    return old;
  }
  enum {tEnd, tSymbol, tString, tInteger, tDouble, tError = -1};
  int type;
  String value;
  int ivalue;
  double dvalue;
  int next();
  Value* int_parse();
public:
  Parser(File* f);
  Value* parse();
};

Parser::Parser(File* f)
  : file(f)
  , ch(0)
{}
int Parser::next()
{
  while (!file->eof() && isspace(ch))
    move();
  if (ch == 0)
    return type = tEnd;
  if (ch == '"')
  {
    type = tString;
    value = "";
    move();
    while (ch != '"')
    {
      if (ch == '\\')
      {
        move();
        switch (ch)
        {
        case '"':
        case '\\':
        case '/':
          value += move();
          break;
        case 'b':
          value += '\b';
          move();
          break;
        case 'f':
          value += '\f';
          move();
          break;
        case 'n':
          value += '\n';
          move();
          break;
        case 'r':
          value += '\r';
          move();
          break;
        case 't':
          value += '\t';
          move();
          break;
        case 'u':
          {
            move();
            wchar_t w[2] = {0, 0};
            for (int i = 0; i < 4; i++)
            {
              if (ch >= '0' && ch <= '9')
                w[0] = w[0] * 16 + (move() - '0');
              else if (ch >= 'a' && ch <= 'f')
                w[0] = w[0] * 16 + (move() - 'a') + 10;
              else if (ch >= 'A' && ch <= 'F')
                w[0] = w[0] * 16 + (move() - 'A') + 10;
              else
                return type = tError;
            }
            value += String::fromWide(w);
          }
          break;
        default:
          return type = tError;
        }
      }
      else if (ch < 0x7F)
        value += move();
      else
      {
        int lead = ch;
        if ((lead & 0xF8) == 0xF8)
          return type = tError;
        value += move();
        lead <<= 1;
        if (!(lead & 0x80))
          return type = tError;
        while (lead & 0x80)
        {
          if ((ch & 0xC0) != 0x80)
            return type = tError;
          value += move();
          lead <<= 1;
        }
      }
    }
    move();
  }
  else if (ch == '-' || (ch >= '0' && ch <= '9'))
  {
    value = "";
    type = tInteger;
    if (ch == '-')
      value += move();
    if (ch == '0')
      value += move();
    else if (ch >= '1' && ch <= '9')
    {
      while (ch >= '0' && ch <= '9')
        value += move();
    }
    else
      return type = tError;
    if (ch == '.')
    {
      type = tDouble;
      value += move();
      if (ch < '0' || ch > '9')
        return type = tError;
      while (ch >= '0' && ch <= '9')
        value += move();
    }
    if (ch == 'e' || ch == 'E')
    {
      type = tDouble;
      value += move();
      if (ch == '-' || ch == '+')
        value += move();
      if (ch < '0' || ch > '9')
        return type = tError;
      while (ch >= '0' && ch <= '9')
        value += move();
    }
    if (type == tInteger)
      ivalue = value.toInt();
    else
      dvalue = value.toDouble();
  }
  else if (ch == '{' || ch == '}' || ch == '[' || ch == ']' || ch == ':' || ch == ',')
  {
    type = tSymbol;
    value = String((char) move());
  }
  else if (ch >= 'a' && ch <= 'z')
  {
    type = tSymbol;
    value = "";
    while (ch >= 'a' && ch <= 'z')
      value += move();
  }
  else
    type = tError;
  return type;
}

Value* Parser::int_parse()
{
  if (type == tString)
  {
    Value* res = Value::newString(value);
    next();
    return res;
  }
  if (type == tInteger)
  {
    Value* res = Value::newInteger(ivalue);
    next();
    return res;
  }
  if (type == tDouble)
  {
    Value* res = Value::newNumber(dvalue);
    next();
    return res;
  }
  if (type == tSymbol)
  {
    if (value == "null")
    {
      next();
      return Value::newNull();
    }
    if (value == "true")
    {
      next();
      return Value::newBoolean(true);
    }
    if (value == "false")
    {
      next();
      return Value::newBoolean(false);
    }
    if (value == "{")
    {
      Value* res = Value::newObject();
      next();
      if (type != tSymbol || value != "}")
      {
        while (true)
        {
          if (type != tString)
          {
            delete res;
            return NULL;
          }
          String key = value;
          next();
          if (type != tSymbol || value != ":")
          {
            delete res;
            return NULL;
          }
          next();
          Value* sub = int_parse();
          if (sub == NULL)
          {
            delete res;
            return NULL;
          }
          res->insert(key, sub);
          if (type != tSymbol || (value != "," && value != "}"))
          {
            delete res;
            return NULL;
          }
          if (value == "}")
            break;
          next();
        }
      }
      next();
      return res;
    }
    if (value == "[")
    {
      Value* res = Value::newArray();
      next();
      if (type != tSymbol || value != "]")
      {
        while (true)
        {
          Value* sub = int_parse();
          if (sub == NULL)
          {
            delete res;
            return NULL;
          }
          res->append(sub);
          if (type != tSymbol || (value != "," && value != "]"))
          {
            delete res;
            return NULL;
          }
          if (value == "]")
            break;
          next();
        }
      }
      next();
      return res;
    }
  }
  return NULL;
}
Value* Parser::parse()
{
  move();
  next();
  Value* res = int_parse();
  if (type != tEnd)
  {
    delete res;
    res = NULL;
  }
  return res;
}

static void write_str(char const* str, File* file)
{
  uint8_const_ptr ptr = (uint8_const_ptr) str;
  file->putc('"');
  while (*ptr)
  {
    uint8_const_ptr prev = ptr;
    uint32 cp = utf8::transform(&ptr, NULL);
    if (cp == '"')
      file->printf("\\\"");
    else if (cp == '\\')
      file->printf("\\\\");
    else if (cp == '/')
      file->printf("\\/");
    else if (cp == '\b')
      file->printf("\\b");
    else if (cp == '\f')
      file->printf("\\f");
    else if (cp == '\n')
      file->printf("\\n");
    else if (cp == '\r')
      file->printf("\\r");
    else if (cp == '\t')
      file->printf("\\t");
    else if (cp < 32)
      file->printf("\\u%04X", cp);
    else
      file->write(prev, ptr - prev);
  }
  file->putc('"');
}
static void write(Value* value, File* file, String prefix)
{
  switch (value->type())
  {
  case Value::tNull:
    file->printf("null");
    break;
  case Value::tString:
    write_str(value->getString(), file);
    break;
  case Value::tNumber:
    if (value->isInteger())
      file->printf("%d", value->getInteger());
    else
      file->printf("%lf", value->getNumber());
    break;
  case Value::tBoolean:
    file->printf(value->getBoolean() ? "true" : "false");
    break;
  case Value::tObject:
    {
      file->printf("{\r\n");
      String iPrefix = prefix + "  ";
      bool prev = false;
      for (uint32 cur = value->enumStart(); cur; cur = value->enumNext(cur))
      {
        if (prev)
          file->printf(",\r\n");
        prev = true;
        file->printf(iPrefix);
        write_str(value->enumGetKey(cur), file);
        file->printf(" : ");
        write(value->enumGetValue(cur), file, iPrefix);
      }
      if (prev)
        file->printf("\r\n");
      file->printf("%s}", prefix);
    }
    break;
  case Value::tArray:
    {
      file->printf("[\r\n");
      String iPrefix = prefix + "  ";
      for (uint32 i = 0; i < value->length(); i++)
      {
        file->printf(iPrefix);
        write(value->at(i), file, iPrefix);
        file->printf(i == value->length() - 1 ? "\r\n" : ",\r\n");
      }
      file->printf("%s]", prefix);
    }
    break;
  }
}

}

Value* Value::parse(File* file)
{
  parser::Parser p(file);
  return p.parse();
}
void Value::write(File* file)
{
  parser::write(this, file, "");
}

}
