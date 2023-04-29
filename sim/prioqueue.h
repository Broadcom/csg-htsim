// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef CTRL_PRIO_QUEUE_H
#define CTRL_PRIO_QUEUE_H

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

class CtrlPrioQueue : public Queue {
public:
    typedef enum {Q_LO=0, Q_HI=1, Q_NONE=2} queue_priority_t;
    CtrlPrioQueue(linkspeed_bps bitrate, mem_b maxsize, 
                  EventList &eventlist, QueueLogger* logger);
    virtual void receivePacket(Packet& pkt);
    virtual void doNextEvent();
    // should really be private, but loggers want to see
    mem_b _queuesize_low,_queuesize_high;
    int num_packets() const { return _num_packets;}
    int num_acks() const { return _num_acks;}
    int num_pulls() const { return _num_pulls;}
    virtual mem_b queuesize() const;
    virtual void setName(const string& name) {
        Logged::setName(name); 
        _nodename += name;
    }
    virtual const string& nodename() { return _nodename; }

    int _num_packets;
    int _num_acks;
    int _num_nacks;
    int _num_pulls;

protected:
    // Mechanism
    void beginService(); // start serving the item at the head of the queue
    void completeService(); // wrap up serving the item at the head of the queue
    queue_priority_t getPriority(Packet& pkt);

    int _serv;
    int _ratio_high, _ratio_low, _crt;

    list<Packet*> _enqueued_low;
    list<Packet*> _enqueued_high;
};

#endif
