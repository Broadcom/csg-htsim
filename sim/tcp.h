// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#ifndef TCP_H
#define TCP_H

/*
 * A TCP source and sink
 */

#include <list>
#include "config.h"
#include "network.h"
#include "tcppacket.h"
#include "eventlist.h"
#include "sent_packets.h"

//#define MODEL_RECEIVE_WINDOW 1

#define timeInf 0

//#define PACKET_SCATTER 1
//#define RANDOM_PATH 1

//#define MAX_SENT 10000

class TcpSink;
class MultipathTcpSrc;
class MultipathTcpSink;

class TcpSrc : public PacketSink, public EventSource {
    friend class TcpSink;
public:
    TcpSrc(TcpLogger* logger, TrafficLogger* pktlogger, EventList &eventlist);
    virtual void connect(const Route& routeout, const Route& routeback, 
                         TcpSink& sink, simtime_picosec startTime);
    void startflow();
    inline void joinMultipathConnection(MultipathTcpSrc* multipathSrc) {
        _mSrc = multipathSrc;
    };

    void doNextEvent();
    virtual void receivePacket(Packet& pkt);

    void replace_route(const Route* newroute);

    void set_flowsize(uint64_t flow_size_in_bytes) {
        _flow_size = flow_size_in_bytes+_mss;
        cout << "Setting flow size to " << _flow_size << endl;
    }
    flowid_t getFlowId() {return _flow.flow_id();}
    
    void set_ssthresh(uint64_t s){_ssthresh = s;}
    void set_cwnd(uint64_t s){_cwnd = s;}
    void set_dst(int d){_dst=d;}
    int  get_dst(){return _dst;}

    uint32_t effective_window();
    virtual void rtx_timer_hook(simtime_picosec now,simtime_picosec period);
    virtual const string& nodename() { return _nodename; }

    // should really be private, but loggers want to see:
    uint64_t _highest_sent;  //seqno is in bytes
    uint64_t _packets_sent;
    uint64_t _flow_size;
    uint32_t _cwnd;
    uint32_t _maxcwnd;
    uint64_t _last_acked;
    uint32_t _ssthresh;
    uint16_t _dupacks;
#ifdef PACKET_SCATTER
    uint16_t DUPACK_TH;
    uint16_t _crt_path;
#endif

    int32_t _app_limited;

    //round trip time estimate, needed for coupled congestion control
    simtime_picosec _rtt, _rto, _mdev,_base_rtt;
    int _cap;
    simtime_picosec _rtt_avg, _rtt_cum;
    //simtime_picosec when[MAX_SENT];
    int _sawtooth;

    uint16_t _mss;
    uint32_t _unacked; // an estimate of the amount of unacked data WE WANT TO HAVE in the network
    uint32_t _effcwnd; // an estimate of our current transmission rate, expressed as a cwnd
    uint64_t _recoverq;
    bool _in_fast_recovery;

    bool _established;

    uint32_t _drops;

    TcpSink* _sink;
    int _dst{-1};
    MultipathTcpSrc* _mSrc;
    simtime_picosec _RFC2988_RTO_timeout;
    bool _rtx_timeout_pending;

    void set_app_limit(int pktps);

    const Route* _route;
    simtime_picosec _last_ping;
#ifdef PACKET_SCATTER
    vector<const Route*>* _paths;

    void set_paths(vector<const Route*>* rt);
#endif
    void send_packets();

        
#ifdef MODEL_RECEIVE_WINDOW
    SentPackets _sent_packets;
    uint64_t _highest_data_seq;
#endif
    int _subflow_id;

    virtual void inflate_window();
    virtual void deflate_window();

private:
    const Route* _old_route;
    uint64_t _last_packet_with_old_route;

    // Housekeeping
    TcpLogger* _logger;
    //TrafficLogger* _pktlogger;

    // Connectivity
    PacketFlow _flow;

    // Mechanism
    void clear_timer(uint64_t start,uint64_t end);

    void retransmit_packet();
    //simtime_picosec _last_sent_time;

    //void clearWhen(TcpAck::seq_t from, TcpAck::seq_t to);
    //void showWhen (int from, int to);
    string _nodename;
};

class TcpSink : public PacketSink, public DataReceiver {
    friend class TcpSrc;
public:
    TcpSink();
    void set_dst(int d) {_dst=d;}
    int  get_dst() {return _dst;}    
    inline void joinMultipathConnection(MultipathTcpSink* multipathSink){
        _mSink = multipathSink;
    };

    void receivePacket(Packet& pkt);
    TcpAck::seq_t _cumulative_ack; // the packet we have cumulatively acked
    uint64_t _packets;
    uint32_t _drops;
    uint64_t cumulative_ack(){ return _cumulative_ack + _received.size()*1000;}
    uint32_t drops(){ return _src->_drops;}
    virtual const string& nodename() { return _nodename; }

    MultipathTcpSink* _mSink;
    list<TcpAck::seq_t> _received; /* list of packets above a hole, that 
                                      we've received */

#ifdef PACKET_SCATTER
    vector<const Route*>* _paths;

    void set_paths(vector<const Route*>* rt);
#endif

    TcpSrc* _src;
    int _dst{-1};
private:
    // Connectivity
    uint16_t _crt_path;

    void connect(TcpSrc& src, const Route& route);
    const Route* _route;

    // Mechanism
    void send_ack(simtime_picosec ts,bool marked);

    string _nodename;
};

class TcpRtxTimerScanner : public EventSource {
public:
    TcpRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist);
    void doNextEvent();
    void registerTcp(TcpSrc &tcpsrc);
private:
    simtime_picosec _scanPeriod;
    typedef list<TcpSrc*> tcps_t;
    tcps_t _tcps;
};

#endif
