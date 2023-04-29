// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef FAIRQUEUE_H
#define FAIRQUEUE_H

/*
 * A fair queue for pull packets
 */

#include <list>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "circular_buffer.h"


template<class PullPkt>
class BasePullQueue {
 public:
    BasePullQueue();
    virtual ~BasePullQueue(){};
    virtual void enqueue(PullPkt& pkt, int priority = 0) = 0;
    virtual PullPkt* dequeue() = 0;
    virtual void flush_flow(flowid_t flow_id, int priority = 0) = 0;
    virtual void set_preferred_flow(flowid_t preferred_flow) {
            _preferred_flow = preferred_flow;
    }
    inline int32_t pull_count() const {return _pull_count;}
    inline bool empty() const {return _pull_count == 0;}
    int32_t _pull_count;
 protected:
    int64_t _preferred_flow; // flow_id is uint32_t, int64_t can store this, plus -1
};

template<class PullPkt>
class FifoPullQueue : public BasePullQueue<PullPkt>{
 public:
    FifoPullQueue();
    virtual void enqueue(PullPkt& pkt, int priority = 0);
    virtual PullPkt* dequeue();
    virtual void flush_flow(flowid_t flow_id, int priority = 0);
 protected:
    list <PullPkt*> _pull_queue; // needs insert middle, so can't use circular buffer
};

template<class PullPkt>
class FairPullQueue : public BasePullQueue<PullPkt>{
 public:
    FairPullQueue();
    virtual void enqueue(PullPkt& pkt, int priority = 0);
    virtual PullPkt* dequeue();
    virtual void flush_flow(flowid_t flow_id, int priority = 0);
 protected:
    map<flowid_t, CircularBuffer<PullPkt*>*> _queue_map;  // map flow id to pull queue
    bool queue_exists(const PullPkt& pkt);
    CircularBuffer<PullPkt*>* find_queue(const PullPkt& pkt);
    CircularBuffer<PullPkt*>* create_queue(const PullPkt& pkt);
    typename map<flowid_t, CircularBuffer<PullPkt*>*>::iterator _current_queue;
};

#endif
