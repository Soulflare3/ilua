#ifndef __BASE_OBJECT_H__
#define __BASE_OBJECT_H__

class Object
{
  int refCount;
public:
  Object()
  {
    refCount = 1;
  }

  Object* retain()
  {
    refCount++;
    return this;
  }
  int release()
  {
    if (this == NULL)
      return 0;
    if (--refCount)
      return refCount;
    delete this;
    return 0;
  }
};

#endif // __BASE_OBJECT_H__
