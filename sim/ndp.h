// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 

#ifndef NDP_H
#define NDP_H

/*
 * A NDP source and sink
 */

#include <list>
#include <map>
#include "config.h"
#include "network.h"
#include "ndppacket.h"
#include "priopullqueue.h"
#include "trigger.h"
#include "eventlist.h"

#define timeInf 0
#define NDP_PACKET_SCATTER

//#define LOAD_BALANCED_SCATTER

//min RTO bound in us
// *** don't change this default - override it by calling NdpSrc::setMinRTO()
#define DEFAULT_RTO_MIN 5000

#define RECORD_PATH_LENS // used for debugging which paths lengths packets were trimmed on - mostly useful for BCube

#define DEBUG_PATH_STATS
enum RouteStrategy {NOT_SET, SINGLE_PATH, SCATTER_PERMUTE, SCATTER_RANDOM, PULL_BASED, SCATTER_ECMP, ECMP_FIB, ECMP_FIB_ECN, REACTIVE_ECN};

class NdpSink;
class NdpRTSPacer;
class Switch;
class ReorderBufferLogger;

class ReceiptEvent {
  public:
    ReceiptEvent()
        : _path_id(-1), _is_header(false) {};
    ReceiptEvent(uint32_t path_id, bool is_header)
        : _path_id(path_id), _is_header(is_header) {}
    inline int32_t path_id() const {return _path_id;}
    inline bool is_header() const {return _is_header;}
    int32_t _path_id;
    bool _is_header;
};

class NdpSrc : public PacketSink, public EventSource, public TriggerTarget {
    friend class NdpSink;
 public:
    NdpSrc(NdpLogger* logger, TrafficLogger* pktlogger, EventList &eventlist, bool rts = false, NdpRTSPacer* pacer = NULL);
    virtual void connect(Route* routeout, Route* routeback, NdpSink& sink, simtime_picosec startTime);

    void set_dst(uint32_t dst) {_dstaddr = dst;}
    void set_traffic_logger(TrafficLogger* pktlogger);
    void startflow();
    void setCwnd(uint32_t cwnd) {_cwnd = cwnd;}
    static void setMinRTO(uint32_t min_rto_in_us) {_min_rto = timeFromUs((uint32_t)min_rto_in_us);}
    static void setRouteStrategy(RouteStrategy strat) {_route_strategy = strat;}
    static void setPathEntropySize(uint32_t path_entropy_size) {_path_entropy_size = path_entropy_size;}
    void set_flowsize(uint64_t flow_size_in_bytes) {
            _flow_size = flow_size_in_bytes;
    }

    void set_stoptime(simtime_picosec stop_time) {
        _stop_time = stop_time;
        cout << "Setting stop time to " << timeAsSec(_stop_time) << endl;
    }

    // called from a trigger to start the flow.
    virtual void activate() {
        startflow();
    }

    void set_end_trigger(Trigger& trigger);

    virtual void doNextEvent();
    virtual void receivePacket(Packet& pkt);

    virtual void processRTS(NdpPacket& pkt);
    virtual void processAck(const NdpAck& ack);
    virtual void processNack(const NdpNack& nack);

    void replace_route(Route* newroute);

    virtual void rtx_timer_hook(simtime_picosec now,simtime_picosec period);
    
    //used by all routing strategies except SINGLE and ECMP_FIB
    void set_paths(vector<const Route*>* rt);

    //used by ECMP_FIB strategy
    void set_paths(uint32_t path_count);


    // should really be private, but loggers want to see:
    uint64_t _highest_sent;  //seqno is in bytes
    uint64_t _packets_sent;
    uint64_t _last_acked;
    uint32_t _new_packets_sent;  // all the below reduced to 32 bits to save RAM
    uint32_t _rtx_packets_sent;
    uint32_t _acks_received;
    uint32_t _nacks_received;
    uint32_t _pulls_received;
    uint32_t _implicit_pulls;
    uint32_t _bounces_received;
    uint32_t _cwnd;
    uint32_t _flight_size;
    uint32_t _acked_packets;

