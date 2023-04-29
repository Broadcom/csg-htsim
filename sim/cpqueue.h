// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef CUTPAYLOAD_QUEUE_H
#define CUTPAYLOAD_QUEUE_H

/*
 * A cut payload queue that transforms packets into headers when occupancy grows beyond a specified threshold. 
 */

#include <list>
#include "queue.h"
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"

class CutPayloadQueue : public Queue {
 public:
    CutPayloadQueue(linkspeed_bps bitrate, mem_b maxsize, 
                   EventList &eventlist, QueueLogger* logger);
    virtual void receivePacket(Packet& pkt);
    // should really be private, but loggers want to see
    mem_b _threshold;
    int num_headers() const { return _num_headers;}
    int num_packets() const { return _num_packets;}
    int num_stripped() const { return _num_stripped;}
    int num_acks() const { return _num_acks;}
    int num_nacks() const { return _num_nacks;}
    int num_pulls() const { return _num_pulls;}

    virtual void setName(const string& name) {
        Logged::setName(name); 
        _nodename += name;
    }
    virtual const string& nodename() { return _nodename; }

 protected:
    // Mechanism
    void completeService(); // wrap up serving the item at the head of the queue

    int _num_packets;
    int _num_headers; // only includes data packets stripped to headers, not acks or nacks
    int _num_acks;
    int _num_nacks;
    int _num_pulls;
    int _num_stripped; // count of packets we stripped
};

#endif
