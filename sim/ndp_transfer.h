// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef NDP_FIXED_TRANSFER_H
#define NDP_FIXED_TRANSFER_H

/*
 * A recurrent NDP flow, source and sink
 */

#include <list>
#include <vector>
#include <sstream>

#include <iostream>
#include "config.h"
#include "network.h"
#include "eventlist.h"
#include "ndp.h"
class NdpSinkTransfer;



class NdpSrcTransfer: public NdpSrc {
public:
    NdpSrcTransfer(NdpLogger* logger, TrafficLogger* pktLogger, EventList &eventlist);
    void connect(route_t* routeout, route_t* routeback, NdpSink& sink, simtime_picosec starttime);

    virtual void rtx_timer_hook(simtime_picosec now,simtime_picosec period);
    virtual void receivePacket(Packet& pkt);
    void reset(uint64_t bb, int rs);
    virtual void doNextEvent();

    uint64_t generateFlowSize();
        
    // should really be private, but loggers want to see:

    uint64_t _bytes_to_send;
    bool _is_active;
    simtime_picosec _started;
    vector<route_t*>* _paths;
    EventSource* _flow_stopped;
};

class NdpSinkTransfer : public NdpSink {
    friend class NdpSrcTransfer;
public:
    NdpSinkTransfer(EventList & ev, linkspeed_bps linkspeed, double pull_rate_modifier);
    NdpSinkTransfer(NdpPullPacer* p);
    void reset();
};

#endif