    // the following are used with SCATTER_PERMUTE, SCATTER_RANDOM and
    // PULL_BASED route strategies
    uint16_t _crt_path;
    uint16_t _crt_direction;
    uint16_t _same_path_burst; // how many packets in a row to use same ECMP value (default is 1)
    void set_path_burst(uint16_t path_burst) {_same_path_burst = path_burst;}
    
    vector<int> _path_ids;

    uint32_t _dstaddr;
    vector<const Route*> _paths;
    vector<const Route*> _original_paths; //paths in original permutation order
#ifdef DEBUG_PATH_STATS
    vector<int> _path_counts_new; // only used for debugging, can remove later.
    vector<int> _path_counts_rtx; // only used for debugging, can remove later.
    vector<int> _path_counts_rto; // only used for debugging, can remove later.
#endif
    vector <int16_t> _path_acks; //keeps path scores
    vector <int16_t> _path_ecns; //keeps path scores
    vector <int16_t> _path_nacks; //keeps path scores
    vector <int16_t> _avoid_ratio; //keeps path scores
    vector <int16_t> _avoid_score; //keeps path scores
    vector <bool> _bad_path; //keeps path scores

    map<NdpPacket::seq_t, simtime_picosec> _sent_times;
    map<NdpPacket::seq_t, simtime_picosec> _first_sent_times;

    void print_stats();

    int _pull_window; // Used to keep track of expected pulls so we
                      // can handle return-to-sender cleanly.
                      // Increase by one for each Ack/Nack received.
                      // Decrease by one for each Pull received.
                      // Indicates how many pulls we expect to
                      // receive, if all currently sent but not yet
                      // acked/nacked packets are lost
                      // or are returned to sender.
    int _first_window_count;

    //round trip time estimate, needed for coupled congestion control
    simtime_picosec _rtt, _rto, _mdev,_base_rtt;

    uint16_t _mss;
 
    uint32_t _drops;

    NdpSink* _sink;
 
    simtime_picosec _rtx_timeout;
    bool _rtx_timeout_pending;
    const Route* _route;

    int choose_route();
    int next_route();

    void pull_packets(NdpPull::seq_t pull_no, NdpPull::seq_t pacer_no);
    int send_packet(NdpPull::seq_t pacer_no); // returns number of packets actually sent

    virtual const string& nodename() { return _nodename; }
    inline void set_flowid(flowid_t flow_id) { _flow.set_flowid(flow_id);}
    inline flowid_t flow_id() const { return _flow.flow_id();}
 
    //debugging hack
    void log_me();
    bool _log_me;

    static uint32_t _global_rto_count;  // keep track of the total number of timeouts across all srcs
    static simtime_picosec _min_rto;
    static RouteStrategy _route_strategy;
    static uint32_t _path_entropy_size; // now many paths do we include in our path set
    static int _global_node_count;
    static int _rtt_hist[10000000];
    int _node_num;

 private:
    // Housekeeping
    NdpLogger* _logger;
    TrafficLogger* _pktlogger;
    Trigger* _end_trigger;

    // Connectivity
    PacketFlow _flow;
    string _nodename;

    bool _rts;
    NdpRTSPacer* _rts_pacer;

    enum  FeedbackType {ACK, ECN, NACK, BOUNCE, UNKNOWN};
    static const int HIST_LEN=12;
    FeedbackType _feedback_history[HIST_LEN];
    int _feedback_count;

