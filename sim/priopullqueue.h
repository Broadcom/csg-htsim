// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#ifndef PRIOQUEUE_H
#define PRIOQUEUE_H

/*
 * A prio queue for pull packets
 */

#include <list>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "circular_buffer.h"
#include "fairpullqueue.h"

template<class PullPkt>
class PrioPullQueue : public BasePullQueue<PullPkt>{
public:
    PrioPullQueue();
    virtual void enqueue(PullPkt& pkt, int priority);
    virtual PullPkt* dequeue();
    virtual void flush_flow(flowid_t flow_id, int priority);
protected:
    //QueueMap _queue_map;  // map flow id to pull queue

    typedef map<flowid_t, CircularBuffer<PullPkt*>*> QueueMap;
    typedef map <int, QueueMap*> PrioMap;
    typedef map <int, int> PrioCounts;
    typedef map <int, typename QueueMap::iterator> CurrentQueueMap;    

    PrioMap _prio_queue_map; // maps priorities to a map of queues at same priority
    CurrentQueueMap _current_queue_map;
    PrioCounts _prio_counts;

    bool queue_exists(const PullPkt& pkt, int priority);
    CircularBuffer<PullPkt*>* find_queue(const PullPkt& pkt, int priority);
    CircularBuffer<PullPkt*>* create_queue(const PullPkt& pkt, int priority);
    void self_check();
    //typename QueueMap::iterator _current_queue;
};

#endif
