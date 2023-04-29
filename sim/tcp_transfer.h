// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#ifndef TCP_FIXED_TRANSFER_H
#define TCP_FIXED_TRANSFER_H

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
#include "tcp.h"
#include "mtcp.h"
class TcpSinkTransfer;

uint64_t generateFlowSize();

class TcpSrcTransfer: public TcpSrc {
public:
    TcpSrcTransfer(TcpLogger* logger, TrafficLogger* pktLogger, EventList &eventlist,
                   uint64_t b, vector<const Route*>* p, EventSource* stopped = NULL);
    void connect(const Route& routeout, const Route& routeback, TcpSink& sink, simtime_picosec starttime);

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

class TcpSinkTransfer : public TcpSink {
    friend class TcpSrcTransfer;
public:
    TcpSinkTransfer();

    void reset();
};

#endif
