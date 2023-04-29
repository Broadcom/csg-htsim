// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#ifndef SWIFT_H
#define SWIFT_H

/*
 * A Swift source and sink, loosely based off of Tcp
 */

#include <list>
#include <set>
#include "config.h"
#include "network.h"
#include "swiftpacket.h"
#include "eventlist.h"
#include "sent_packets.h"

//#define MODEL_RECEIVE_WINDOW 1

#define timeInf 0

class SwiftSink;
class SwiftSubflowSrc;

class SwiftPacer : public EventSource {
public:
    SwiftPacer(SwiftSubflowSrc& sub, EventList& eventlist);
    bool is_pending() const {return _interpacket_delay > 0;}  // are we pacing?
    void schedule_send(simtime_picosec delay);  // schedule a paced packet "delay" picoseconds after the last packet was sent
    void cancel();     // cancel pacing
    void just_sent();  // called when we've just sent a packet, even if it wasn't paced
    void doNextEvent();
private:
    SwiftSubflowSrc* _sub;
    simtime_picosec _interpacket_delay; // the interpacket delay, or zero if we're not pacing
    simtime_picosec _last_send;  // when the last packet was sent (always set, even when we're not pacing)
    simtime_picosec _next_send;  // when the next scheduled packet should be sent
};

// stuff that is specific to a subflow rather than the whole connection
class SwiftSubflowSrc : public EventSource {
    friend class SwiftSrc;
    friend class SwiftLoggerSimple;
public:
    SwiftSubflowSrc(SwiftSrc& src, TrafficLogger* pktlogger);
    void update_rtt(simtime_picosec delay);
    void adjust_cwnd(simtime_picosec delay, SwiftAck::seq_t ackno);
    void applySwiftLimits();
    void handle_ack(SwiftAck::seq_t ackno);
    void move_path();
    void doNextEvent();
    void rtx_timer_hook(simtime_picosec now, simtime_picosec period);
    void send_next_packet();
    inline simtime_picosec pacing_delay() const {return _pacing_delay;}
    PacketFlow& flow() {return _flow;}
    uint32_t drops() const { return _drops;}
protected:
    // connection state
    bool _established;

    uint64_t _highest_sent;  //seqno is in bytes

    //round trip time estimate
    simtime_picosec _rtt, _rto, _min_rto, _mdev;

    // stuff needed for reno-like fast recovery
    uint32_t _inflate; // how much we're currently extending cwnd based off dup ack arrivals.
    uint64_t _recoverq;
    bool _in_fast_recovery;

    uint32_t _swift_cwnd;  // congestion window controlled by swift algorithm
    uint32_t _prev_cwnd;
    uint64_t _packets_sent;
    uint64_t _last_acked; // ack number of the last packet we received a cumulative ack for
    uint16_t _dupacks;
    uint32_t _retransmit_cnt;

    bool _can_decrease;  // limit backoff to once per RTT
    simtime_picosec _last_decrease; //when we last decreased
    simtime_picosec _pacing_delay;  // inter-packet pacing when cwnd < 1 pkt.
    map <SwiftPacket::seq_t, SwiftPacket::seq_t> _dsn_map;  // map of subflow seqno to data seqno

    // PLB stuff
    int _decrease_count;
    simtime_picosec _last_good_path;

    uint32_t _drops;
    simtime_picosec _RFC2988_RTO_timeout;
    bool _rtx_timeout_pending;

    // Connectivity
    PacketFlow _flow;
    int _path_index;
    const Route* _route;
    SwiftSrc& _src;
    SwiftPacer _pacer;

private:
    void send_packets();
    void retransmit_packet();
    inline EventList& eventlist() const;
    inline uint16_t mss() const;
    inline double ai() const;
    inline double beta() const;
    inline double max_mdf() const;
};

