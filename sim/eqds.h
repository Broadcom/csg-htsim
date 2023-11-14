// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef EQDS_H
#define EQDS_H

#include <memory>
#include <tuple>
#include <list>

#include "eventlist.h"
#include "trigger.h"
#include "eqdspacket.h"

#define timeInf 0
//min RTO bound in us
// *** don't change this default - override it by calling EqdsSrc::setMinRTO()
#define DEFAULT_EQDS_RTO_MIN 200

template <typename T, unsigned Size>
class ModularVector {
    // Size is a power of 2 (because of seqno wrap-around)                                                                                                  
    //static_assert(!(Size & (Size - 1)));
    T buf[Size];

 public:
    ModularVector(T default_value) {
        for (uint i = 0; i < Size; i++) {
            buf[i] = default_value;
        }
    }
    T &operator [](unsigned idx) { return buf[idx & (Size - 1)]; }
};

static const unsigned eqdsMaxInFlightPkts = 1 << 12;
class EqdsPullPacer;
class EqdsSink;
class EqdsSrc;

// EqdsNIC aggregates EqdsSrcs that are on the same NIC.  It round
// robins between active srcs when we're limited by the sending
// linkspeed due to outcast (or just at startup) - this avoids
// building an output queue like the old NDP simulator did, and so
// better models what happens in a h/w NIC.
class EqdsNIC : public EventSource {

public:
    EqdsNIC(EventList &eventList, linkspeed_bps linkspeed);
    bool requestSending(EqdsSrc& src);
    void startSending(EqdsSrc& src, mem_b pkt_size);
    void cantSend(EqdsSrc& src);
    void doNextEvent();
private:
    list <EqdsSrc*> _active_srcs;
    linkspeed_bps _linkspeed;
    int _num_queued_srcs;
    simtime_picosec _send_end_time;
    mem_b _last_pktsize;
};


class EqdsSrc : public EventSource, public PacketSink, public TriggerTarget {
 public:
    struct Stats {
        uint64_t sent;
        uint64_t timeouts;
        uint64_t nacks;
        uint64_t pulls;
    };
    EqdsSrc(TrafficLogger *trafficLogger, EventList &eventList, EqdsNIC &nic, bool rts = false);
    void logFlowEvents(FlowEventLogger& flow_logger) {_flow_logger = &flow_logger;}
    virtual void connect(Route &routeout, Route &routeback, EqdsSink &sink, simtime_picosec start);
    void timeToSend();
    void receivePacket(Packet &pkt) ;
    void doNextEvent();
    void setDst(uint32_t dst) {_dstaddr = dst;}
    static void setMinRTO(uint32_t min_rto_in_us) {_min_rto = timeFromUs((uint32_t)min_rto_in_us);}
    void setCwnd(mem_b cwnd) {
        _maxwnd = cwnd;
        _cwnd = cwnd;
    }
    const Stats &stats() const { return _stats; }

    void setEndTrigger(Trigger& trigger);
    // called from a trigger to start the flow.
    virtual void activate();

    static uint32_t _path_entropy_size; // now many paths do we include in our path set
    static int _global_node_count;
    static simtime_picosec _min_rto;
    static uint16_t _hdr_size;
    static uint16_t _mss; // does not include header
    static uint16_t _mtu; // does include header
    
    virtual const string& nodename() { return _nodename; }
    inline void setFlowId(flowid_t flow_id) { _flow.set_flowid(flow_id);}
    void setFlowsize(uint64_t flow_size_in_bytes);
    uint64_t flowsize() {return _flow_size;}
    inline PacketFlow* flow(){return &_flow;}

    inline flowid_t flowId() const { return _flow.flow_id();}


    // status for debugging
    uint32_t _new_packets_sent;
    uint32_t _rtx_packets_sent;
    uint32_t _rts_packets_sent;
    uint32_t _bounces_received;
 
    static bool _debug;
   
