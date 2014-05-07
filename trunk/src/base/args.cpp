#include "args.h"

void ArgumentParser::registerArgument(WideString name, WideString defaultValue, bool required)
{
  int position = m_default.length();
  DefaultValue& value = m_default.push();
  value.name = name;
  value.defaultValue = defaultValue;
  value.required = required;
  m_arguments.set(name, position);
}

bool ArgumentParser::parse(wchar_t const* cmd, ArgumentList& result)
{
  // list of specified required arguments; used in the end to apply default values
  Array<bool> argsSet;
  argsSet.resize(m_default.length(), false);

  while (*cmd)
  {
    if (iswspace(*cmd))
      cmd++;
    else if (*cmd == '"')
    {
      cmd++;
      wchar_t const* start = cmd;
      while (*cmd && *cmd != '"')
        cmd++;
      result.addFreeArgument(WideString(start, cmd - start));
      if (*cmd == '"')
        cmd++;
    }
    else if (*cmd != '-' || cmd[1] == 0 || iswspace(cmd[1]))
    {
      String cur = "";
      wchar_t const* start = cmd;
      while (*cmd && !iswspace(*cmd))
        cmd++;
      result.addFreeArgument(WideString(start, cmd - start));
    }
    else
    {
      // full argument name
      cmd++;
      wchar_t const* start = cmd;
      while (*cmd && !iswspace(*cmd) && *cmd != '=')
        cmd++;

      WideString name(start, cmd - start);
      if (!m_arguments.has(name))
        return false; // argument not found
      int argumentId = m_arguments.get(name);
      argsSet[argumentId] = true;
      if (m_default[argumentId].required && *cmd != '=')
      {
        // required arguments must have a value
        return false;
      }
      if (*cmd == '=')
      {
        cmd++;
        if (*cmd == '"')
        {
          cmd++;
          wchar_t const* start = cmd;
          while (*cmd && *cmd != '"')
            cmd++;
          result.addArgument(name, WideString(start, cmd - start));
          if (*cmd == '"')
            cmd++;
        }
        else
        {
          wchar_t const* start = cmd;
          while (*cmd && !isspace(*cmd))
            cmd++;
          result.addArgument(name, WideString(start, cmd - start));
        }
      }
      else
        result.addArgument(name, m_default[argumentId].defaultValue);
    }
  }
  for (int i = 0; i < m_default.length(); ++i)
    if (!argsSet[i] && m_default[i].required)
      result.addArgument(m_default[i].name, m_default[i].defaultValue);
  return true;
}
