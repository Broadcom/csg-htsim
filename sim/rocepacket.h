// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef ROCEPACKET_H
#define ROCEPACKET_H

#include <list>
#include "network.h"

// NdpPacket and NdpAck are subclasses of Packet.
// They incorporate a packet database, to reuse packet objects that are no longer needed.
// Note: you never construct a new NdpPacket or NdpAck directly; 
// rather you use the static method newpkt() which knows to reuse old packets from the database.

#define VALUE_NOT_SET -1
//#define PULL_MAXPATHS 256 // maximum path ID that can be pulled

class RocePacket : public Packet {
 public:
    typedef uint64_t seq_t;

    // pseudo-constructor for a routeless packet - routing information
    // must be filled in later
    inline static RocePacket* newpkt(PacketFlow &flow, 
                                    seq_t seqno, int size, 
                                    bool retransmitted, 
                                    bool last_packet,
                                    uint32_t destination = UINT32_MAX) {
                RocePacket* p = _packetdb.allocPacket();
                p->set_attrs(flow, size+ACKSIZE, seqno+size-1); // The NDP sequence number is the first byte of the packet; I will ID the packet by its last byte.
                p->_type = ROCE;
                p->_is_header = false;
                p->_seqno = seqno;
                p->_retransmitted = retransmitted;
                p->_last_packet = last_packet;
                p->_path_len = 0;
                p->_direction = NONE;
                p->set_dst(destination);
                return p;
    }
  
    inline static RocePacket* newpkt(PacketFlow &flow, const Route &route, 
                                    seq_t seqno, int size, 
                                    bool retransmitted,
                                    bool last_packet,
                                    uint32_t destination = UINT32_MAX) {
                RocePacket* p = _packetdb.allocPacket();
                p->set_route(flow,route,size+ACKSIZE,seqno+size-1); // The NDP sequence number is the first byte of the packet; I will ID the packet by its last byte.
                p->_type = ROCE;
                p->_seqno = seqno;
                p->_is_header = false;
                p->_direction = NONE;        
                p->_retransmitted = retransmitted;
                p->_last_packet = last_packet;
                p->_path_len = route.size();
                p->set_dst(destination);
                return p;
    }
  
    void free() {_packetdb.freePacket(this);}
    virtual ~RocePacket(){}
    
        inline seq_t seqno() const {return _seqno;}
    inline bool retransmitted() const {return _retransmitted;}
    inline bool last_packet() const {return _last_packet;}
    inline simtime_picosec ts() const {return _ts;}
    inline void set_ts(simtime_picosec ts) {_ts = ts;}
    inline uint32_t path_id() const {if (_pathid!=UINT32_MAX) return _pathid; else return _route->path_id();}
    virtual PktPriority priority() const {return Packet::PRIO_LO;}
    const static int ACKSIZE=64;
 protected:
    seq_t _seqno;
    simtime_picosec _ts;
    bool _retransmitted;
    bool _last_packet;  // set to true in the last packet in a flow.
    static PacketDB<RocePacket> _packetdb;
};

class RoceAck : public Packet {
 public:
    typedef RocePacket::seq_t seq_t;
  
    inline static RoceAck* newpkt(PacketFlow &flow, const Route &route, 
                                 seq_t ackno,
                                 uint32_t destination = UINT32_MAX) {
                RoceAck* p = _packetdb.allocPacket();
                p->set_route(flow,route,RocePacket::ACKSIZE,ackno);
                p->_type = ROCEACK;
                p->_is_header = true;
                p->_ackno = ackno;
                p->_path_len = 0;
                p->_direction = NONE;
                p->set_dst(destination);
                return p;
    }
  
    void free() {_packetdb.freePacket(this);}
    inline seq_t ackno() const {return _ackno;}
    inline simtime_picosec ts() const {return _ts;}
    inline void set_ts(simtime_picosec ts) {_ts = ts;}
    virtual PktPriority priority() const {return Packet::PRIO_HI;}
  
    virtual ~RoceAck(){}

 protected:
    seq_t _ackno;
    simtime_picosec _ts;
    static PacketDB<RoceAck> _packetdb;
};


class RoceNack : public Packet {
 public:
    typedef RocePacket::seq_t seq_t;
  
    inline static RoceNack* newpkt(PacketFlow &flow, const Route &route, 
                                  seq_t ackno,
                                  uint32_t destination = UINT32_MAX) {
                RoceNack* p = _packetdb.allocPacket();
                p->set_route(flow,route,RocePacket::ACKSIZE,ackno);
                p->_type = ROCENACK;
                p->_is_header = true;
                p->_ackno = ackno;
                p->_direction = NONE;
                p->set_dst(destination);
                return p;
    }
  
    void free() {_packetdb.freePacket(this);}
    inline seq_t ackno() const {return _ackno;}
    inline simtime_picosec ts() const {return _ts;}
    inline void set_ts(simtime_picosec ts) {_ts = ts;}
    virtual PktPriority priority() const {return Packet::PRIO_HI;}
  
    virtual ~RoceNack(){}

 protected:
    seq_t _ackno;
    simtime_picosec _ts;
    static PacketDB<RoceNack> _packetdb;
};


#endif