class SwiftSrc : public PacketSink, public EventSource {
    friend class SwiftSink;
    friend class SwiftRtxTimerScanner;
    //friend class SwiftSubflowSrc;
 public:
    SwiftSrc(SwiftLogger* logger, TrafficLogger* pktlogger, EventList &eventlist);
    uint32_t get_id(){ return id;}
    void log(SwiftLogger::SwiftEvent event);
    virtual void connect(const Route& routeout, const Route& routeback, 
			 SwiftSink& sink, simtime_picosec startTime);
    void startflow();

    void doNextEvent();
    virtual void receivePacket(Packet& pkt);

    void set_flowsize(uint64_t flow_size_in_bytes) {
	_flow_size = flow_size_in_bytes + mss();
	cout << "Setting flow size to " << _flow_size << endl;
    }

    void set_stoptime(simtime_picosec stop_time) {
	_stop_time = stop_time;
	cout << "Setting stop time to " << timeAsSec(_stop_time) << endl;
    }

    // cwnd is in bytes
    void set_cwnd(uint32_t cwnd);

    void set_hdiv(double hdiv);

    // add paths for PLB
    void enable_plb() {_plb = true;}
    void set_paths(vector<const Route*>* rt);
    void permute_paths();
    bool _plb;
    inline bool plb() const {return _plb;}

    virtual const string& nodename() { return _nodename; }

    // should really be private, but loggers want to see:
    uint64_t _highest_dsn_sent;  //seqno is in bytes - data sequence number, for MPSwift
    uint64_t _flow_size;
    simtime_picosec _stop_time;
    uint32_t _maxcwnd;
    int32_t _app_limited;
    uint32_t drops() { return _sub->_drops;}

    uint16_t _mss;
    inline uint16_t mss() const {return _mss;}
    inline double ai() const {return _ai;}
    inline double beta() const {return _beta;}
    inline double max_mdf() const {return _max_mdf;}

    // state needed for swift congestion control
    double _ai;  // increase constant.  
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

    // paths for PLB or MPSwift
    vector<const Route*> _paths;

    SwiftSink* _sink;
    void set_app_limit(int pktps);


    // Swift helper functions
    simtime_picosec targetDelay(bool, const Route& route);

private:
    // Housekeeping
    SwiftLogger* _logger;
    //TrafficLogger* _pktlogger;

    // Mechanism
    void clear_timer(uint64_t start,uint64_t end);

    
    SwiftSubflowSrc* _sub;
    string _nodename;
};

class SwiftSink : public PacketSink, public DataReceiver, public Logged {
    friend class SwiftSrc;
 public:
    SwiftSink();

    void add_buffer_logger(ReorderBufferLogger *logger) {
        _buffer_logger = logger;
    }

    void receivePacket(Packet& pkt);
    SwiftAck::seq_t _cumulative_ack; // seqno of the last byte in the packet we have
                                     // cumulatively acked
    SwiftAck::seq_t _cumulative_data_ack; // seqno of the last DSN byte in the packet we have
                                          // cumulatively acked
    uint64_t _packets;
    uint32_t _drops;
    uint32_t drops(){ return _src->drops();}
    uint32_t get_id(){ return id;}
    virtual const string& nodename() { return _nodename; }

    list<SwiftAck::seq_t> _received; /* list of packets above a hole, that 
				      we've received */
    set <SwiftPacket::seq_t> _dsn_received; // use a map because multipath packets will arrive out of order

    uint64_t cumulative_ack() {
	// this is needed by some loggers.  If we ever need it, figure out what it should really return
	return _cumulative_ack + _received.size()*(_src->_mss);
    } 

    SwiftSrc* _src;
 private:
    // Connectivity
    void connect(SwiftSrc& src, const Route& route);
    const Route* _route;

    // Mechanism
    void send_ack(simtime_picosec ts);

    string _nodename;
    ReorderBufferLogger* _buffer_logger;
};

class SwiftRtxTimerScanner : public EventSource {
 public:
    SwiftRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist);
    void doNextEvent();
    void registerSwift(SwiftSrc &subflow_src);
 private:
    simtime_picosec _scanPeriod;
    typedef list<SwiftSubflowSrc*> subflows_t;
    subflows_t _subflows;
};

#endif
