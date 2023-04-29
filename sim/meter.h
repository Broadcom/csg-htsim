// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef METER_H
#define METER_H

/*
 * A simple Meter that returns RED or GREEN depending on whether the current packet fits in the meter or not. If it does
 * the packet is considered enqueued.
 */

#include <list>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"

typedef enum {
      METER_RED,
      METER_GREEN
} meter_output;

class Meter : public EventSource {
 public:
    Meter(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist);

    meter_output execute(Packet& pkt);
    void doNextEvent();

    // should really be private, but loggers want to see
    mem_b _maxsize; 
    inline simtime_picosec drainTime(Packet *pkt) { 
        return (simtime_picosec)(pkt->size() * _ps_per_byte); 
    }
    inline simtime_picosec drainTime(int size) { 
        return (simtime_picosec)(size * _ps_per_byte); 
    }
    inline mem_b serviceCapacity(simtime_picosec t) { 
        return (mem_b)(timeAsSec(t) * (double)_bitrate); 
    }

    inline void trimPesiTime(int N) { _endpesitrim = eventlist().now() + N * drainTime(Packet::data_packet_size());_endsemipesitrim = eventlist().now() + 4 * N * drainTime(Packet::data_packet_size());_last_update = eventlist().now(); _credit = 0;}
    
    virtual mem_b queuesize();
    simtime_picosec serviceTime();

    virtual void setName(const string& name) {
        Logged::setName(name); 
        _nodename += name;
    }
    virtual const string& nodename() { return _nodename; }

 protected:
    // Housekeeping
    // start serving the item at the head of the queue
    virtual void beginService(); 

    // wrap up serving the item at the head of the queue
    virtual void completeService(); 

    linkspeed_bps _bitrate; 
    simtime_picosec _ps_per_byte;  // service time, in picoseconds per byte
    mem_b _queuesize;
    mem_b _credit;
    simtime_picosec _last_update;
    
    list<simtime_picosec> _enqueued;
    string _nodename;
    uint16_t _rr;
    simtime_picosec _endpesitrim,_endsemipesitrim;
};

#endif
