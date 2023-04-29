// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#ifndef MTCP_H
#define MTCP_H

/*
 * A MTCP source and sink
 */

#include <math.h>
#include <list>
#include "config.h"
#include "network.h"
#include "tcp.h"
#include "eventlist.h"
#include "sent_packets.h"

#define USE_AVG_RTT 0
#define UNCOUPLED 1

#define FULLY_COUPLED 2
//params for fully coupled
#define A 1
#define B 2

#define COUPLED_INC 3
#define COUPLED_TCP 4

#define COUPLED_EPSILON 5
#define COUPLED_SCALABLE_TCP 6

//#define MODEL_RECEIVE_WINDOW 1
//#define STALL_SLOW_SUBFLOWS 1
//#define REXMIT_ENABLED 1
//#define DYNAMIC_RIGHT_SIZING 1

class MultipathTcpSink;

class MultipathTcpSrc : public PacketSink, public EventSource {
public:
    MultipathTcpSrc(char cc_type,EventList & ev,MultipathTcpLogger* logger,int rwnd = 1000);
    void addSubflow(TcpSrc* tcp);
    void receivePacket(Packet& pkt);
  
    // should really be private, but loggers want to see:

    uint32_t inflate_window(uint32_t cwnd,int newly_acked,uint32_t mss);
    uint32_t deflate_window(uint32_t cwnd, uint32_t mss);
    void window_changed();
    void doNextEvent();
    double compute_a();
    uint32_t compute_a_scaled();
    uint32_t compute_a_tcp();
    double compute_alfa();

    uint64_t compute_total_bytes();

    uint32_t a;
    // Connectivity; list of subflows
    list<TcpSrc*> _subflows; // list of active subflows for this connection


    void connect(MultipathTcpSink* sink) {
        _sink = sink;
    };

    virtual const string& nodename() { return _nodename; }
#ifdef MODEL_RECEIVE_WINDOW
    int getDataSeq(uint64_t* seq,TcpSrc* subflow);

    uint64_t _highest_sent;
    uint64_t _last_acked;

    bool _packets_mapped[100000][4];
    simtime_picosec _last_reduce[4];

    //this maintains the total number of bytes allowed in flight past the data_ack
    //there is no need to have it signalled from the receiver if there is no flow control
    uint64_t _receive_window;
#endif

    double _alfa;
    uint32_t compute_total_window();
    MultipathTcpSink* _sink;
private:
    MultipathTcpLogger* _logger;

    char _cc_type;

    double _e;

    string _nodename;
};

class MultipathTcpSink : public PacketSink , public EventSource {
public:
    MultipathTcpSink(EventList& ev);
    void addSubflow(TcpSink* tcp);
    void receivePacket(Packet& pkt);
        
    uint64_t data_ack(){
        return _cumulative_ack;
    };
    uint64_t cumulative_ack(){ return _cumulative_ack + _received.size()*1000;}

    uint32_t drops(){ return 0;}

    void doNextEvent();
    virtual const string& nodename() { return _nodename; }
    list<TcpAck::seq_t> _received; // list of packets above a hole, that we've received
private:

    // Connectivity
#ifdef DYNAMIC_RIGHT_SIZING
    simtime_picosec _last_min_rtt;
    TcpAck::seq_t _last_seq;
#endif
    list<TcpSink*> _subflows;

    // Mechanism
    TcpAck::seq_t _cumulative_ack; // the packet we have cumulatively acked
        
    string _nodename;
};
#endif
