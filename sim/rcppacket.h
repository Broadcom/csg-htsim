// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#ifndef RCPPACKET_H
#define RCPPACKET_H

#include <list>
#include "network.h"

// RcpPacket and RcpAck are subclasses of Packet.
// They incorporate a packet database, to reuse packet objects that are no longer needed.
// Note: you never construct a new RcpPacket or RcpAck directly; 
// rather you use the static method newpkt() which knows to reuse old packets from the database.

class RcpPacket : public Packet {
public:
    typedef uint32_t seq_t;

    inline static RcpPacket*
    RcpPacket::newpkt(PacketFlow &flow, route_t &route, 
                      seq_t seqno, int size) {
        RcpPacket* p = _packetdb.allocPacket();
        p->set(flow,route,size,seqno);
        p->_seqno = seqno;
        return p;
    }

    void free() {_packetdb.freePacket(this);}
    inline seq_t seqno() const {return _seqno;}
    const static int DEFAULTDATASIZE=1000; // size of a data packet, measured in bytes; used by RcpSrc
protected:
    seq_t _seqno;
    double _rtt;
    double _rateaccumulator;
    static PacketDB<RcpPacket> _packetdb;
};

class RcpAck : public Packet {
public:
    typedef RcpPacket::seq_t seq_t;

    inline static RcpAck*
    RcpAck::newpkt(PacketFlow &flow, route_t &route, 
                   seq_t seqno, seq_t ackno) {
        RcpAck* p = _packetdb.allocPacket();
        p->set(flow,route,ACKSIZE,ackno);
        p->_seqno = seqno;
        p->_ackno = ackno;
        return p;
    }

    void free() {_packetdb.freePacket(this);}
    inline seq_t seqno() const {return _seqno;}
    inline seq_t ackno() const {return _ackno;}
    const static int ACKSIZE=40;
protected:
    seq_t _seqno;
    seq_t _ackno;
    double _rtt;
    double _rateaccumulator;
    static PacketDB<RcpAck> _packetdb;
};

#endif
