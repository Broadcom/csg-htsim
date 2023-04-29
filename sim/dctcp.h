// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef DCTCP_H
#define DCTCP_H

/*
 * A DCTCP source simply changes the congestion control algorithm.
 */

#include "tcp.h"

class DCTCPSrc : public TcpSrc {
public:
    DCTCPSrc(TcpLogger* logger, TrafficLogger* pktlogger, EventList &eventlist);
    ~DCTCPSrc(){}

    // Mechanism
    virtual void deflate_window();
    virtual void receivePacket(Packet& pkt);
    virtual void rtx_timer_hook(simtime_picosec now,simtime_picosec period);

private:
    uint32_t _past_cwnd;
    double _alfa;
    uint32_t _pkts_seen, _pkts_marked;
};

#endif
