// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#ifndef STRACK_H
#define STRACK_H

/*
 * A STrack source and sink, loosely based off of Tcp
 */

#include <list>
#include <set>
#include "config.h"
#include "network.h"
#include "strackpacket.h"
#include "swift_scheduler.h"
#include "eventlist.h"
#include "sent_packets.h"

//#define MODEL_RECEIVE_WINDOW 1

#define timeInf 0

class STrackSink;
class STrackSrc;
class STrackRtxTimerScanner;
class BaseScheduler;

class STrackPacer : public EventSource {
public:
    STrackPacer(STrackSrc& sub, EventList& eventlist);
    bool is_pending() const {return _interpacket_delay > 0;}  // are we pacing?
    void schedule_send(simtime_picosec delay);  // schedule a paced packet "delay" picoseconds after the last packet was sent
    void cancel();     // cancel pacing
    void just_sent();  // called when we've just sent a packet, even if it wasn't paced
    void doNextEvent();
private:
    STrackSrc* _src;
    simtime_picosec _interpacket_delay; // the interpacket delay, or zero if we're not pacing
    simtime_picosec _last_send;  // when the last packet was sent (always set, even when we're not pacing)
    simtime_picosec _next_send;  // when the next scheduled packet should be sent
};

class STrackSrc : public EventSource, public PacketSink, public ScheduledSrc {
    friend class STrackSink;
    friend class STrackRtxTimerScanner;
    //friend class STrackSubflowSrc;
 public:
    STrackSrc(STrackRtxTimerScanner& rtx_scanner, STrackLogger* logger, TrafficLogger* pktlogger, EventList &eventlist);
    void log(STrackLogger::STrackEvent event);
    virtual void connect(const Route& routeout, const Route& routeback, 
                         STrackSink& sink, simtime_picosec startTime);
    void startflow();
    void doNextEvent();
    //void update_dsn_ack(STrackAck::seq_t ds_ackno);

    void set_flowsize(uint64_t flow_size_in_bytes) {
        _flow_size = flow_size_in_bytes + mss();
        cout << "Setting flow size to " << _flow_size << endl;
    }

    void set_stoptime(simtime_picosec stop_time) {
        _stop_time = stop_time;
        cout << "Setting stop time to " << timeAsSec(_stop_time) << endl;
    }

    bool more_data_available() const;


    bool check_stoptime();
    void set_cwnd(uint32_t cwnd);
    void set_hdiv(double hdiv);

    void set_paths(vector<const Route*>* rt);
    void permute_paths();

    // should really be private, but loggers want to see:
    uint64_t _flow_size;
    simtime_picosec _stop_time;
    bool _stopped;
    uint32_t _maxcwnd;
    uint32_t _strack_cwnd;  // congestion window controlled by strack algorithm
    int32_t _app_limited;
    uint64_t _highest_sent;  //seqno is in bytes
    uint64_t _last_acked; // ack number of the last packet we received a cumulative ack for
    simtime_picosec _RFC2988_RTO_timeout;

    // stuff needed for reno-like fast recovery
    uint32_t _inflate; // how much we're currently extending cwnd based off dup ack arrivals.
    uint64_t _recoverq;
    bool _in_fast_recovery;

    // state needed for strack congestion control
    simtime_picosec _base_rtt; // for measuring achieved BDP
    void set_base_rtt(simtime_picosec base_rtt) {_base_rtt = base_rtt;}
    uint32_t _rx_count; // amount received in a base_rtt
    uint32_t _achieved_BDP; // how much we managed to get to the receiver last RTT
    simtime_picosec _last_BDP_update; // the last time we updated achieved_BDP
    simtime_picosec _last_active;  // the last time we received an ACK
    
    double _alpha;  // increase constant.  
    double _beta;   // decrease constant
    double _max_mdf; // max multiplicate decrease factor
    double _h;      // multi-hop scaling constant
    simtime_picosec _base_delay;    // configured base target delay
    uint32_t _rtx_reset_threshold; // constant
    uint32_t _min_cwnd;  // minimum cwnd we can use
    uint32_t _max_cwnd;  // maximum cwnd we can use

    // flow scaling constants
    double _fs_alpha;
    double _fs_beta;  // I think this beta is different from Algorithm 1 beta
    simtime_picosec _fs_range;
    double _fs_min_cwnd;  // note: in packets
    double _fs_max_cwnd;  // note: in packets

