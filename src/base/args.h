#ifndef __BASE_ARGS__
#define __BASE_ARGS__

#include "base/wstring.h"
#include "base/array.h"
#include "base/hashmap.h"

class ArgumentList
{
public:
  ArgumentList()
  {}
  ~ArgumentList()
  {}

  void addArgument(WideString name, WideString value)
  {
    m_arguments.set(name, value);
  }
  void addFreeArgument(WideString value)
  {
    m_freeArguments.push(value);
  }

  bool hasArgument(WideString name) const
  {
    return m_arguments.has(name);
  }
  WideString getArgumentString(WideString name) const
  {
    return m_arguments.get(name);
  }
  int getArgumentInt(WideString name) const
  {
    return m_arguments.get(name).toInt();
  }
  double getArgumentReal(WideString name) const
  {
    return m_arguments.get(name).toDouble();
  }
  bool getArgumentBool(WideString name) const
  {
    WideString value = m_arguments.get(name);
    return (value.icompare(L"true") == 0 || value.icompare(L"yes") == 0 || value == L"1");
  }
  int getFreeArgumentCount() const
  {
    return m_freeArguments.length();
  }
  WideString getFreeArgument(int index) const
  {
    return m_freeArguments[index];
  }

private:
  HashMap<WideString, WideString> m_arguments;
  Array<WideString> m_freeArguments;
};

class ArgumentParser
{
public:
  ArgumentParser()
  {}
  ~ArgumentParser()
  {}

  void registerArgument(WideString name, WideString defaultValue, bool required = false);

  bool parse(wchar_t const* cmd, ArgumentList& result);
private:
  struct DefaultValue
  {
    WideString name;
    WideString defaultValue;
    bool required;
  };
  Array<DefaultValue> m_default;
  HashMap<WideString, int> m_arguments;
};

#endif // __BASE_ARGS__
