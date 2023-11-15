// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef LOGGERTYPES_H
#define LOGGERTYPES_H
#include <vector>

class Packet;
class PacketFlow;
class TcpSrc;
class NdpSrc;
class SwiftSrc;
class SwiftSubflowSrc;
class STrackSrc;
class NdpTunnelSrc;
class NdpLiteSrc;
class BaseQueue;
class QcnReactor;
class RoceSrc;
class HPCCSrc;
class QcnQueue;
class MultipathTcpSrc;
class Logfile;
class RawLogEvent;
class Logged;

// keep track of all logged items so we can do ID->Name mapping later
class LoggedManager {
public:
    LoggedManager();
    void add_logged(Logged* logged);
    void dump_idmap();
private:
    vector<Logged*> _idmap;
};

class Logged {
 public:
    typedef uint32_t id_t;
    Logged(const string& name) {_name=name; _log_id=LASTIDNUM; Logged::LASTIDNUM++; _logged_manager.add_logged(this);}
    virtual ~Logged() {}
    virtual void setName(const string& name) { _name=name; }
    virtual const string& str() { return _name; };
    inline id_t get_id() const {return _log_id;}
    // usually things get their own IDs, but flows, for example, get associated with the sender ID
    void set_id(id_t id) {assert(id < LASTIDNUM); _log_id = id;}
    string _name;
    static void dump_idmap() {_logged_manager.dump_idmap();}
 private:
    id_t _log_id;
    static id_t LASTIDNUM;
    static LoggedManager _logged_manager;
};

class Logger {
    friend class Logfile;
 public:
    enum EventType { QUEUE_EVENT=0, TCP_EVENT=1, TCP_STATE=2, TRAFFIC_EVENT=3, 
                     QUEUE_RECORD=4, QUEUE_APPROX=5, TCP_RECORD=6, 
                     QCN_EVENT=7, QCNQUEUE_EVENT=8, 
                     TCP_TRAFFIC=9, NDP_TRAFFIC=10, 
                     TCP_SINK = 11, MTCP = 12, ENERGY = 13, 
                     TCP_MEMORY = 14, NDP_EVENT=15, NDP_STATE=16, NDP_RECORD=17, 
                     NDP_SINK = 18, NDP_MEMORY = 19,
                     SWIFT_EVENT=20, SWIFT_STATE=21, SWIFT_TRAFFIC=22,
                     SWIFT_SINK=23, SWIFT_MEMORY=24,
                     ROCE_TRAFFIC=25,ROCE_SINK=26,
                     HPCC_TRAFFIC=27,HPCC_SINK=28,
                     STRACK_EVENT=29, STRACK_STATE=30, STRACK_TRAFFIC=31,
                     STRACK_SINK=32, STRACK_MEMORY=33,
                     EQDS_EVENT=38, EQDS_STATE=39, EQDS_RECORD=40,
                     EQDS_SINK = 41, EQDS_MEMORY = 42, EQDS_TRAFFIC = 43,
                     FLOW_EVENT = 44 };
    static string event_to_str(RawLogEvent& event);
    Logger() {};
    virtual ~Logger(){};
 protected:
    void setLogfile(Logfile& logfile) { _logfile=&logfile; }
    Logfile* _logfile;
};

class FlowEventLogger: public Logger {
public:
    enum FlowEvent {START = 0, FINISH = 1};
    virtual void logEvent(PacketFlow& flow, Logged& location, FlowEvent ev, mem_b bytes, uint64_t pkts) =0;
    virtual ~FlowEventLogger(){};
};

class TrafficLogger : public Logger {
 public:
    enum TrafficEvent { PKT_ARRIVE=0, PKT_DEPART=1, PKT_CREATESEND=2, PKT_DROP=3, PKT_RCVDESTROY=4, PKT_CREATE=5, PKT_SEND=6, PKT_TRIM=7, PKT_BOUNCE=8 };
    virtual void logTraffic(Packet& pkt, Logged& location, TrafficEvent ev) =0;
    virtual ~TrafficLogger(){};
};