 private:
    EqdsNIC& _nic;
    struct sendRecord {
        // need a constructor to be able to put this in a map
        sendRecord(mem_b psize, simtime_picosec stime) :
            pkt_size(psize), send_time(stime) {};
        mem_b pkt_size;
        simtime_picosec send_time;
    };
    bool _rts;

    EqdsLogger* _logger;
    TrafficLogger* _pktlogger;
    FlowEventLogger* _flow_logger;
    Trigger* _end_trigger;
    
    // TODO in-flight packet storage - acks and sacks clear it
    //list<EqdsDataPacket*> _activePackets;

    // we need to access the in_flight packet list quickly by sequence number, or by send time.
    map <EqdsDataPacket::seq_t, sendRecord> _active_packets;
    map <simtime_picosec, EqdsDataPacket::seq_t> _send_times;
    
    map <EqdsDataPacket::seq_t, mem_b> _rtx_queue;
    void startFlow();
    bool isSpeculative();
    uint16_t nextEntropy();
    void sendIfPermitted();
    mem_b sendPacket();
    mem_b sendNewPacket();
    mem_b sendRtxPacket();
    void sendRTS();
    void createSendRecord(EqdsDataPacket::seq_t seqno, mem_b pkt_size);
    void queueForRtx(EqdsBasePacket::seq_t seqno, mem_b pkt_size);
    void recalculateRTO();
    void startRTO(simtime_picosec send_time);
    void clearRTO(); // timer just expired, clear the state
    void cancelRTO(); // cancel running timer and clear state

    // not used, except for debugging timer issues
    void checkRTO() {
        if (_rtx_timeout_pending )
            assert(_rto_timer_handle != eventlist().nullHandle());
        else
            assert(_rto_timer_handle == eventlist().nullHandle());
    }
    
    void rtxTimerExpired();
    mem_b computePullTarget();
    simtime_picosec computeRTO(simtime_picosec send_time);
    void handlePull(mem_b pullno);
    void handleAckno(EqdsDataPacket::seq_t ackno);
    void handleCumulativeAck(EqdsDataPacket::seq_t cum_ack);
    void processAck(const EqdsAckPacket& pkt);
    void processNack(const EqdsNackPacket& pkt);
    void processPull(const EqdsPullPacket& pkt);
    bool checkFinished(EqdsDataPacket::seq_t cum_ack);
    inline void penalizePath(uint16_t path_id, uint8_t penalty);
    Stats _stats;
    EqdsSink* _sink;

    // unlike in the NDP simulator, we maintain all the main quantities in bytes
    mem_b _flow_size;
    bool _done_sending; // make sure we only trigger once
    mem_b _backlog; // how much we need to send, including retransmissions
    mem_b _unsent;  // how much new stuff we need to send, ignoring retransmissions
    mem_b _cwnd;
    mem_b _maxwnd;
    mem_b _pull_target;
    mem_b _highest_pull;
    mem_b _received_credit; 
    mem_b _speculative_credit;
    inline mem_b credit() const;
    void clearSpeculativeCredit();
    bool spendCredit(mem_b pktsize);
    EqdsDataPacket::seq_t _highest_sent;
    mem_b _in_flight;
    bool _send_blocked_on_nic;

    // entropy value calculation
    uint16_t _no_of_paths; // must be a power of 2
    uint16_t _path_random; // random upper bits of EV, set at startup and never changed
    uint16_t _path_xor; // random value set each time we wrap the entropy values - XOR with _current_ev_index
    uint16_t _current_ev_index; // count through _no_of_paths and then wrap.  XOR with _path_xor to get EV
    vector <uint8_t> _path_penalties;  // paths scores for load balancing
    uint8_t _max_penalty; // max value we allow in _path_penalties (typically 1 or 2).

    // RTT estimate data for RTO
    simtime_picosec _rtt, _mdev, _rto;
    bool _rtx_timeout_pending; // is the RTO running?
    simtime_picosec _rto_send_time; // when we sent the oldest packet that the RTO is waiting on.
    simtime_picosec _rtx_timeout; // when the RTO is currently set to expire
    simtime_picosec _last_rts;  // time when we last sent an RTS (or zero if never sent)
    EventList::Handle _rto_timer_handle;