    // Mechanism
    void clear_timer(uint64_t start,uint64_t end);
    void retransmit_packet();
    void permute_paths();
    void update_rtx_time();
    void process_cumulative_ack(NdpPacket::seq_t cum_ackno);
    inline void count_ack(int32_t path_id) {count_feedback(path_id, ACK);}
    inline void count_nack(int32_t path_id) {count_feedback(path_id, NACK);}
    inline void count_bounce(int32_t path_id) {count_feedback(path_id, BOUNCE);}
    void count_ecn(int32_t path_id);
    void count_feedback(int32_t path_id, FeedbackType fb);
    bool is_bad_path();
    void log_rtt(simtime_picosec sent_time);
    NdpPull::seq_t _last_pull;
    NdpPull::seq_t _max_pull;
    uint64_t _flow_size;  //The flow size in bytes.  Stop sending after this amount.
    simtime_picosec _stop_time;
    map <NdpPacket::seq_t, NdpPacket*> _rtx_queue; //Packets queued for (hopefuly) imminent retransmission
};

class NdpPullPacer;
class NdpRTSPacer;

class NdpSink : public PacketSink, public DataReceiver {
    friend class NdpSrc;
 public:
    NdpSink(EventList& ev, linkspeed_bps linkspeed, double pull_rate_modifier);
    NdpSink(NdpPullPacer* pacer);

    void add_buffer_logger(ReorderBufferLogger *logger) {
            _buffer_logger = logger;
    }
 
    void receivePacket(Packet& pkt);
    void process_request_to_send(NdpRTS* rts);

    void receiver_core_trim(NdpPacket* p);
    void receiver_ecn_accounting(NdpPacket* p);
    void receiver_increase(NdpPacket* p);

    NdpAck::seq_t _cumulative_ack; // the packet we have cumulatively acked
    uint32_t _drops;
    uint64_t cumulative_ack() { return _cumulative_ack + _received.size()*9000;}
    uint64_t total_received() const { return _total_received;}
    uint32_t drops(){ return _src->_drops;}
    virtual const string& nodename() { return _nodename; }
    void increase_window() {_pull_no++;} 
    static void setRouteStrategy(RouteStrategy strat) {_route_strategy = strat;}

    void set_src(uint32_t s) {_srcaddr = s;}
    void set_end_trigger(Trigger& trigger);

    list<NdpAck::seq_t> _received; // list of packets above a hole, that we've received
 
    NdpSrc* _src;

    //debugging hack
    void log_me();
    bool _log_me;

    uint32_t _srcaddr;
    
    //needed by all strategies except SINGLE and ECMP_FIB
    void set_paths(vector<const Route*>* rt);
    void set_paths(uint32_t no_of_paths);

#ifdef RECORD_PATH_LENS
#define MAX_PATH_LEN 20u
    vector<uint32_t> _path_lens;
    vector<uint32_t> _trimmed_path_lens;
#endif
    static RouteStrategy _route_strategy;

    uint64_t reorder_buffer_size() {return _received.size();};
    uint64_t reorder_buffer_max() {return _ooo;};

    void set_priority(int priority) {_priority = priority;}
    inline int priority() const {return _priority;}
    static bool _oversubscribed_congestion_control;
    static double _g;
 private:
 
    // Connectivity
    void connect(NdpSrc& src, Route* route);

    inline uint32_t flow_id() const {
            return _src->flow_id();
    };

    // the following are used with SCATTER_PERMUTE, SCATTER_RANDOM,
    // and PULL_BASED route strategies
    uint16_t _crt_path; // index into paths
    uint16_t _crt_direction;
    vector<int> _path_ids; // path IDs to be used for ECMP FIB. 
    vector<const Route*> _paths; //paths in current permutation order
    vector<const Route*> _original_paths; //paths in original permutation order
    const Route* _route;
    Trigger* _end_trigger;

    string _nodename;
    ReorderBufferLogger* _buffer_logger;
 
    NdpPullPacer* _pacer;
    NdpPull::seq_t _pull_no; // pull sequence number (local to connection)
    NdpPacket::seq_t _last_packet_seqno; //sequence number of the last
                                         //packet in the connection (or 0 if not known)
    uint64_t _total_received;
    NdpPacket::seq_t _highest_seqno;
    int _priority; // this receiver's priority relative to others on same pacer - low is best

    uint32_t _parked_cwnd;
    uint32_t _parked_increase;

    //used for DCTCP implementation 
    uint32_t _ecn_decrease;
    uint64_t _marked_bytes, _acked_bytes;
    double _alpha;

