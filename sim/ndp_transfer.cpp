#include "ndp_transfer.h"
#include "math.h"
#include <iostream>
#include "config.h"

////////////////////////////////////////////////////////////////
//  NDP PERIODIC SOURCE
////////////////////////////////////////////////////////////////

int CDF_WEB [] = {250,500,1000,1500,2000,3000,4000,10000,100000,1000000};


NdpSrcTransfer::NdpSrcTransfer(NdpLogger* logger, TrafficLogger* pktLogger, EventList &eventlist) : NdpSrc(logger,pktLogger,eventlist)
{
  _is_active = false;

  _bytes_to_send = generateFlowSize();

  set_flowsize(_bytes_to_send);
}

void NdpSrcTransfer::reset(uint64_t bb, int shouldRestart){
  //reset here!
  _bytes_to_send = bb;
  set_flowsize(_bytes_to_send);

  if (shouldRestart)
    //eventlist().sourceIsPendingRel(*this,_rtt*(1+drand()));
    eventlist().sourceIsPendingRel(*this,timeFromMs(1));
}


uint64_t NdpSrcTransfer::generateFlowSize(){
  return CDF_WEB[(int)(drand()*10)];  
}

void 
NdpSrcTransfer::connect(route_t* routeout, route_t* routeback, NdpSink& sink, simtime_picosec starttime)
{
  _is_active = false;

  NdpSrc::connect(routeout,routeback,sink,starttime);
}

void 
NdpSrcTransfer::doNextEvent() {
  if (!_is_active){
    _is_active = true;

    ((NdpSinkTransfer*)_sink)->reset();

    _started = eventlist().now();
    startflow();
  }
  else NdpSrc::doNextEvent();
}

void 
NdpSrcTransfer::receivePacket(Packet& pkt){
  if (_is_active){
    NdpSrc::receivePacket(pkt);

    if (_bytes_to_send>0){
      if (_last_acked>=_bytes_to_send){
        _is_active = false;

        cout << endl << "Flow " << str() << " " <<  _bytes_to_send << " finished after " << timeAsMs(eventlist().now()-_started) << endl;

        reset(generateFlowSize(),1);
      }
    }
  }
  else {
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);
    pkt.free();
  }
}

void NdpSrcTransfer::rtx_timer_hook(simtime_picosec now, simtime_picosec period) {
  if (!_is_active) return;

  // FIXME
  if (now <= _rtx_timeout || _rtx_timeout==timeInf) return;

  if (_highest_sent == 0) return;

  cout << "Transfer timeout: active " << _is_active << " bytes to send " << _bytes_to_send << " sent " << _last_acked << endl;
  
  NdpSrc::rtx_timer_hook(now,period);
}

////////////////////////////////////////////////////////////////
//  Ndp Transfer SINK
////////////////////////////////////////////////////////////////

NdpSinkTransfer::NdpSinkTransfer(EventList& ev, linkspeed_bps linkspeed, double pull_rate_modifier)
  : NdpSink(ev, linkspeed, pull_rate_modifier) 
{
}

NdpSinkTransfer::NdpSinkTransfer(NdpPullPacer* pace) : NdpSink(pace) 
{
}

void NdpSinkTransfer::reset(){
  _cumulative_ack = 0;
  _received.clear();

  //queue logger sampling?
}
