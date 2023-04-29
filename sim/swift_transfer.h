#ifndef SWIFT_FIXED_TRANSFER_H
#define SWIFT_FIXED_TRANSFER_H

/*
 * A recurrent SWIFT flow, source and sink
 */

#include <list>
#include <vector>
#include <sstream>

#include <iostream>
#include "config.h"
#include "network.h"
#include "eventlist.h"
#include "swift.h"
class SwiftSinkTransfer;

uint64_t generateFlowSize();

class SwiftSrcTransfer: public SwiftSrc {
public:
  SwiftSrcTransfer(SwiftLogger* logger, TrafficLogger* pktLogger, EventList &eventlist,
                 uint64_t b, vector<const Route*>* p, EventSource* stopped = NULL);
  void connect(const Route& routeout, const Route& routeback, SwiftSink& sink, simtime_picosec starttime);

  virtual void rtx_timer_hook(simtime_picosec now,simtime_picosec period);
  virtual void receivePacket(Packet& pkt);
  void reset(uint64_t bb, int rs);
  virtual void doNextEvent();
 
// should really be private, but loggers want to see:

  uint64_t _bytes_to_send;
  bool _is_active;
  simtime_picosec _started;
  vector<const Route*>* _paths;
  EventSource* _flow_stopped;
};

class SwiftSinkTransfer : public SwiftSink {
friend class SwiftSrcTransfer;
public:
        SwiftSinkTransfer();

        void reset();
};

#endif
