// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef EQDS_H
#define EQDS_H

#include <memory>
#include <tuple>
#include <list>

#include "eventlist.h"
#include "trigger.h"
#include "eqdspacket.h"
#include "circular_buffer.h"


#define timeInf 0
//min RTO bound in us
// *** don't change this default - override it by calling EqdsSrc::setMinRTO()
#define DEFAULT_EQDS_RTO_MIN 100

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
class EqdsLogger;

// EqdsNIC aggregates EqdsSrcs that are on the same NIC.  It round
// robins between active srcs when we're limited by the sending
// linkspeed due to outcast (or just at startup) - this avoids
// building an output queue like the old NDP simulator did, and so
// better models what happens in a h/w NIC.
class EqdsNIC : public EventSource {
public:
    EqdsNIC(EventList &eventList, linkspeed_bps linkspeed);

    //handle traffic sources.
    bool requestSending(EqdsSrc& src);
    void startSending(EqdsSrc& src, mem_b pkt_size);
    void cantSend(EqdsSrc& src);

    //handle control traffic from receivers.
    bool sendControlPacket(EqdsBasePacket* pkt);
    void doNextEvent();
private:
    list <EqdsSrc*> _active_srcs;
    list<EqdsBasePacket*> _control;
    mem_b _control_size;

    linkspeed_bps _linkspeed;
    int _num_queued_srcs;
    simtime_picosec _send_end_time;
    mem_b _last_pktsize;

    int _ratio_data, _ratio_control, _crt;
};

class EqdsSrc : public EventSource, public PacketSink, public TriggerTarget {
 public:
    struct Stats {
        uint64_t sent;
        uint64_t timeouts;
        uint64_t nacks;
        uint64_t pulls;
        uint64_t rts_nacks;
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
    mem_b maxWnd() const {return _maxwnd;}

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
    mem_b flowsize() {return _flow_size;}
    inline PacketFlow* flow(){return &_flow;}

    inline flowid_t flowId() const { return _flow.flow_id();}


    // status for debugging
    uint32_t _new_packets_sent;
    uint32_t _rtx_packets_sent;
    uint32_t _rts_packets_sent;
    uint32_t _bounces_received;
 
    static bool _debug;
    bool _debug_src;
    bool debug() const {return _debug_src;}
   
 private:
    EqdsNIC& _nic;
    struct sendRecord {
        // need a constructor to be able to put this in a map
        sendRecord(mem_b psize, simtime_picosec stime) :
            pkt_size(psize), send_time(stime) {};
        mem_b pkt_size;
        simtime_picosec send_time;
    };
    EqdsLogger* _logger;
    TrafficLogger* _pktlogger;
    FlowEventLogger* _flow_logger;
    Trigger* _end_trigger;
    
    // TODO in-flight packet storage - acks and sacks clear it
    //list<EqdsDataPacket*> _activePackets;

    // we need to access the in_flight packet list quickly by sequence number, or by send time.
    map <EqdsDataPacket::seq_t, sendRecord> _tx_bitmap;
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
    EqdsBasePacket::pull_quanta computePullTarget();
    simtime_picosec computeDynamicRTO(simtime_picosec send_time);
    void handlePull(EqdsBasePacket::pull_quanta pullno);
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
    EqdsBasePacket::pull_quanta  _pull_target;
    EqdsBasePacket::pull_quanta  _pull;
    mem_b _credit_pull;  // receive request credit in pull_quanta, but consume it in bytes
    mem_b _credit_spec;
    inline mem_b credit() const;
    void stopSpeculating();
    // spendCredit returns true if we can send, and sets whether the send is speculative
    bool spendCredit(mem_b pktsize, bool& speculative); 
    EqdsDataPacket::seq_t _highest_sent;
    mem_b _in_flight;
    bool _send_blocked_on_nic;

    // entropy value calculation
    uint16_t _no_of_paths; // must be a power of 2
    uint16_t _path_random; // random upper bits of EV, set at startup and never changed
    uint16_t _path_xor; // random value set each time we wrap the entropy values - XOR with _current_ev_index
    uint16_t _current_ev_index; // count through _no_of_paths and then wrap.  XOR with _path_xor to get EV
    vector <uint8_t> _ev_skip_bitmap;  // paths scores for load balancing
    uint8_t _max_penalty; // max value we allow in _path_penalties (typically 1 or 2).

    // RTT estimate data for RTO
    simtime_picosec _rtt, _mdev, _rto;
    bool _rtx_timeout_pending; // is the RTO running?
    simtime_picosec _rto_send_time; // when we sent the oldest packet that the RTO is waiting on.
    simtime_picosec _rtx_timeout; // when the RTO is currently set to expire
    simtime_picosec _last_rts;  // time when we last sent an RTS (or zero if never sent)
    EventList::Handle _rto_timer_handle;

