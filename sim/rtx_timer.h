#ifndef __RTX_TIMER
#define __RTX_TIMER

#include <list>
#include <hash_map>
#include <map>
#include "config.h"
#include "network.h"
#include "tcppacket.h"
#include "eventlist.h"


class RtxTimer {
 public:
  RtxTimer(EventList & list);

  void schedule_timer(TcpSrc* who,simtime_picosec when);
  void reschedule_timer(TcpSrc* who,simtime_picosec when);
  void remove_timer(TcpSrc* who);

  void check_lists(); 

  struct timer_entry {
    TcpSrc* who;
    simtime_picosec when;
  };

  EventList& _eventList;

  struct eqint
  {
    bool operator()(const int i1, const int i2) const
    {
      return i1==i2;
    }
  };

  struct ltint
  {
    bool operator()(const int i1, const int i2) const
    {
      return i1<i2;
    }
  };

  struct lttime
  {
    bool operator()(const simtime_picosec i1, const simtime_picosec i2) const
    {
      return i1<i2;
    }
  };


  struct eqtcp
  {
    bool operator()(const TcpSrc* src1, const TcpSrc* src2) const
    {
      return src1==src2;
    }
  };

  typedef hash_map<const TcpSrc*, timer_entry*, hash<int>,eqtcp> Timers;

  struct TimerBucket {
    Timers timers;
    simtime_picosec base;
    simtime_picosec expire;
  }

  map<simtime_picosec,timer_entry*,lttime> _soon;
  vector<TimerBucket*> _buckets;

  hash_set<const TcpSrc*, hash<int>, eqint> _active_timers;
  hash_map<const TcpSrc*, TimerBucket*,hash<int>,eqint> _timer_to_bucket;
};

#endif
