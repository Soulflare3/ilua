#ifndef __BASE_MUTEX__
#define __BASE_MUTEX__

class Mutex
{
  void* impl;
public:
  Mutex();
  ~Mutex();
  void lock();
  void unlock();

  class Locker
  {
    Mutex* mutex;
  public:
    Locker(Mutex* m)
      : mutex(m)
    {
      if (mutex)
        mutex->lock();
    }
    ~Locker()
    {
      if (mutex)
        mutex->unlock();
    }
  };
};

#endif // __BASE_MUTEX__
