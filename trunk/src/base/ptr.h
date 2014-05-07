#ifndef __BASE_PTR__
#define __BASE_PTR__

template<class T, bool is_array = false>
class LocalPtr
{
  T* m_ptr;
public:
  LocalPtr(T* value)
    : m_ptr(value)
  {}
  ~LocalPtr()
  {
    if (is_array)
      delete[] m_ptr;
    else
      delete m_ptr;
  }

  T* operator ->()
  {
    return m_ptr;
  }
  operator T*()
  {
    return m_ptr;
  }
  T* ptr()
  {
    return m_ptr;
  }
  T& operator[] (int i)
  {
    return m_ptr[i];
  }

  T* yield()
  {
    T* val = m_ptr;
    m_ptr = NULL;
    return val;
  }
};

template<class T>
class RefPtr
{
  T* m_ptr;
public:
  RefPtr(T* value)
    : m_ptr(value)
  {}
  ~RefPtr()
  {
    m_ptr->release();
  }

  T* ptr()
  {
    return m_ptr;
  }

  T* operator ->()
  {
    return m_ptr;
  }
};

#endif // __BASE_PTR__
