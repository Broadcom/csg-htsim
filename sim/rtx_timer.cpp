#include "rtx_timer.h"

RtxTimer::RtxTimer(EventList& list):
  _eventList(list)
{
  TimerBucket* bucket;
  simtime_picosec now;
  now = _eventList().now();

  for (int i=50;i<=400;i*=2){
    bucket = new TimerBucket();
    bucket->base = timeFromMs(i);
    bucket->expire  = now + bucket->base;
  }
}

//time is absolute!
void RtxTimer::schedule_timer(TcpSrc* who,simtime_picosec when){
  timer_entry * t;

  if (active_timers.find(who)!=active_timers.end()){
    t = remove_timer(who);
  }
  else {
    t = new timer_entry();
    active_timers.insert(who);
  }
  t->who = who;
  t->when = when;
  
  simtime_picosec now = _eventList().now(), delta = when-now;

  vector<TimerBucket*>::iterator it;
  TimerBucket* crt = NULL;
  int j = -1;

  for (int i = 0;i<_buckets.size();i++){
    crt = _buckets[i];
    
    if (crt->base>delta){
      j = i-1;
      break;
    }
    else j = i;
  }

  if (j<0){
    _soon[t->when] = t;
  }
  else{
    //insert in that timerbucket;
    _buckets[j]->timers[who] = t;
    _timer_to_bucket[who] = _buckets[j];
  }
}

void RtxTimer::reschedule_timer(TcpSrc* who,simtime_picosec when){
  //fast case only!
  if (_active_timers.find(who)==_active_timers.end()){
    return schedule_timer(who,when);
  }  
  bool ok = false;
  
  if (_timer_to_bucket.find(who)!=_timer_to_bucket.end()){
    TimerBucket* bucket = _timer_to_bucket[who];
    simtime_picosec now = _eventList().now(), delta = when-now;
    if (delta>bucket->base) //ok to reschedule here!
      if (bucket->timers.find(who)!=bucket->timers.end()){
        t = bucket->timers[who];
        t->when = when;
        ok = true;
      }
  }

  if (!ok) 
    schedule_timer(who,when);
}

void RtxTimer::remove_timer(TcpSrc* who){
  timer_entry * t = NULL;

  if (_active_timers.find(who)==_active_timers.end()){
    return NULL;
  }  

  _active_timers.erase(who);

  if (_timer_to_bucket.find(who)!=_timer_to_bucket.end()){
    TimerBucket* bucket = _timer_to_bucket[who];
    if (bucket->timers.find(who)!=bucket->timers.end()){
      t = bucket->timers[who];
      bucket->timers.erase(who);
    }

    _timer_to_bucket.erase(_timer_to_bucket.find(who).second);
  }
  else {
    list<timer_entry*>::iterator it;
    bool found = false;

    for (it = _soon.begin();it!=_soon.end();it++){
      t = _soon[*it];
      if (t->who==who){
        found = true;
        _soon.erase(it);
        break;
      }
    }
    if (!found)
      t = NULL;
  }

  return t;
}
