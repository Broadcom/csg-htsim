// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#ifndef CBR_H
#define CBR_H

/*
 * A non responsive flow, source and sink
 */

#include <list>
#include "config.h"
#include "network.h"
#include "eventlist.h"

class CbrSink;

class CbrSrc: public EventSource {
public:
    CbrSrc(EventList &eventlist,linkspeed_bps rate,simtime_picosec active=0,simtime_picosec idle=0);

    void connect(route_t& routeout, CbrSink& sink, simtime_picosec startTime);
    void send_packet();
    void doNextEvent();
 
    // should really be private, but loggers want to see:
    linkspeed_bps _bitrate;
    int _crt_id;  
    int _mss;
    simtime_picosec _period,_active_time,_idle_time,_start_active,_end_active;
    bool _is_active;
 
private:
    // Connectivity
    PacketFlow _flow;
    CbrSink* _sink;
    route_t* _route;
 
    // Mechanism
    void send_packets();
};

class CbrSink : public PacketSink, public DataReceiver {
    friend class CbrSrc;
public:
    CbrSink();
    ~CbrSink(){};
    void receivePacket(Packet& pkt);
    uint32_t _last_id; // the id of the last packet we have received
    uint32_t _received;//number of packets received;_last_id-_received = dropped packets 
    uint64_t _cumulative_ack;//_received * 1000 - this is for loggers

    uint64_t cumulative_ack(){return _cumulative_ack;}
    uint32_t drops(){return 1;}

    const string& nodename(){return _nodename;};
private:
    string _nodename;
    // Connectivity
};

#endif
