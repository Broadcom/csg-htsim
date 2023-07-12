// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef SWIFTPACKET_H
#define SWIFTPACKET_H

#include <list>
#include "network.h"



// SwiftPacket and SwiftAck are subclasses of Packet.
// They incorporate a packet database, to reuse packet objects that are no longer needed.
// Note: you never construct a new SwiftPacket or SwiftAck directly; 
// rather you use the static method newpkt() which knows to reuse old packets from the database.

class SwiftPacket : public Packet {
public:
    typedef uint64_t seq_t;

    inline static SwiftPacket* newpkt(PacketFlow &flow, const Route &route, 
                                      seq_t seqno, seq_t dsn, int size) {
        SwiftPacket* p = _packetdb.allocPacket();
        p->set_route(flow,route,size,seqno+size-1); // The Swift sequence number is the first byte of the packet; I will ID the packet by its last byte.
        p->_type = SWIFT;
        p->_seqno = seqno;
        p->_dsn = dsn;
        p->_syn = false;
        return p;
    }

    inline static SwiftPacket* new_syn_pkt(PacketFlow &flow, const Route &route, 
                                           seq_t seqno, int size) {
        SwiftPacket* p = newpkt(flow,route,seqno,0,size);
        p->_syn = true;
        return p;
    }

    void free() {_packetdb.freePacket(this);}
    virtual ~SwiftPacket(){}
    inline seq_t seqno() const {return _seqno;}
    inline seq_t dsn() const {return _dsn;}
    inline simtime_picosec ts() const {return _ts;}
    inline void set_ts(simtime_picosec ts) {_ts = ts;}
    virtual PktPriority priority() const {return Packet::PRIO_LO;}  // change this if you want to use swift with priority queues

protected:
    seq_t _seqno;
    seq_t _dsn;
    bool _syn;
    simtime_picosec _ts;
    static PacketDB<SwiftPacket> _packetdb;
};

class SwiftAck : public Packet {
public:
    typedef SwiftPacket::seq_t seq_t;

    inline static SwiftAck* newpkt(PacketFlow &flow, const Route &route, 
                                   seq_t seqno, seq_t ackno, seq_t ds_ackno,
                                   simtime_picosec ts_echo) {
        SwiftAck* p = _packetdb.allocPacket();
        p->set_route(flow,route,ACKSIZE,ackno);
        p->_type = SWIFTACK;
        p->_seqno = seqno;
        p->_ackno = ackno;
        p->_ds_ackno = ds_ackno;
        p->_ts_echo = ts_echo;
        return p;
    }

    void free() {_packetdb.freePacket(this);}
    inline seq_t seqno() const {return _seqno;}
    inline seq_t ackno() const {return _ackno;}
    inline seq_t ds_ackno() const {return _ds_ackno;}
    inline simtime_picosec ts_echo() const {return _ts_echo;}
    virtual PktPriority priority() const {return Packet::PRIO_HI;}

    virtual ~SwiftAck(){}
    const static int ACKSIZE=40;
protected:
    seq_t _seqno;
    seq_t _ackno;
    seq_t _ds_ackno;
    simtime_picosec _ts_echo;
    static PacketDB<SwiftAck> _packetdb;
};

#endif
