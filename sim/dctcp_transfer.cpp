// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "dctcp_transfer.h"
#include "mtcp.h"
#include "math.h"
#include <iostream>
#include "config.h"

////////////////////////////////////////////////////////////////
//  TCP PERIODIC SOURCE
////////////////////////////////////////////////////////////////

extern int CDF_WEB [];

DCTCPSrcTransfer::DCTCPSrcTransfer(TcpLogger* logger, TrafficLogger* pktLogger, EventList &eventlist,
                               uint64_t bytes_to_send, vector<const Route*>* p, 
                               EventSource* stopped) : DCTCPSrc(logger,pktLogger,eventlist)
{
  _is_active = false;  
  _ssthresh = 0xffffffff;
  //_cwnd = 90000;
  _bytes_to_send = generateFlowSize();
  set_flowsize(_bytes_to_send+2*_mss);
  _paths = p;

  _flow_stopped = stopped;

  //#if PACKET_SCATTER
  //set_paths(p);
  //#endif
}

uint64_t DCTCPSrcTransfer::generateFlowSize(){
  return CDF_WEB[(int)(drand()*10)];  
}

void DCTCPSrcTransfer::reset(uint64_t bb, int shouldRestart){
  _sawtooth = 0;
  _rtt_avg = timeFromMs(0);
  _rtt_cum = timeFromMs(0);
  _highest_sent = 0;
  _effcwnd = 0;
  _ssthresh = 0xffffffff;
  _last_acked = 0;
  _dupacks = 0;
  _bytes_to_send = generateFlowSize();
  set_flowsize(_bytes_to_send+2*_mss);  
  _mdev = 0;
  _rto = timeFromMs(3000);
  _recoverq = 0;
  _in_fast_recovery = false;
  _established = false;
  
  _rtx_timeout_pending = false;
  _RFC2988_RTO_timeout = timeInf;
  
  //_bytes_to_send = bb;

  if (shouldRestart)
      eventlist().sourceIsPendingRel(*this,timeFromMs(1));
}

void 
DCTCPSrcTransfer::connect(const Route& routeout, const Route& routeback, TcpSink& sink, simtime_picosec starttime)
{
  _is_active = false;

  DCTCPSrc::connect(routeout,routeback,sink,starttime);
}

void 
DCTCPSrcTransfer::doNextEvent() {
  if (!_is_active){
    _is_active = true;

    //delete _route;
    if (_paths!=NULL){
        Route* rt = new Route(*(_paths->at(rand()%_paths->size())));
        rt->push_back(_sink);
        _route = rt;
    }

    //should reset route here!
    //how?
    ((DCTCPSinkTransfer*)_sink)->reset();

    _started = eventlist().now();
    startflow();
  }
  else DCTCPSrc::doNextEvent();
}

void 
DCTCPSrcTransfer::receivePacket(Packet& pkt){
  if (_is_active){
      DCTCPSrc::receivePacket(pkt);

      if (_bytes_to_send>0){
          assert (!_mSrc);
          if (_last_acked>=_bytes_to_send){
              _is_active = false;
              
              cout << endl << "Flow " << str() << " " <<_bytes_to_send << " finished after " << timeAsMs(eventlist().now()-_started) << endl;
              
              if (_flow_stopped){
                  _flow_stopped->doNextEvent();
              }
              else 
                  reset(_bytes_to_send,1);
          }
      }
  }
  else {
      pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);
      pkt.free();
  }
}

void DCTCPSrcTransfer::rtx_timer_hook(simtime_picosec now, simtime_picosec period) {
  if (!_is_active) return;

  if (now <= _RFC2988_RTO_timeout || _RFC2988_RTO_timeout==timeInf) return;
  if (_highest_sent == 0) return;

  cout << "Transfer timeout: active " << _is_active << " bytes to send " << _bytes_to_send << " sent " << _last_acked << " established? " << _established << " HSENT " << _highest_sent << endl;
  
  DCTCPSrc::rtx_timer_hook(now,period);
}

////////////////////////////////////////////////////////////////
//  DCTCP Transfer SINK
////////////////////////////////////////////////////////////////

DCTCPSinkTransfer::DCTCPSinkTransfer() : TcpSink() 
{
}

void DCTCPSinkTransfer::reset(){
  _cumulative_ack = 0;
  _received.clear();

  //queue logger sampling?
}
