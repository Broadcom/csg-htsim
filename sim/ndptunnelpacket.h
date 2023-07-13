// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef NDPTUNNELPACKET_H
#define NDPTUNNELPACKET_H

#include <list>
#include "network.h"

// NdpTunnelPacket and NdpAck are subclasses of Packet.
// They incorporate a packet database, to reuse packet objects that are no longer needed.
// Note: you never construct a new NdpTunnelPacket or NdpAck directly; 
// rather you use the static method newpkt() which knows to reuse old packets from the database.

#define ACKSIZE 64
#define VALUE_NOT_SET -1
#define PULL_MAXPATHS 256 // maximum path ID that can be pulled

class NdpTunnelPacket : public Packet {
public:
    typedef uint64_t seq_t;

    // pseudo-constructor for a routeless packet - routing information
    // must be filled in later
    inline static NdpTunnelPacket* newpkt(PacketFlow &flow, 
                                          seq_t seqno, seq_t pacerno, int size, 
                                          bool retransmitted, 
                                          bool last_packet,
                                          Packet * encap) {
        NdpTunnelPacket* p = _packetdb.allocPacket();
        p->set_attrs(flow, size+ACKSIZE, seqno+size-1); // The NDP sequence number is the first byte of the packet; I will ID the packet by its last byte.
        p->_type = NDP;
        p->_is_header = false;
        p->_bounced = false;
        p->_seqno = seqno;
        p->_pacerno = pacerno;
        p->_retransmitted = retransmitted;
        p->_last_packet = last_packet;
        p->_path_len = 0;

        p->_encap_packet = encap;
        encap->inc_ref_count();

        return p;
    }
  
    inline static NdpTunnelPacket* newpkt(PacketFlow &flow, const Route &route, 
                                          seq_t seqno, seq_t pacerno, int size, 
                                          bool retransmitted, int32_t no_of_paths,
                                          bool last_packet,
                                          Packet* encap) {
        NdpTunnelPacket* p = _packetdb.allocPacket();
        p->set_route(flow,route,size+ACKSIZE,seqno+size-1); // The NDP sequence number is the first byte of the packet; I will ID the packet by its last byte.
        p->_type = NDP;
        p->_is_header = false;
        p->_bounced = false;
        p->_seqno = seqno;
        p->_pacerno = pacerno;
        p->_retransmitted = retransmitted;
        p->_no_of_paths = no_of_paths;
        p->_last_packet = last_packet;
        p->_path_len = route.size();

        p->_encap_packet = encap;
        encap->inc_ref_count();
        return p;
    }
  
    virtual inline void  strip_payload() {
        Packet::strip_payload(); _size = ACKSIZE;
    };

    void free() {_encap_packet->free();_packetdb.freePacket(this);}

    void save_state(){
        _oldnexthop = _nexthop;
        _oldsize = _size;
    }

    void load_state(){
        _is_header = false;
        _nexthop = _oldnexthop;
        _size = _oldsize;
    }
    
    virtual ~NdpTunnelPacket(){}
    inline seq_t seqno() const {return _seqno;}
    inline seq_t pacerno() const {return _pacerno;}
    inline void set_pacerno(seq_t pacerno) {_pacerno = pacerno;}
    inline bool retransmitted() const {return _retransmitted;}
    inline bool last_packet() const {return _last_packet;}
    inline simtime_picosec ts() const {return _ts;}
    inline void set_ts(simtime_picosec ts) {_ts = ts;}
    inline int32_t path_id() const {return _route->path_id();}
    inline int32_t no_of_paths() const {return _no_of_paths;}

    inline Packet* inner_packet(){ return _encap_packet;}
    virtual PktPriority priority() const {
        if (_is_header) {
            return Packet::PRIO_HI;
        } else {
            return Packet::PRIO_LO;
        }
    }
    
protected:
    seq_t _seqno;
    seq_t _pacerno;  // the pacer sequence number from the pull, seq space is common to all flows on that pacer
    simtime_picosec _ts;
    bool _retransmitted;
    int32_t _no_of_paths;  // how many paths are in the sender's
    // list.  A real implementation would not
    // send this in every packet, but this is
    // simulation, and this is easiest to
    // implement

    Packet* _encap_packet;
    bool _last_packet;  // set to true in the last packet in a flow.
    
    static PacketDB<NdpTunnelPacket> _packetdb;
};

#endif
