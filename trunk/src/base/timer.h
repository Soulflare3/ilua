#ifndef __BASE_TIMER__
#define __BASE_TIMER__

#include "base/types.h"

class Timer
{
  static uint64 frequency;
  double start;
  double pauseTime;
  int pauseCount;
public:
  Timer();

  double elapsed() const;
  double reset();
  void pause();
  void resume();
  bool paused() const
  {
    return pauseCount == 0;
  }

  static uint64 counter();
  static double time();
};

#endif // __BASE_TIMER__
