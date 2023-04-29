// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        


#ifndef NDP_H
#define NDP_H
enum RouteStrategy {NOT_SET, SINGLE_PATH, SCATTER_PERMUTE, SCATTER_RANDOM, PULL_BASED};

#endif

#ifndef NDPTUNNEL_H
#define NDPTUNNEL_H

/*
 * A NDP tunnel source and sink
 */

#include <list>
#include <map>
#include "config.h"
#include "network.h"
#include "ndppacket.h"
#include "ndptunnelpacket.h"
#include "fairpullqueue.h"
#include "eventlist.h"

#define timeInf 0
#define NDP_PACKET_SCATTER

#define DEFAULT_RTO_MIN 5000

#define RECORD_PATH_LENS // used for debugging which paths lengths packets were trimmed on - mostly useful for BCube

class NdpTunnelSink;

class NdpTunnelSrc : public PacketSink, public EventSource {
    friend class NdpTunnelSink;

public:
    NdpTunnelSrc(NdpTunnelLogger* logger, TrafficLogger* pktlogger, EventList &eventlist);
    virtual void connect(Route& routeout, Route& routeback, NdpTunnelSink& sink, simtime_picosec startTime);
    void set_traffic_logger(TrafficLogger* pktlogger);
    void startflow();
    void setCwnd(uint32_t cwnd) {_cwnd = cwnd;}
    static void setMinRTO(uint32_t min_rto_in_us) {_min_rto = timeFromUs((uint32_t)min_rto_in_us);}
    static void setRouteStrategy(RouteStrategy strat) {_route_strategy = strat;}
    void set_flowsize(uint64_t flow_size_in_bytes) {
        _flow_size = flow_size_in_bytes;
    }

    virtual void doNextEvent();
    virtual void receivePacket(Packet& pkt);

    virtual void processRTS(NdpTunnelPacket& pkt);
    virtual void processAck(const NdpAck& ack);
    virtual void processNack(const NdpNack& nack);

    void replace_route(Route* newroute);

    virtual void rtx_timer_hook(simtime_picosec now,simtime_picosec period);
    void set_paths(vector<const Route*>* rt);

    // should really be private, but loggers want to see:
    uint64_t _highest_sent;  //seqno is in bytes
    uint64_t _packets_sent;
    uint64_t _new_packets_sent;
    uint64_t _rtx_packets_sent;
    uint64_t _acks_received;
    uint64_t _nacks_received;
    uint64_t _pulls_received;
    uint64_t _implicit_pulls;
    uint64_t _bounces_received;
    uint32_t _cwnd;
    uint64_t _last_acked;
    uint32_t _flight_size;
    uint32_t _acked_packets;

    // the following are used with SCATTER_PERMUTE, SCATTER_RANDOM and
    // PULL_BASED route strategies
    uint16_t _crt_path;
    uint16_t _crt_direction;
    vector<const Route*> _paths;
    vector<const Route*> _original_paths; //paths in original permutation order

    map<NdpTunnelPacket::seq_t, simtime_picosec> _sent_times;
    map<NdpTunnelPacket::seq_t, simtime_picosec> _first_sent_times;

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

    NdpTunnelSink* _sink;
 
    simtime_picosec _rtx_timeout;
    bool _rtx_timeout_pending;

    const Route* _route;
    const Route *choose_route();

    void pull_packets(NdpPull::seq_t pull_no, NdpPull::seq_t pacer_no);
    int send_packet(NdpPull::seq_t pacer_no);

    virtual const string& nodename() { return _nodename; }
    inline uint32_t flow_id() const { return _flow.flow_id();}

    void set_queue_size(int qs){_maxqs = qs;}
    
    //debugging hack
    void log_me();
    bool _log_me;

    static uint32_t _global_rto_count;  // keep track of the total number of timeouts across all srcs
    static simtime_picosec _min_rto;
    static RouteStrategy _route_strategy;
    static int _global_node_count;
    static int _rtt_hist[10000000];
    int _node_num;

 private:
    // Housekeeping
    NdpTunnelLogger* _logger;
    TrafficLogger* _pktlogger;
    // Connectivity
    PacketFlow _flow;
    string _nodename;

    enum  FeedbackType {ACK, NACK, BOUNCE, UNKNOWN};

    // Mechanism
    void clear_timer(uint64_t start,uint64_t end);
    void retransmit_packet();
    void permute_paths();
    void update_rtx_time();
    void process_cumulative_ack(NdpTunnelPacket::seq_t cum_ackno);

    void log_rtt(simtime_picosec sent_time);
    NdpPull::seq_t _last_pull;
    uint64_t _flow_size;  //The flow size in bytes.  Stop sending after this amount.