class QueueLogger : public Logger  {
 public:
    enum QueueEvent { PKT_ENQUEUE=0, PKT_DROP=1, PKT_SERVICE=2, PKT_TRIM=3, PKT_BOUNCE=4, PKT_UNQUEUE=5, PKT_ARRIVE=6 };
    enum QueueRecord { CUM_TRAFFIC=0 };
    enum QueueApprox { QUEUE_RANGE=0, QUEUE_OVERFLOW=1 };
    virtual void logQueue(BaseQueue& queue, QueueEvent ev, Packet& pkt) = 0;
    virtual ~QueueLogger(){};
};

class MultipathTcpLogger  : public Logger {
 public:
    enum MultipathTcpEvent { CHANGE_A=0, RTT_UPDATE=1, WINDOW_UPDATE=2, RATE=3, MEMORY=4 };

    virtual void logMultipathTcp(MultipathTcpSrc &src, MultipathTcpEvent ev) =0;
    virtual ~MultipathTcpLogger(){};
};

class EnergyLogger  : public Logger {
 public:
    enum EnergyEvent { DRAW=0 };

    virtual ~EnergyLogger(){};
};

class TcpLogger  : public Logger {
 public:
    enum TcpEvent { TCP_RCV=0, TCP_RCV_FR_END=1, TCP_RCV_FR=2, TCP_RCV_DUP_FR=3, 
                    TCP_RCV_DUP=4, TCP_RCV_3DUPNOFR=5, 
                    TCP_RCV_DUP_FASTXMIT=6, TCP_TIMEOUT=7 };
    enum TcpState { TCPSTATE_CNTRL=0, TCPSTATE_SEQ=1 };
    enum TcpRecord { AVE_CWND=0 };
    enum TcpSinkRecord { RATE = 0 };
    enum TcpMemoryRecord  {MEMORY = 0};

    virtual void logTcp(TcpSrc &src, TcpEvent ev) =0;
    virtual ~TcpLogger(){};
};

class SwiftLogger  : public Logger {
 public:
    enum SwiftEvent { SWIFT_RCV=0, SWIFT_RCV_FR_END=1, SWIFT_RCV_FR=2, SWIFT_RCV_DUP_FR=3,
                    SWIFT_RCV_DUP=4, SWIFT_RCV_3DUPNOFR=5,
                    SWIFT_RCV_DUP_FASTXMIT=6, SWIFT_TIMEOUT=7 };
    enum SwiftState { SWIFTSTATE_CNTRL=0, SWIFTSTATE_SEQ=1 };
    enum SwiftRecord { AVE_CWND=0 };
    enum SwiftSinkRecord { RATE = 0 };
    enum SwiftMemoryRecord  {MEMORY = 0};

    virtual void logSwift(SwiftSubflowSrc &src, SwiftEvent ev) =0;
    virtual ~SwiftLogger(){};
};

class STrackLogger  : public Logger {
 public:
    enum STrackEvent { STRACK_RCV=0, STRACK_RCV_FR_END=1, STRACK_RCV_FR=2, STRACK_RCV_DUP_FR=3, 
                    STRACK_RCV_DUP=4, STRACK_RCV_3DUPNOFR=5, 
                    STRACK_RCV_DUP_FASTXMIT=6, STRACK_TIMEOUT=7 };
    enum STrackState { STRACKSTATE_CNTRL=0, STRACKSTATE_SEQ=1 };
    enum STrackRecord { AVE_CWND=0 };
    enum STrackSinkRecord { RATE = 0 };
    enum STrackMemoryRecord  {MEMORY = 0};

    virtual void logSTrack(STrackSrc &src, STrackEvent ev) =0;
    virtual ~STrackLogger(){};
};

class NdpLogger  : public Logger {
 public:
    enum NdpEvent { NDP_RCV=0, NDP_RCV_FR_END=1, NDP_RCV_FR=2, NDP_RCV_DUP_FR=3, 
                    NDP_RCV_DUP=4, NDP_RCV_3DUPNOFR=5, 
                    NDP_RCV_DUP_FASTXMIT=6, NDP_TIMEOUT=7 };
    enum NdpState { NDPSTATE_CNTRL=0, NDPSTATE_SEQ=1 };
    enum NdpRecord { AVE_CWND=0 };
    enum NdpSinkRecord { RATE = 0 };
    enum NdpMemoryRecord  {MEMORY = 0};

