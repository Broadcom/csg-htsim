#ifndef DCTCP_FIXED_TRANSFER_H
#define DCTCP_FIXED_TRANSFER_H

/*
 * A recurrent TCP flow, source and sink
 */

#include <list>
#include <vector>
#include <sstream>

#include <iostream>
#include "config.h"
#include "network.h"
#include "eventlist.h"
#include "dctcp.h"
#include "mtcp.h"
class DCTCPSinkTransfer;

uint64_t generateFlowSize();

class DCTCPSrcTransfer: public DCTCPSrc {
public:
  DCTCPSrcTransfer(TcpLogger* logger, TrafficLogger* pktLogger, EventList &eventlist,
                 uint64_t b, vector<const Route*>* p, EventSource* stopped = NULL);
  void connect(const Route& routeout, const Route& routeback, TcpSink& sink, simtime_picosec starttime);

  virtual void rtx_timer_hook(simtime_picosec now,simtime_picosec period);
  virtual void receivePacket(Packet& pkt);
  void reset(uint64_t bb, int rs);
  virtual void doNextEvent();
  uint64_t generateFlowSize();
  
// should really be private, but loggers want to see:

  uint64_t _bytes_to_send;
  bool _is_active;
  simtime_picosec _started;
  vector<const Route*>* _paths;
  EventSource* _flow_stopped;
};

class DCTCPSinkTransfer : public TcpSink {
friend class DCTCPSrcTransfer;
public:
        DCTCPSinkTransfer();

        void reset();
};

#endif