    // Mechanism
    void send_ack(simtime_picosec ts, NdpPacket::seq_t ackno, NdpPacket::seq_t pacer_no,
                  bool ecn_marked, bool enqueue_pull);
    void send_nack(simtime_picosec ts, NdpPacket::seq_t ackno, NdpPacket::seq_t pacer_no,
                   bool enqueue_pull, bool ecn_marked);
    void permute_paths();
   
    //Path History
    void update_path_history(const NdpPacket& p);
#define HISTORY_PER_PATH 1 //how much history to hold - we hold an
                           //average of HISTORY_PER_PATH entries for
                           //each possible path
    vector<ReceiptEvent> _path_history;  //this is a bit heavyweight,
                                         //but it will let us
                                         //experiment with different
                                         //algorithms
    int _path_hist_index; //index of last entry to be added to _path_history
    int _path_hist_first; //index of oldest entry added to _path_history
    int _no_of_paths;
    uint64_t _ooo;
};

class NdpPullPacer : public EventSource {
 public:
    NdpPullPacer(EventList& ev, linkspeed_bps linkspeed, double pull_rate_modifier);  
    NdpPullPacer(EventList& ev, char* fn);  
    // pull_rate_modifier is the multiplier of link speed used when
    // determining pull rate.  Generally 1 for FatTree, probable 2 for BCube
    // as there are two distinct paths between each node pair.

    void sendPacket(Packet* p, NdpPacket::seq_t pacerno, NdpSink *receiver);
    virtual void doNextEvent();
    void release_pulls(uint32_t flow_id, NdpSink *receiver);
    void enqueue_pull(NdpPull* pkt, NdpSink *receiver);

    //debugging hack
    void log_me();
    bool _log_me;

    //void set_preferred_flow(int id) { _preferred_flow = id;cout << "Preferring flow "<< id << endl;};
    NdpPull::seq_t pacer_no() {return _pacer_no;}

 private:
    void set_pacerno(Packet *pkt, NdpPull::seq_t pacer_no);

    //#define FIFO_PULL_QUEUE
#define FAIR_PULL_QUEUE
#ifdef FIFO_PULL_QUEUE
    FifoPullQueue<NdpPull> _pull_queue;
#elifdef FAIR_PULL_QUEUE
    FairPullQueue<NdpPull> _pull_queue;
#else
    PrioPullQueue<NdpPull> _pull_queue;
#endif
    simtime_picosec _last_pull;
    simtime_picosec _packet_drain_time;
    NdpPull::seq_t _pacer_no; // pull sequence number, shared by all connections on this pacer

    //pull distribution from real life
    static int _pull_spacing_cdf_count;
    static double* _pull_spacing_cdf;

    //debugging
    double _total_excess;
    int _excess_count;
    //int _preferred_flow;
};


class NdpRTSPacer : public EventSource {
 public:
    NdpRTSPacer(EventList& ev, linkspeed_bps linkspeed, double pull_rate_modifier);  

    // pull_rate_modifier is the multiplier of link speed used when
    // determining pull rate.  Generally 1 for FatTree, probable 2 for BCube
    // as there are two distinct paths between each node pair.

    virtual void doNextEvent();

    void enqueue_rts(NdpRTS* pkt);

 private:
    //#define RTS_FIFO_PULL_QUEUE

#ifdef RTS_FIFO_PULL_QUEUE
    FifoPullQueue<NdpRTS> _rts_queue;
#else
    FairPullQueue<NdpRTS> _rts_queue;
#endif
    
    simtime_picosec _last_rts;
    bool _first;
    simtime_picosec _packet_drain_time;
};


class NdpRtxTimerScanner : public EventSource {
 public:
    NdpRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist);
    void doNextEvent();
    void registerNdp(NdpSrc &tcpsrc);
 private:
    simtime_picosec _scanPeriod;
    typedef list<NdpSrc*> tcps_t;
    tcps_t _tcps;
};

#endif