    virtual void logNdp(NdpSrc &src, NdpEvent ev) =0;
    virtual ~NdpLogger(){};
};

class RoceLogger  : public Logger {
 public:
    enum RoceEvent { ROCE_RCV=0, ROCE_TIMEOUT=1 };
    enum RoceState { ROCESTATE_ON=0, ROCESTATE_OFF=0 };
    enum RoceRecord { AVE_RATE=0 };
    enum RoceSinkRecord { RATE = 0 };
    enum RoceMemoryRecord  {MEMORY = 0};

    virtual void logRoce(RoceSrc &src, RoceEvent ev) =0;
    virtual ~RoceLogger(){};
};

class HPCCLogger  : public Logger {
 public:
    enum HPCCEvent { HPCC_RCV=0, HPCC_TIMEOUT=1 };
    enum HPCCState { HPCCSTATE_ON=1, HPCCSTATE_OFF=0 };
    enum HPCCRecord { AVE_RATE=0 };
    enum HPCCSinkRecord { RATE = 0 };
    enum HPCCMemoryRecord  {MEMORY = 0};

    virtual void logHPCC(HPCCSrc &src, HPCCEvent ev) =0;
    virtual ~HPCCLogger(){};
};

class NdpTunnelLogger  : public Logger {
 public:
    enum NdpEvent { NDP_RCV=0, NDP_RCV_FR_END=1, NDP_RCV_FR=2, NDP_RCV_DUP_FR=3, 
                    NDP_RCV_DUP=4, NDP_RCV_3DUPNOFR=5, 
                    NDP_RCV_DUP_FASTXMIT=6, NDP_TIMEOUT=7 };
    enum NdpState { NDPSTATE_CNTRL=0, NDPSTATE_SEQ=1 };
    enum NdpRecord { AVE_CWND=0 };
    enum NdpSinkRecord { RATE = 0 };
    enum NdpMemoryRecord  {MEMORY = 0};

    virtual void logNdp(NdpTunnelSrc &src, NdpEvent ev) =0;
    virtual ~NdpTunnelLogger(){};
};

class NdpLiteLogger  : public Logger {
 public:
    enum NdpLiteEvent { NDP_RCV=0, NDP_RCV_FR_END=1, NDP_RCV_FR=2, NDP_RCV_DUP_FR=3, 
                    NDP_RCV_DUP=4, NDP_RCV_3DUPNOFR=5, 
                    NDP_RCV_DUP_FASTXMIT=6, NDP_TIMEOUT=7 };
    enum NdpLiteState { NDPSTATE_CNTRL=0, NDPSTATE_SEQ=1 };
    enum NdpLiteRecord { AVE_CWND=0 };
    enum NdpLiteSinkRecord { RATE = 0 };
    enum NdpLiteMemoryRecord  {MEMORY = 0};

    virtual void logNdpLite(NdpLiteSrc &src, NdpLiteEvent ev) =0;
    virtual ~NdpLiteLogger(){};
};

class QcnLogger  : public Logger {
 public:
    enum QcnEvent { QCN_SEND=0, QCN_INC=1, QCN_DEC=2, QCN_INCD=3, QCN_DECD=4 };
    enum QcnQueueEvent { QCN_FB=0, QCN_NOFB=1 };
    virtual void logQcn(QcnReactor &src, QcnEvent ev, double var3) =0;
    virtual void logQcnQueue(QcnQueue &src, QcnQueueEvent ev, double var1, double var2, double var3) =0;
    virtual ~QcnLogger(){};
};

class ReorderBufferLogger : public Logger {
public:
    enum BufferEvent {BUF_ENQUEUE=0, BUF_DEQUEUE};
    virtual void logBuffer(BufferEvent ev)=0;
};

#endif
