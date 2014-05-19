#ifndef __BASE_PAIR__
#define __BASE_PAIR__

#include <algorithm>

template<class First, class Second>
struct Pair
{
  First first;
  Second second;

  Pair() {}
  Pair(Pair<First, Second> const& p)
    : first(p.first)
    , second(p.second)
  {}
  Pair(First const& theFirst, Second const& theSecond)
    : first(theFirst)
    , second(theSecond)
  {}
};
template<class First, class Second>
bool operator == (Pair<First, Second> const& a, Pair<First, Second> const& b)
{return a.first == b.first && a.second == b.second;}
template<class First, class Second>
bool operator != (Pair<First, Second> const& a, Pair<First, Second> const& b)
{return a.first != b.first || a.second != b.second;}
template<class First, class Second>
bool operator < (Pair<First, Second> const& a, Pair<First, Second> const& b)
{return a.first < b.first || (a.first == b.first && a.second < b.second);}
template<class First, class Second>
bool operator <= (Pair<First, Second> const& a, Pair<First, Second> const& b)
{return a.first < b.first || (a.first == b.first && a.second <= b.second);}
template<class First, class Second>
bool operator > (Pair<First, Second> const& a, Pair<First, Second> const& b)
{return a.first > b.first || (a.first == b.first && a.second > b.second);}
template<class First, class Second>
bool operator >= (Pair<First, Second> const& a, Pair<First, Second> const& b)
{return a.first > b.first || (a.first == b.first && a.second >= b.second);}

template<class First, class Second>
Pair<First, Second> MakePair(First const& first, Second const& second)
{
  return Pair<First, Second>(first, second);
}

#endif // __BASE_PAIR__