    uint32_t _qs,_maxqs;
    list<Packet*> _queue;
    list<NdpTunnelPacket*> _inflight;
    list<NdpTunnelPacket*> _rtx_queue; //Packets queued for (hopefuly) imminent retransmission
};

class NdpTunnelPullPacer;

class NdpTunnelSink : public PacketSink, public DataReceiver {
    friend class NdpTunnelSrc;
 public:
    NdpTunnelSink(EventList& ev, linkspeed_bps linkspeed, double pull_rate_modifier);
    NdpTunnelSink(NdpTunnelPullPacer* pacer);
 
    void receivePacket(Packet& pkt);
    NdpAck::seq_t _cumulative_ack; // the packet we have cumulatively acked
    uint32_t _drops;
    uint64_t cumulative_ack() { return _cumulative_ack + _received.size()*9000;}
    uint64_t total_received() const { return _total_received;}
    uint32_t drops(){ return _src->_drops;}
    virtual const string& nodename() { return _nodename; }
    void increase_window() {_pull_no++;} 
    static void setRouteStrategy(RouteStrategy strat) {_route_strategy = strat;}

    list<NdpTunnelPacket*> _received; // list of packets above a hole, that we've received
 
    NdpTunnelSrc* _src;

    //debugging hack
    void log_me();
    bool _log_me;

    void set_paths(vector<const Route*>* rt);

    static RouteStrategy _route_strategy;
    int priority() const {return _priority;}
 private:
 
    // Connectivity
    void connect(NdpTunnelSrc& src, Route& route);

    inline uint32_t flow_id() const {
        return _src->flow_id();
    };

    // the following are used with SCATTER_PERMUTE, SCATTER_RANDOM,
    // and PULL_BASED route strategies
    uint16_t _crt_path;
    uint16_t _crt_direction;
    vector<const Route*> _paths; //paths in current permutation order
    vector<const Route*> _original_paths; //paths in original permutation order
    const Route* _route;

    string _nodename;
 
    NdpTunnelPullPacer* _pacer;
    NdpPull::seq_t _pull_no; // pull sequence number (local to connection)
    NdpTunnelPacket::seq_t _last_packet_seqno; //sequence number of the last
                                         //packet in the connection (or 0 if not known)
    uint64_t _total_received;

    int _priority;
 
    // Mechanism
    void send_ack(simtime_picosec ts, NdpTunnelPacket::seq_t ackno, NdpTunnelPacket::seq_t pacer_no);
    void send_nack(simtime_picosec ts, NdpTunnelPacket::seq_t ackno, NdpTunnelPacket::seq_t pacer_no);
    void permute_paths();
    
};

class NdpTunnelPullPacer : public EventSource {
 public:
    NdpTunnelPullPacer(EventList& ev, linkspeed_bps linkspeed, double pull_rate_modifier);  
    NdpTunnelPullPacer(EventList& ev, char* fn);  
    // pull_rate_modifier is the multiplier of link speed used when
    // determining pull rate.  Generally 1 for FatTree, probable 2 for BCube
    // as there are two distinct paths between each node pair.

    void sendPacket(Packet* p, NdpTunnelPacket::seq_t pacerno, NdpTunnelSink *receiver);
    virtual void doNextEvent();
    void release_pulls(uint32_t flow_id);

    //debugging hack
    void log_me();
    bool _log_me;

    void set_preferred_flow(int id) { _preferred_flow = id;cout << "Preferring flow "<< id << endl;};

 private:
    void set_pacerno(Packet *pkt, NdpPull::seq_t pacer_no);
//#define FIFO_PULL_QUEUE
//#ifdef FIFO_PULL_QUEUE
//   FifoPullQueue<NdpPull> _pull_queue;
//#else
    FairPullQueue<NdpPull> _pull_queue;
    //#endif
    simtime_picosec _last_pull;
    simtime_picosec _packet_drain_time;
    NdpPull::seq_t _pacer_no; // pull sequence number, shared by all connections on this pacer

    //pull distribution from real life
    static int _pull_spacing_cdf_count;
    static double* _pull_spacing_cdf;

    //debugging
    double _total_excess;
    int _excess_count;
    int _preferred_flow;
};


class NdpTunnelRtxTimerScanner : public EventSource {
 public:
    NdpTunnelRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist);
    void doNextEvent();
    void registerNdp(NdpTunnelSrc &tcpsrc);
 private:
    simtime_picosec _scanPeriod;
    typedef list<NdpTunnelSrc*> tcps_t;
    tcps_t _tcps;
};

#endif

