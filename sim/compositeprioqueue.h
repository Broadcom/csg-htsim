// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef COMPOSITE_PRIO_QUEUE_H
#define COMPOSITE_PRIO_QUEUE_H

/*
 * A composite queue that transforms packets into headers when there is no space and services headers with priority. 
 */

#define QUEUE_INVALID 0
#define QUEUE_LOW 1
#define QUEUE_HIGH 2


#include <list>
#include "queue.h"
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"

#define MAX_PATH_LEN 20u
class CompositePrioQueue : public Queue {
 public:
    CompositePrioQueue(linkspeed_bps bitrate, mem_b maxsize, 
                   EventList &eventlist, QueueLogger* logger);
    virtual void receivePacket(Packet& pkt);
    virtual void doNextEvent();
    // should really be private, but loggers want to see
    mem_b _queuesize_low,_queuesize_high;
    int num_headers() const { return _num_headers;}
    int num_packets() const { return _num_packets;}
    int num_stripped() const { return _stripped;}
    int num_acks() const { return _num_acks;}
    int num_nacks() const { return _num_nacks;}
    int num_pulls() const { return _num_pulls;}
    virtual mem_b queuesize() const;
    virtual void setName(const string& name) {
        Logged::setName(name); 
        _nodename += name;
    }
    virtual const string& nodename() { return _nodename; }

 protected:
    // Mechanism
    void beginService(); // start serving the item at the head of the queue
    void completeService(); // wrap up serving the item at the head of the queue

    int _serv;
    int _ratio_high, _ratio_low, _crt;
    int _num_packets;
    int _num_headers; // only includes data packets stripped to headers, not acks or nacks
    int _num_acks;
    int _num_nacks;
    int _num_pulls;
    int _stripped;
    int _dropped;

    list<Packet*> _enqueued_low;
    vector <uint32_t> _enqueued_path_lens;
    uint32_t _max_path_len_queued;
    uint32_t _max_path_len_seen;
    list<Packet*> _enqueued_high;
 private:
    void find_max_path_len_queued();
    void trim_low_priority_packet(uint32_t prio);
    void enqueue_packet(Packet& pkt);
    void check_queued();
    Packet* dequeue_low_packet();
    Packet* dequeue_high_packet();
};

#endif
