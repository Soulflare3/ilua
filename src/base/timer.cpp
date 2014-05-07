#include <Windows.h>
#include "timer.h"

uint64 Timer::frequency = 0;

Timer::Timer()
{
  start = time();
  pauseCount = 0;
}
double Timer::elapsed() const
{
  return (pauseCount ? pauseTime : time()) - start;
}
double Timer::reset()
{
  double ret = elapsed();
  start = time();
  pauseTime = start;
  return ret;
}
void Timer::pause()
{
  if (!pauseCount)
    pauseTime = time();
  pauseCount = 1;
}
void Timer::resume()
{
  if (pauseCount)
    start += time() - pauseTime;
  pauseCount = 0;
}

uint64 Timer::counter()
{
  LARGE_INTEGER c;
  QueryPerformanceCounter(&c);
  return c.QuadPart;
}
double Timer::time()
{
  if (frequency == 0)
  {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    frequency = f.QuadPart;
  }
  return double(counter()) / double(frequency);
}
