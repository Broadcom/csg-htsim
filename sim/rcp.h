// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#ifndef RCP_H
#define RCP_H

/*
 * An RCP source and sink
 */

#include <list>
#include "config.h"
#include "network.h"
#include "rcppacket.h"
#include "eventlist.h"

class RcpSink;

class RcpSrc : public PacketSink, public EventSource {
    friend class RcpSink;
public:
    RcpSrc(RcpLogger* logger, TrafficLogger* pktlogger, EventList &eventlist);
    void connect(route_t& routeout, route_t& routeback, RcpSink& sink, simtime_picosec startTime);
    void startflow();
    void doNextEvent() {startflow();}
    void receivePacket(Packet& pkt);
    void rtx_timer_hook(simtime_picosec now);
    // should really be private, but loggers want to see:
    uint32_t _highest_sent;  //seqno is in bytes
    uint32_t _cwnd;
    uint32_t _maxcwnd;
    uint32_t _last_acked;
    uint32_t _ssthresh;
    uint16_t _dupacks;
    uint16_t _mss;
    uint32_t _unacked; // an estimate of the amount of unacked data WE WANT TO HAVE in the network
    uint32_t _effcwnd; // an estimate of our current transmission rate, expressed as a cwnd
    uint32_t _recoverq;
    bool _in_fast_recovery;
private:
    // Housekeeping
    RcpLogger* _logger;
    //TrafficLogger* _pktlogger;
    // Connectivity
    PacketFlow _flow;
    RcpSink* _sink;
    route_t* _route;
    // Mechanism
    void inflate_window();
    void send_packets();
    void retransmit_packet();
    simtime_picosec _rtt;
    simtime_picosec _last_sent_time;
};

class RcpSink : public PacketSink, public Logged {
    friend class RcpSrc;
public:
    RcpSink();
    void receivePacket(Packet& pkt);
private:
    // Connectivity
    void connect(RcpSrc& src, route_t& route);
    route_t* _route;
    RcpSrc* _src;
    // Mechanism
    void send_ack();
    RcpAck::seq_t _cumulative_ack; // the packet we have cumulatively acked
    list<RcpAck::seq_t> _received; // list of packets above a hole, that we've received
};


class RcpRtxTimerScanner : public EventSource {
public:
    RcpRtxTimerScanner::RcpRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist);
    void doNextEvent();
    void registerRcp(RcpSrc &rcpsrc);
private:
    simtime_picosec _scanPeriod;
    typedef list<RcpSrc*> rcps_t;
    rcps_t _rcps;
};

#endif
