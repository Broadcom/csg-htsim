#include "tcp_periodic.h"
#include "math.h"
#include <iostream>

////////////////////////////////////////////////////////////////
//  TCP PERIODIC SOURCE
////////////////////////////////////////////////////////////////

TcpSrcPeriodic::TcpSrcPeriodic(TcpLogger* logger, TrafficLogger* pktLogger, EventList &eventlist,simtime_picosec active,simtime_picosec idle)
  : TcpSrc(logger,pktLogger,eventlist)
{
  _start_active = 0;
  _end_active = 0;
  _idle_time = idle;
  _active_time = active;
  _is_active = false;  
  _ssthresh = 0xffffffff;
}

void TcpSrcPeriodic::reset(){
  _sawtooth = 0;
  _rtt_avg = timeFromMs(0);
  _rtt_cum = timeFromMs(0);
  _highest_sent = 0;
  _effcwnd = 0;
  _ssthresh = 0xffffffff;
  _last_acked = 0;
  _dupacks = 0;
  _rtt = 0;
  _rto = timeFromMs(3000);
  _mdev = 0;
  _recoverq = 0;
  _in_fast_recovery = false;
  _mSrc = NULL;

  _rtx_timeout_pending = false;
  _RFC2988_RTO_timeout = timeInf;
}

void 
TcpSrcPeriodic::connect(const Route& routeout, const Route& routeback, TcpSink& sink, simtime_picosec starttime)
{
  _is_active = false;

  TcpSrc::connect(routeout,routeback,sink,starttime);
}

void 
TcpSrcPeriodic::doNextEvent() {
  if (_idle_time==0||_active_time==0){
    _is_active = true;
    startflow();
    return;
  }

  if (_is_active){
    if (eventlist().now()>=_end_active && _idle_time!=0 && _active_time!=0 ){
      _is_active = false;
      
      //this clears RTOs too
      reset();
      eventlist().sourceIsPendingRel(*this,(simtime_picosec)(2*drand()*_idle_time));
    }
    else if (_rtx_timeout_pending) {
      TcpSrc::doNextEvent();
    }
    else {
      cout << "Wrong place to be in: doNextDEvent 1" << eventlist().now() << " end active " << _end_active << "timeout " << _rtx_timeout_pending << endl;
      //maybe i got a timeout here. How?
      exit(1);
    }
  }
  else {
    _is_active = true;
    ((TcpSinkPeriodic*)_sink)->reset();
    _start_active = eventlist().now();
    _end_active = _start_active + (simtime_picosec)(2*drand()*_active_time);
    eventlist().sourceIsPending(*this,_end_active);
    startflow();
  }
}

void 
TcpSrcPeriodic::receivePacket(Packet& pkt){
  if (_is_active)
    TcpSrc::receivePacket(pkt);
  else {
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);
    pkt.free();
  }
}

////////////////////////////////////////////////////////////////
//  Tcp Periodic SINK
////////////////////////////////////////////////////////////////

TcpSinkPeriodic::TcpSinkPeriodic() : TcpSink() 
{
}

void TcpSinkPeriodic::reset(){
  _cumulative_ack = 0;
  _received.clear();

  //queue logger sampling?
}





