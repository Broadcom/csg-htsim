// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        

/*
    Implements a basic queue as described in the Aeolus paper and supported by most existing switches.

    Queue behaviour:
    - high priority packets go into a higher priority queue. This is mainly for control packets. maxsize bytes can be buffered by this queue before packets are dropped.
    - all other packets (low or medium priority) go into the low priority queue.
    - medium priority packets are admitted as long as _queuesize_low < maxsize.
    - low priority packets (or speculative packets) are admitted as long as _queuesize_low < speculative_threshold (which must be smaller than maxsize for this to work properly).

    - the low priority queue also marks ECN on egress based on the queuesize and ECN thresholds low and high. Default config has ECN off.
*/

#ifndef AEOLUS_QUEUE_H
#define AEOLUS_QUEUE_H

#define QUEUE_INVALID 0
#define QUEUE_LOW 1
#define QUEUE_HIGH 2


#include <list>
#include "queue.h"
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"

class AeolusQueue : public Queue {
 public:
    AeolusQueue(linkspeed_bps bitrate, mem_b maxsize, mem_b specsize,
                   EventList &eventlist, QueueLogger* logger);

    virtual void receivePacket(Packet& pkt);
    virtual void doNextEvent();
    // should really be private, but loggers want to see
    mem_b _queuesize_low,_queuesize_high;
    int num_prio_packets() const { return _num_prio_packets;}
    int num_packets() const { return _num_packets;}
    int num_speculative_packets() const { return _num_speculative_packets;}
    virtual mem_b queuesize() const;
    virtual void setName(const string& name) {
        Logged::setName(name); 
        _nodename += name;
    }

    virtual const string& nodename() { return _nodename; }
    void set_ecn_threshold(mem_b ecn_thresh) {
        _ecn_minthresh = ecn_thresh;
        _ecn_maxthresh = ecn_thresh;
    }
    void set_ecn_thresholds(mem_b min_thresh, mem_b max_thresh) {
        _ecn_minthresh = min_thresh;
        _ecn_maxthresh = max_thresh;
    }

    void set_speculative_threshold(mem_b spec_thresh) {
        _speculative_thresh = spec_thresh;
    }

    int _num_packets, _num_prio_packets, _num_speculative_packets;

 protected:
    // Mechanism
    void beginService(); // start serving the item at the head of the queue
    void completeService(); // wrap up serving the item at the head of the queue
    bool decide_ECN();

    int _serv;
    int _ratio_high, _ratio_low, _crt;
    // below minthresh, 0% marking, between minthresh and maxthresh
    // increasing random mark propbability, abve maxthresh, 100%
    // marking.
    mem_b _ecn_minthresh; 
    mem_b _ecn_maxthresh;
    mem_b _speculative_thresh;

    CircularBuffer<Packet*> _enqueued_low;
    CircularBuffer<Packet*> _enqueued_high;
};

#endif