    simtime_picosec _last_credit_move;

    // It may seem odd that _speculating can be true in a non
    // SPECULATING state, but it's possible to move between
    // SPECULATING and IDLE without _speculating going false
    enum {INITIALIZE_CREDIT, SPECULATING, COMMITTED, IDLE} _state;
    bool _speculating;   

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

    EqdsSink(TrafficLogger *trafficLogger, EqdsPullPacer* pullPacer, EqdsNIC &nic);
    EqdsSink(TrafficLogger *trafficLogger, linkspeed_bps linkSpeed, double rate_modifier, uint16_t mtu, EventList &eventList, EqdsNIC &nic);
    virtual void receivePacket(Packet &pkt);

    void processData(const EqdsDataPacket& pkt);
    void processRts(const EqdsRtsPacket& pkt);
    void processTrimmed(const EqdsDataPacket& pkt);

    void handlePullTarget(EqdsBasePacket::seq_t pt);

    virtual const string& nodename() { return _nodename; }
    virtual uint64_t cumulative_ack() {return _expected_epsn;}
    virtual uint32_t drops() {return 0;}

    inline flowid_t flowId() const { return _flow.flow_id();}

    EqdsPullPacket* pull();

    bool shouldSack();
    uint16_t unackedPackets();
    void setEndTrigger(Trigger& trigger);

    EqdsBasePacket::seq_t sackBitmapBase(EqdsBasePacket::seq_t epsn);
    EqdsBasePacket::seq_t sackBitmapBaseIdeal();
    uint64_t buildSackBitmap(EqdsBasePacket::seq_t ref_epsn);
    EqdsAckPacket* sack(uint16_t path_id, EqdsBasePacket::seq_t seqno, bool ce);

    EqdsNackPacket* nack(uint16_t path_id, EqdsBasePacket::seq_t seqno);

    EqdsBasePacket::pull_quanta backlog() { if (_highest_pull_target > _latest_pull) return _highest_pull_target - _latest_pull; else return 0;}
    EqdsBasePacket::pull_quanta slowCredit() { if (_highest_pull_target >= _latest_pull) return 0; else return _latest_pull - _highest_pull_target;}

    EqdsBasePacket::pull_quanta rtx_backlog() { return _retx_backlog;}
    const Stats &stats() const { return _stats; }
    void connect(EqdsSrc*, Route *routeback);
    void setSrc(uint32_t s) {_srcaddr = s;}
    inline void setFlowId(flowid_t flow_id) { _flow.set_flowid(flow_id);}

    inline bool inPullQueue() const {return _in_pull;}
    inline bool inSlowPullQueue() const {return _in_slow_pull;}

    inline void addToPullQueue() { _in_pull = true;}
    inline void removeFromPullQueue() { _in_pull = false;}
    inline void addToSlowPullQueue() { _in_pull = false; _in_slow_pull = true;}
    inline void removeFromSlowPullQueue() { _in_pull = false; _in_slow_pull = false;}
    inline EqdsNIC* getNIC() const {return &_nic;}  

    uint16_t nextEntropy();
    
    EqdsSrc* getSrc(){ return _src;}
    uint32_t getMaxCwnd() { return _src->maxWnd();};

    static mem_b _bytes_unacked_threshold;
    static EqdsBasePacket::pull_quanta _credit_per_pull;
    static int TGT_EV_SIZE;

    // for sink logger
    inline mem_b total_received() const {return _stats.bytes_received;}
    uint32_t reorder_buffer_size(); // count is in packets
private:
    uint32_t _srcaddr;
    EqdsNIC & _nic;
    EqdsSrc* _src;
    PacketFlow _flow;
    EqdsPullPacer* _pullPacer;
    EqdsBasePacket::seq_t _expected_epsn;
    EqdsBasePacket::seq_t _high_epsn;
    EqdsBasePacket::seq_t _ref_epsn;//used for SACK bitmap calculation in spec, unused here for NOW. 
    EqdsBasePacket::pull_quanta _retx_backlog;
    EqdsBasePacket::pull_quanta _latest_pull;
    EqdsBasePacket::pull_quanta _highest_pull_target;

    bool _in_pull;//this tunnel is in the pull queue.
    bool _in_slow_pull;//this tunnel is in the slow pull queue.

    const Route* _route;

    mem_b _received_bytes;

    uint16_t _accepted_bytes;

    Trigger* _end_trigger;
    ModularVector<uint8_t, eqdsMaxInFlightPkts> _epsn_rx_bitmap; // list of packets above a hole, that we've received
    
    uint32_t _out_of_order_count;
    bool _ack_request;

    uint16_t _entropy;

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


