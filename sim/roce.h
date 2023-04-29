// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        


#ifndef ROCE_H
#define ROCE_H

/*
 * A ROCEv2 source and sink
 */

#include <list>
#include <map>
//#include "util.h"
#include "math.h"
#include "config.h"
#include "network.h"
#include "rocepacket.h"
#include "queue.h"
#include "eventlist.h"
#include "eth_pause_packet.h"
#include "trigger.h"

#define timeInf 0

//min RTO bound in us
// *** don't change this default - override it by calling RoceSrc::setMinRTO()
#define DEFAULT_RTO_MIN 5000

class RoceSink;
class Switch;

class RoceSrc : public BaseQueue, public TriggerTarget {
    friend class RoceSink;
public:
    RoceSrc(RoceLogger* logger, TrafficLogger* pktlogger, EventList &eventlist, linkspeed_bps rate);

    virtual void connect(Route* routeout, Route* routeback, RoceSink& sink, simtime_picosec startTime);
    void set_dst(uint32_t dst) {_dstaddr = dst;}
    void set_traffic_logger(TrafficLogger* pktlogger);

    void startflow();
    void setRate(linkspeed_bps r) {_bitrate = r;_packet_spacing = (simtime_picosec)((Packet::data_packet_size()+ACKSIZE) * (pow(10.0,12.0) * 8) / _bitrate);doNextEvent();}

    inline void set_flowid(flowid_t flow_id) { _flow.set_flowid(flow_id);}


    static void setMinRTO(uint32_t min_rto_in_us) {_min_rto = timeFromUs((uint32_t)min_rto_in_us);}

    void set_flowsize(uint64_t flow_size_in_bytes) {
        _flow_size = flow_size_in_bytes;
    }

    void set_stoptime(simtime_picosec stop_time) {
        _stop_time = stop_time;
        cout << "Setting stop time to " << timeAsSec(_stop_time) << endl;
    }

    // called from a trigger to start the flow.
    virtual void activate() {
        cout << "Activate called " << _flow._name << endl;
        startflow();
    }

    void set_end_trigger(Trigger& trigger);

    virtual void doNextEvent();
    virtual void receivePacket(Packet& pkt);

    virtual void setPath(uint32_t p) {_pathid = p;}

    virtual void processPause(const EthPausePacket& pkt);
    virtual void processAck(const RoceAck& ack);
    virtual void processNack(const RoceNack& nack);

    virtual mem_b queuesize() const { return 0;};
    virtual mem_b maxsize() const { return 0;}; 

    // should really be private, but loggers want to see:
    uint64_t _highest_sent;  //seqno is in bytes
    uint64_t _packets_sent;
    uint64_t _last_acked;
    uint32_t _new_packets_sent;  // all the below reduced to 32 bits to save RAM
    uint32_t _rtx_packets_sent;
    uint32_t _acks_received;
    uint32_t _nacks_received;

    uint32_t _acked_packets;
    uint32_t _pathid;

    enum {PAUSED,READY};

    uint32_t _dstaddr;

    void print_stats();

    //round trip time estimate, needed for RTO calculation
    simtime_picosec _rtt, _rto, _mdev,_base_rtt;

    uint16_t _mss;
    uint32_t _drops;

    RoceSink* _sink;
 
    const Route* _route;
    bool _flow_started;
    uint16_t _state_send;

    void send_packet();

    virtual const string& nodename() { return _nodename; }
    inline uint32_t flow_id() const { return _flow.flow_id();}
 
    //debugging hack
    void log_me();
    bool _log_me;

    static uint32_t _global_node_count; 
    static uint32_t _global_rto_count;  // keep track of the total number of timeouts across all srcs
    static simtime_picosec _min_rto;

private:
    // Housekeeping
    RoceLogger* _logger;
    Trigger* _end_trigger;

    TrafficLogger* _pktlogger;
    // Connectivity
    PacketFlow _flow;
    string _nodename;
    uint32_t _node_num;

    // Mechanism
    void clear_timer(uint64_t start,uint64_t end);

    uint64_t _flow_size;  //The flow size in bytes.  Stop sending after this amount.
    simtime_picosec _stop_time;
    simtime_picosec _packet_spacing;
    simtime_picosec _time_last_sent;
    bool _done;
};

class RoceSink : public PacketSink, public DataReceiver {
    friend class RoceSrc;
public:
    RoceSink();

    enum {PAUSED,READY};

    virtual void receivePacket(Packet& pkt);
    
    RoceAck::seq_t _cumulative_ack; // the packet we have cumulatively acked
    uint32_t _drops;
    uint64_t cumulative_ack() { return _cumulative_ack;}
    uint64_t total_received() const { return _cumulative_ack;}
    uint32_t drops(){ return _src->_drops;}
    virtual const string& nodename() { return _nodename; }

    void set_src(uint32_t s) {_srcaddr = s;}
 
    RoceSrc* _src;

    //debugging hack
    void log_me();
    bool _log_me;

    uint32_t _srcaddr;
    
private:
 
    // Connectivity
    void connect(RoceSrc& src, Route* route);

    inline uint32_t flow_id() const {
        return _src->flow_id();
    };

    const Route* _route;

    string _nodename;
 
    RocePacket::seq_t _last_packet_seqno; //sequence number of the last
    //packet in the connection (or 0 if not known)
    uint64_t _total_received;
    RocePacket::seq_t _highest_seqno;
 
    // Mechanism
    void send_ack(simtime_picosec ts);
    void send_nack(simtime_picosec ts, RocePacket::seq_t ackno);
};


#endif