    // paths for PLB or MPSTrack
    vector<const Route*> _paths;

    STrackSink* _sink;
    void set_app_limit(int pktps);

    // STrack helper functions
    simtime_picosec targetDelay(const Route& route);

    int queuesize(int flow_id);

    virtual const string& nodename() { return _nodename; }
    void connect(STrackSink& sink, const Route& routeout, const Route& routeback, uint32_t flow_id, BaseScheduler* scheduler);
    virtual void receivePacket(Packet& pkt);
    void update_rtt(simtime_picosec delay);
    void adjust_cwnd(simtime_picosec delay, STrackAck::seq_t ackno);
    void applySTrackLimits();
    void handle_ack(STrackAck::seq_t ackno);
    void move_path();
    void reroute(const Route &route);
    void rtx_timer_hook(simtime_picosec now, simtime_picosec period);
    inline simtime_picosec pacing_delay() const {return _pacing_delay;}
    PacketFlow& flow() {return _flow;}
    uint32_t drops() { return _drops;}
    bool send_next_packet();
    void send_callback();  // called by scheduler when it has more space
protected:
    // connection state
    bool _established;


    //round trip time estimate
    simtime_picosec _rtt, _rto, _min_rto, _mdev;


    // remember the default cwnd
    static uint32_t _default_cwnd;
    uint32_t _prev_cwnd;
    uint64_t _packets_sent;
    uint16_t _dupacks;
    uint32_t _retransmit_cnt;

    bool _can_decrease;  // limit backoff to once per RTT
    simtime_picosec _last_decrease; //when we last decreased
    simtime_picosec _pacing_delay;  // inter-packet pacing when cwnd < 1 pkt.

    uint32_t _drops;
    bool _rtx_timeout_pending;

    // Connectivity
    PacketFlow _flow;
    uint32_t _path_index;
    const Route* _route;
    STrackPacer _pacer;

private:
    int send_packets();
    void retransmit_packet();
    uint16_t _mss;
    inline uint16_t mss() const {return _mss;}
    inline double beta() const {return _beta;}
    inline double max_mdf() const {return _max_mdf;}
    bool _deferred_send;  // set if we tried to send and the scheduler said no.
    simtime_picosec _plb_interval;
    string _nodename;

    // Housekeeping
    STrackLogger* _logger;
    TrafficLogger* _traffic_logger;
    BaseScheduler* _scheduler;
    STrackRtxTimerScanner* _rtx_timer_scanner;

    // Mechanism
    void clear_timer(uint64_t start,uint64_t end);

};

/**********************************************************************************/
/** SINK                                                                         **/
/**********************************************************************************/

class STrackSink : public PacketSink, public DataReceiver {
    friend class STrackSrc;
 public:
    STrackSink();

    void add_buffer_logger(ReorderBufferLogger *logger) {
        _buffer_logger = logger;
    }

    void receivePacket(Packet& pkt);
    virtual const string& nodename() { return _nodename; }

    STrackSrc* _src;
    uint64_t cumulative_ack();

    // cumulatively acked
    inline uint64_t total_received() const {return _total_received;}
    uint32_t drops(); // sender measures this due to reordering

    uint64_t reorder_buffer_size() {return _received.size();};
    uint64_t reorder_buffer_max() {return _ooo;};
 private:
    // Connectivity
    void connect(STrackSrc& src, const Route& route);
    const Route* _route;

    // Mechanism
    void send_ack(simtime_picosec ts);

    list<STrackAck::seq_t> _received; /* list of packets above a hole, that 
                                      we've received */
    uint64_t _ooo; // out of order max
    uint64_t _total_received;
    string _nodename;
    STrackAck::seq_t _cumulative_ack; // seqno of the last byte in the packet we have
                                      // cumulatively acked
    ReorderBufferLogger* _buffer_logger;
};

class STrackRtxTimerScanner : public EventSource {
 public:
    STrackRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist);
    void doNextEvent();
    void registerSrc(STrackSrc &src);
 private:
    simtime_picosec _scanPeriod;
    typedef list<STrackSrc*> srcs_t;
    srcs_t _srcs;
};

#endif
