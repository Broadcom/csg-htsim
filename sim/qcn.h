// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#ifndef QCN_H
#define QCN_H

#include <list>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "queue.h"

//#ifndef max
#define qmax(a,b) ((a)>(b)?(a):(b))
// gcc stl doesn't like us redefining max, so we'll call it qmax
//#endif

class QcnPacket;
class QcnAck;
class QcnReactor;
class QcnEndpoint;
class QcnQueue;

// QcnPacket and QcnAck are subclasses of Packet.
// They incorporate a packet database, to reuse packet objects that are no longer needed.

class QcnPacket : public Packet {
    friend class QcnAck;
public:
    typedef uint32_t seq_t;
    inline static QcnPacket* newpkt(PacketFlow &flow, route_t &route, routes_t &routesback, PacketSink &reactor, int size, seq_t seqno) {
        QcnPacket* p = _packetdb.allocPacket();
        p->set_route(flow,route,size,seqno);
        p->_seqno = seqno;
        p->_reactor = &reactor;
        p->_routesback = &routesback;
        return p;
    }

    virtual ~QcnPacket(){};
    void free() {_packetdb.freePacket(this);}
    virtual PktPriority priority() const {return Packet::PRIO_NONE;} // change this if you want to use QCN packets with priority queues
    const static int PKTSIZE;
protected:
    routes_t* _routesback;
    seq_t _seqno;
    PacketSink* _reactor;
    static PacketDB<QcnPacket> _packetdb;
};

class QcnAck : public Packet {
    friend class QcnReactor;
public:
    typedef double fb_t;
    inline static QcnAck* newpkt(QcnPacket &datapkt, fb_t fb) {
        QcnAck* p = _packetdb.allocPacket();
        assert(datapkt._nexthop>=1);
        route_t* routeback = (*datapkt._routesback)[datapkt._nexthop-1];
        p->set_route(*datapkt._flow, *routeback, QcnAck::ACK_SIZE, datapkt._seqno);
        p->_reactor = datapkt._reactor;
        p->_fb = fb;
        return p;
    }
    virtual ~QcnAck(){};
    void free() {_packetdb.freePacket(this);}
    virtual PktPriority priority() const {return Packet::PRIO_NONE;}
    const static int ACK_SIZE;
    PacketSink* sendOn() { 
        assert(_nexthop<=_route->size());
        PacketSink* nextsink;
        if (_nexthop==_route->size()) nextsink=_reactor; else nextsink=(*_route).at(_nexthop);
        _nexthop++;
        nextsink->receivePacket(*this);
        return nextsink;
    }
protected:
    static PacketDB<QcnAck> _packetdb;
    PacketSink* _reactor;
    fb_t _fb;
};

class QcnReactor : public PacketSink, public EventSource {
public:
    QcnReactor(QcnLogger* logger, TrafficLogger* pktlogger, EventList &eventlist);
    void connect(route_t& route, routes_t& routesback, simtime_picosec startTime, linkspeed_bps linkspeed);
    void doNextEvent();
    void receivePacket(Packet& pkt);
    const static double GAIN;
    const static int CYCLESOFRECOVERY;
    const static int PACKETSPERRECOVERYCYCLE;
    const static linkspeed_bps RATEINCREASE;
    const static linkspeed_bps MINRATE;
    // State (should be private, but make it public so the loggers can access it)
    int _packetCycles;
    int _timerCycles;
    double _currentRate;
    double _targetRate;
    linkspeed_bps _linkspeed;
    int _packetsSentInCurrentCycle;
private:
    // Housekeeping
    QcnLogger* _logger;
    //TrafficLogger* _pktlogger;
    // Connectivity
    PacketFlow _flow;
    route_t* _route;
    routes_t* _routesback;
    QcnPacket::seq_t _seqno;
    // Mechanism
    void onFeedback(double fb);
    void onPacketSent();
};

class QcnEndpoint : public PacketSink {
    virtual void receivePacket(Packet& pkt);
};

class QcnQueue : public Queue {
public:
    QcnQueue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, QueueLogger* logger, QcnLogger* qcnlogger);
    void receivePacket(Packet& pkt);
    QcnLogger* _qcnlogger;
    int _packetsTillNextFeedback;
    mem_b _lastSampledQueuesize;
    mem_b _targetQueuesize;
    const static double GAMMA;
    // Mechanism
    void onPacketReceived(QcnPacket &pkt);
};

#endif
