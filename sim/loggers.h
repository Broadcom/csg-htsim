// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef LOGGERS_H
#define LOGGERS_H

#include "logfile.h"
#include "network.h"
#include "eventlist.h"
#include "loggertypes.h"
#include "queue.h"
#include "tcp.h"
#include "swift.h"
#include "strack.h"
#include "ndp.h"
#include "roce.h"
#include "hpcc.h"
#include "mtcp.h"
#include "qcn.h"
#include <list>
#include <map>

class FlowEventLoggerSimple : public FlowEventLogger {
public:
    void logEvent(PacketFlow& flow, Logged& location, FlowEvent ev, mem_b bytes, uint64_t pkts);
    static string event_to_str(RawLogEvent& event);
};

class TrafficLoggerSimple : public TrafficLogger {
 public:
    void logTraffic(Packet& pkt, Logged& location, TrafficEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class TcpTrafficLogger : public TrafficLogger {
 public:
    void logTraffic(Packet& pkt, Logged& location, TrafficEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class SwiftTrafficLogger : public TrafficLogger {
 public:
    void logTraffic(Packet& pkt, Logged& location, TrafficEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class STrackTrafficLogger : public TrafficLogger {
 public:
    void logTraffic(Packet& pkt, Logged& location, TrafficEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class NdpTrafficLogger : public TrafficLogger {
 public:
    void logTraffic(Packet& pkt, Logged& location, TrafficEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class RoceTrafficLogger : public TrafficLogger {
 public:
    void logTraffic(Packet& pkt, Logged& location, TrafficEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class HPCCTrafficLogger : public TrafficLogger {
 public:
    void logTraffic(Packet& pkt, Logged& location, TrafficEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class TcpLoggerSimple : public TcpLogger {
 public:
    virtual void logTcp(TcpSrc &tcp, TcpEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class SwiftLoggerSimple : public SwiftLogger {
 public:
    virtual void logSwift(SwiftSubflowSrc &tcp, SwiftEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class STrackLoggerSimple : public STrackLogger {
 public:
    virtual void logSTrack(STrackSrc &tcp, STrackEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class MultipathTcpLoggerSimple: public MultipathTcpLogger {
 public:
    void logMultipathTcp(MultipathTcpSrc& mtcp, MultipathTcpEvent ev);
    static string event_to_str(RawLogEvent& event);
};

// a queue logger manager will create the relevant QueueLogger when
// requested.  This is useful so we don't need to tell every topology
// what type of logging we want right now - just configure the
// QueueLoggerManager, and it will create QueueLoggers when requested
// according to its config.
class QueueLoggerFactory {
public:
    enum QueueLoggerType {LOGGER_SIMPLE, LOGGER_SAMPLING, MULTIQUEUE_SAMPLING, LOGGER_EMPTY};
    QueueLoggerFactory(Logfile* lg, QueueLoggerType logtype, EventList& eventlist);
    QueueLogger *createQueueLogger();
    void set_sample_period(simtime_picosec sample_period) {
        _sample_period = sample_period;
    }
private:
    Logfile* _logfile;
    QueueLoggerType _logger_type;
    simtime_picosec _sample_period;
    EventList& _eventlist;
    vector <QueueLogger*> _loggers;
};

class QueueLoggerSimple : public QueueLogger {
 public:
    virtual void logQueue(BaseQueue& queue, QueueEvent ev, Packet& pkt);
    static string event_to_str(RawLogEvent& event);
};


// QueueLoggerEmpty simply keeps track of the amount of time a queue was busy
class QueueLoggerEmpty : public QueueLogger, public EventSource {
public:
    QueueLoggerEmpty(simtime_picosec period, EventList& eventlist);
    virtual void logQueue(BaseQueue& queue, QueueEvent ev, Packet& pkt);
    void doNextEvent();
    static string event_to_str(RawLogEvent& event);
    void reset_count();
    simtime_picosec _last_transition;
    simtime_picosec _total_busy;
private:
    simtime_picosec _period;
    simtime_picosec _last_dump;
    bool _busy;
    BaseQueue* _queue;
    uint32_t _pkt_arrivals;
    uint32_t _pkt_trims;
};

class QueueLoggerSampling : public QueueLogger, public EventSource {
 public:
    QueueLoggerSampling(simtime_picosec period, EventList& eventlist);
    void logQueue(BaseQueue& queue, QueueEvent ev, Packet& pkt);
    void doNextEvent();
    static string event_to_str(RawLogEvent& event);
 private:
    BaseQueue* _queue;
    simtime_picosec _lastlook;
    simtime_picosec _period;
    mem_b _lastq;
    bool _seenQueueInD;
    mem_b _minQueueInD;
    mem_b _maxQueueInD;
    mem_b _lastDroppedInD;
    mem_b _lastIdledInD;
    int _numIdledInD;
    int _numDropsInD;
    double _cumidle;
    double _cumarr;
    double _cumdrop;
};

class MultiQueueLoggerSampling : public QueueLogger, public EventSource {
 public:
    MultiQueueLoggerSampling(id_t id, simtime_picosec period, EventList& eventlist);
    void logQueue(BaseQueue& queue, QueueEvent ev, Packet& pkt);
    void doNextEvent();
    static string event_to_str(RawLogEvent& event);
 private:
    int _id;
    simtime_picosec _period;
    bool _seenQueueInD;
    mem_b _minQueueInD;
    mem_b _maxQueueInD;
    mem_b _currentQueueSizeBytes;
    int _currentQueueSizePkts;
};

class SinkLoggerSampling : public Logger, public EventSource {
 public:
    SinkLoggerSampling(simtime_picosec period, EventList& eventlist,
                       Logger::EventType sink_type, int _event_type);
    virtual void doNextEvent();
    void monitorSink(DataReceiver* sink);
    void monitorMultipathSink(DataReceiver* sink);
 protected:
    vector<DataReceiver*> _sinks;
    vector<uint32_t> _multipath;

    vector<uint64_t> _last_seq;
    vector<uint32_t> _last_sndbuf;
    vector<double> _last_rate;

    struct lttime
    {
        bool operator()(const MultipathTcpSrc* i1, const MultipathTcpSrc* i2) const
        {
            return i1->get_id() < i2->get_id();
        }
    };
    typedef map<MultipathTcpSrc*,double,lttime> multipath_map;
    multipath_map _multipath_src;
    multipath_map _multipath_seq;
        
    simtime_picosec _last_time;
    simtime_picosec _period;
    Logger::EventType _sink_type;
    int _event_type;
};

class TcpSinkLoggerSampling : public SinkLoggerSampling {
 public:
    TcpSinkLoggerSampling(simtime_picosec period, EventList& eventlist);
    static string event_to_str(RawLogEvent& event);
};

class SwiftSinkLoggerSampling : public SinkLoggerSampling {
 public:
    SwiftSinkLoggerSampling(simtime_picosec period, EventList& eventlist);
    virtual void doNextEvent();
    static string event_to_str(RawLogEvent& event);
private:
    vector<vector<SwiftPacket::seq_t> > _last_sub_seq;
};

class STrackSinkLoggerSampling : public SinkLoggerSampling {
 public:
    STrackSinkLoggerSampling(simtime_picosec period, EventList& eventlist);
    virtual void doNextEvent();
    static string event_to_str(RawLogEvent& event);
private:
    vector<vector<STrackPacket::seq_t> > _last_sub_seq;
};

class NdpSinkLoggerSampling : public SinkLoggerSampling {
    virtual void doNextEvent();
 public:
    NdpSinkLoggerSampling(simtime_picosec period, EventList& eventlist);
    static string event_to_str(RawLogEvent& event);
};

class RoceSinkLoggerSampling : public SinkLoggerSampling {
    virtual void doNextEvent();
 public:
    RoceSinkLoggerSampling(simtime_picosec period, EventList& eventlist);
    static string event_to_str(RawLogEvent& event);
};

class HPCCSinkLoggerSampling : public SinkLoggerSampling {
    virtual void doNextEvent();
 public:
    HPCCSinkLoggerSampling(simtime_picosec period, EventList& eventlist);
    static string event_to_str(RawLogEvent& event);
};


class MemoryLoggerSampling : public Logger, public EventSource {
 public:
    MemoryLoggerSampling(simtime_picosec period, EventList& eventlist);
    void doNextEvent();
    void monitorTcpSink(TcpSink* sink);
    void monitorTcpSource(TcpSrc* sink);
    void monitorMultipathTcpSink(MultipathTcpSink* sink);
    void monitorMultipathTcpSource(MultipathTcpSrc* sink);
    static string event_to_str(RawLogEvent& event);
 private:
    vector<TcpSink*> _tcp_sinks;
    vector<MultipathTcpSink*> _mtcp_sinks;
    vector<TcpSrc*> _tcp_sources;
    vector<MultipathTcpSrc*> _mtcp_sources;

    simtime_picosec _period;
};


class AggregateTcpLogger : public Logger, public EventSource {
 public:
    AggregateTcpLogger(simtime_picosec period, EventList& eventlist);
    void doNextEvent();
    void monitorTcp(TcpSrc& tcp);
    static string event_to_str(RawLogEvent& event);
 private:
    simtime_picosec _period;
    typedef vector<TcpSrc*> tcplist_t;
    tcplist_t _monitoredTcps;
};

class QcnLoggerSimple : public QcnLogger {
 public:
    void logQcn(QcnReactor &src, QcnEvent ev, double var3);

    void logQcnQueue(QcnQueue &src, QcnQueueEvent ev, double var1, 
                     double var2, double var3);
    static string event_to_str(RawLogEvent& event);
};

// Odd to have a logger be logged, but we otherwise don't have an ID for a group of sinks
class ReorderBufferLoggerSampling: public ReorderBufferLogger, public EventSource {
public:
    ReorderBufferLoggerSampling(simtime_picosec period, EventList& eventlist);
    void doNextEvent();
    void logBuffer(BufferEvent ev);
private:
    simtime_picosec _period;
    int _queue_len;
    int _min_queue;
    int _max_queue;
};

#endif