    // Connectivity
    PacketFlow _flow;
    string _nodename;
    int _node_num;
    uint32_t _dstaddr;
    const Route* _route;  // we're only going to support ECMP_HOST for now.
};


class EqdsSink : public PacketSink, public DataReceiver {
 public:
    struct Stats {
        uint64_t received;
        uint64_t bytes_received;
        uint64_t duplicates;
        uint64_t out_of_order;
        uint64_t trimmed;
        uint64_t pulls;
        uint64_t rts;
    };

    EqdsSink(TrafficLogger *trafficLogger, EqdsPullPacer* pullPacer);
    EqdsSink(TrafficLogger *trafficLogger, linkspeed_bps linkSpeed, double rate_modifier, uint16_t mtu, EventList &eventList);
    virtual void receivePacket(Packet &pkt);
    virtual const string& nodename() { return _nodename; }
    virtual uint64_t cumulative_ack() {return _cumulative_ack;}
    virtual uint32_t drops() {return 0;}

    inline flowid_t flowId() const { return _flow.flow_id();}

    EqdsBasePacket* pull();

    bool shouldSack();
    uint16_t unackedPackets();
    void setEndTrigger(Trigger& trigger);

    EqdsBasePacket::seq_t sackBitmapBase(EqdsBasePacket::seq_t epsn);
    EqdsBasePacket::seq_t sackBitmapBaseIdeal();
    uint64_t buildSackBitmap(EqdsBasePacket::seq_t ref_epsn);
    EqdsAckPacket* sack(uint16_t path_id, EqdsBasePacket::seq_t seqno, bool ce);

    EqdsNackPacket* nack(uint16_t path_id, EqdsBasePacket::seq_t seqno);

    mem_b backlog() { return _highest_pull_target - _pull_no;}
    mem_b rtx_backlog() { return _rtx_backlog;}
    const Stats &stats() const { return _stats; }
    void connect(EqdsSrc*, Route *routeback);
    void setSrc(uint32_t s) {_srcaddr = s;}
    inline void setFlowId(flowid_t flow_id) { _flow.set_flowid(flow_id);}
    
    EqdsSrc* getSrc(){ return _src;}
    uint32_t getMaxCwnd() { return 100000; /* need _src->_maxwnd;*/};

    static mem_b _bytes_unacked_threshold;
    static mem_b _credit_per_pull;

    // for sink logger
    inline mem_b total_received() const {return _stats.bytes_received;}
    uint32_t reorder_buffer_size(); // count is in packets
private:
    uint32_t _srcaddr;
    EqdsSrc* _src;
    PacketFlow _flow;
    EqdsPullPacer* _pullPacer;
    EqdsBasePacket::seq_t _cumulative_ack;
    EqdsBasePacket::seq_t _highest_received;
    mem_b _rtx_backlog;
    mem_b _pull_no;
    mem_b _highest_pull_target;
    const Route* _route;
    uint16_t _bytes_unacked;
    mem_b _received_bytes;
    Trigger* _end_trigger;
    ModularVector<uint8_t, eqdsMaxInFlightPkts> _out_of_order; // list of packets above a hole, that we've received

    Stats _stats;
    string _nodename;
};

class EqdsPullPacer : public EventSource {
    list<EqdsSink*> _rtx_senders; // TODO priorities?
    list<EqdsSink*> _active_senders; // TODO priorities?
    list<EqdsSink*> _idle_senders; // TODO priorities?

    const simtime_picosec _pktTime;
    bool _active;

 public:
    EqdsPullPacer(linkspeed_bps linkSpeed, double pull_rate_modifier, uint16_t mtu, EventList &eventList);
    void doNextEvent() ;
    void requestPull(EqdsSink *sink);
    void requestRetransmit(EqdsSink *sink);

    bool isActive(EqdsSink *sink);
    bool isRetransmitting(EqdsSink *sink);
    bool isIdle(EqdsSink *sink);
};

#endif // EQDS_H


