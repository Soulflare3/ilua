#ifndef __BASE_OPTIONAL__
#define __BASE_OPTIONAL__

#include "base/types.h"

template<class T>
class Optional
{
  uint8 storage[sizeof(T)];
  bool hasValue;
public:
  Optional()
    : hasValue(false)
  {}
  Optional(Optional<T> const& opt)
    : hasValue(opt.hasValue)
  {
    if (hasValue)
      new(storage) T(*(T*) opt.storage);
  }
  Optional(T const& value)
    : hasValue(true)
  {
    new(storage) T(value);
  }
  ~Optional()
  {
    if (hasValue)
      ((T*) storage)->~T();
  }

  Optional& operator = (Optional const& opt)
  {
    if (&opt != this)
    {
      if (hasValue && !opt.hasValue)
        ((T*) storage)->~T();
      else if (hasValue && opt.hasValue)
        *(T*) storage = *(T*) opt.storage;
      else if (!hasValue && opt.hasValue)
        new(storage) T(*(T*) opt.storage);
      hasValue = opt.hasValue;
    }
    return *this;
  }
  Optional& operator = (T const& value)
  {
    if (hasValue)
      *(T*) storage = value;
    else
      new(storage) T(value);
    return *this;
  }

  operator bool() const
  {
    return hasValue;
  }

  T const& operator * () const
  {
    return *(T*) storage;
  }
  T& operator * ()
  {
    return *(T*) storage;
  }
  T const* operator -> () const
  {
    return (T*) storage;
  }
  T* operator -> ()
  {
    return (T*) storage;
  }

  T const* ptr() const
  {
    return (hasValue ? (T*) storage : NULL);
  }
  T* ptr()
  {
    return (hasValue ? (T*) storage : NULL);
  }
  void reset()
  {
    if (hasValue)
      ((T*) storage)->~T();
    hasValue = false;
  }
  T& set()
  {
    if (hasValue)
      ((T*) storage)->~T();
    new(storage) T;
    hasValue = true;
    return *(T*) storage;
  }
  void set(Optional<T> const& opt)
  {
    if (&opt != this)
    {
      if (hasValue)
        ((T*) storage)->~T();
      if (opt.hasValue)
        new(storage) T(*(T*) opt.storage);
      hasValue = opt.hasValue;
    }
  }
  T& set(T const& value)
  {
    if (hasValue)
      ((T*) storage)->~T();
    new(storage) T(value);
    hasValue = true;
    return *(T*) storage;
  }

  friend bool operator == (Optional<T> const& a, Optional<T> const& b)
  {
    if (a.hasValue != b.hasValue)
      return false;
    if (a.hasValue)
      return true;
    return *(T*) a.storage == *(T*) b.storage;
  }
  friend bool operator != (Optional<T> const& a, Optional<T> const& b)
  {
    if (a.hasValue != b.hasValue)
      return true;
    if (a.hasValue)
      return false;
    return *(T*) a.storage != *(T*) b.storage;
  }
  friend bool operator == (Optional<T> const& opt, T const& value)
  {
    return opt.hasValue && (*(T*) opt.storage == value);
  }
  friend bool operator != (Optional<T> const& opt, T const& value)
  {
    return !opt.hasValue || (*(T*) opt.storage != value);
  }
  friend bool operator == (T const& value, Optional<T> const& opt)
  {
    return opt.hasValue && (*(T*) opt.storage == value);
  }
  friend bool operator != (T const& value, Optional<T> const& opt)
  {
    return !opt.hasValue || (*(T*) opt.storage != value);
  }
};

#endif // __BASE_OPTIONAL__
