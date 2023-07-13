// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#ifndef STRACKPACKET_H
#define STRACKPACKET_H

#include <list>
#include "network.h"



// STrackPacket and STrackAck are subclasses of Packet.
// They incorporate a packet database, to reuse packet objects that are no longer needed.
// Note: you never construct a new STrackPacket or STrackAck directly; 
// rather you use the static method newpkt() which knows to reuse old packets from the database.

class STrackPacket : public Packet {
public:
    typedef uint64_t seq_t;

    inline static STrackPacket* newpkt(PacketFlow &flow, const Route &route, 
                                       seq_t seqno, int size) {
        STrackPacket* p = _packetdb.allocPacket();
        p->set_route(flow,route,size,seqno+size-1); // The STrack sequence number is the first byte of the packet; I will ID the packet by its last byte.
        p->_type = STRACK;
        p->_seqno = seqno;
        p->_syn = false;
        return p;
    }

    inline static STrackPacket* new_syn_pkt(PacketFlow &flow, const Route &route, 
                                            seq_t seqno, int size) {
        STrackPacket* p = newpkt(flow,route,seqno,size);
        p->_syn = true;
        return p;
    }

    void free() {_packetdb.freePacket(this);}
    virtual ~STrackPacket(){}
    inline seq_t seqno() const {return _seqno;}
    inline simtime_picosec ts() const {return _ts;}
    inline void set_ts(simtime_picosec ts) {_ts = ts;}
    virtual PktPriority priority() const {return Packet::PRIO_NONE;}
protected:
    seq_t _seqno;
    bool _syn;
    simtime_picosec _ts;
    static PacketDB<STrackPacket> _packetdb;
};

class STrackAck : public Packet {
public:
    typedef STrackPacket::seq_t seq_t;

    inline static STrackAck* newpkt(PacketFlow &flow, const Route &route, 
                                    seq_t seqno, seq_t ackno,
                                    simtime_picosec ts_echo) {
        STrackAck* p = _packetdb.allocPacket();
        p->set_route(flow,route,ACKSIZE,ackno);
        p->_type = STRACKACK;
        p->_seqno = seqno;
        p->_ackno = ackno;
        p->_ts_echo = ts_echo;
        return p;
    }

    void free() {_packetdb.freePacket(this);}
    inline seq_t seqno() const {return _seqno;}
    inline seq_t ackno() const {return _ackno;}
    inline simtime_picosec ts_echo() const {return _ts_echo;}
    virtual PktPriority priority() const {return Packet::PRIO_NONE;}

    virtual ~STrackAck(){}
    const static int ACKSIZE=40;
protected:
    seq_t _seqno;
    seq_t _ackno;

    simtime_picosec _ts_echo;
    static PacketDB<STrackAck> _packetdb;
};

#endif
