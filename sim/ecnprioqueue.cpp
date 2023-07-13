// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "ecnprioqueue.h"
#include "ecn.h"
#include <math.h>
#include <iostream>
#include <sstream>

ECNPrioQueue::ECNPrioQueue(linkspeed_bps bitrate,
                           mem_b maxsize_hi, mem_b maxsize_lo,
                           mem_b ecn_thresh_hi, mem_b ecn_thresh_lo,
                           EventList& eventlist, 
                           QueueLogger* logger)
    : Queue(bitrate, maxsize_hi + maxsize_lo, eventlist, logger)
{
    _num_packets = 0;
    _num_drops = 0;
    _ecn = false;

    _queuesize[Q_LO] = 0;
    _queuesize[Q_HI] = 0;
    _maxsize[Q_LO] = maxsize_lo;
    _maxsize[Q_HI] = maxsize_hi;
    _ecn_thresh[Q_HI] = ecn_thresh_hi;
    _ecn_thresh[Q_LO] = ecn_thresh_lo;
    
    _serv = Q_NONE;
    stringstream ss;
    ss << "ecnprioqueue(" << bitrate/1000000 << "Mb/s," << maxsize_lo << "bytes_L," << maxsize_hi << "bytes_H)";
    _nodename = ss.str();
}

void ECNPrioQueue::beginService(){
    _ecn = false;
    if (!_enqueued[Q_HI].empty()){
        _serv = Q_HI;
        // set ECN bit on start of dequeue
        if (_queuesize[Q_HI] > _ecn_thresh[Q_HI])
            _ecn = true;
        eventlist().sourceIsPendingRel(*this, drainTime(_enqueued[Q_HI].back()));
    } else if (!_enqueued[Q_LO].empty()){
        _serv = Q_LO;
        // set ECN bit on start of dequeue
        if (_queuesize[Q_LO] > _ecn_thresh[Q_LO])
            _ecn = true;
        eventlist().sourceIsPendingRel(*this, drainTime(_enqueued[Q_LO].back()));
    } else {
        assert(0);
        _serv = Q_NONE;
    }
}

void
ECNPrioQueue::completeService(){
    Packet* pkt;

    assert(_serv != Q_NONE);

    pkt = _enqueued[_serv].pop();
    _queuesize[_serv] -= pkt->size();
    _num_packets++;
    
    pkt->flow().logTraffic(*pkt,*this,TrafficLogger::PKT_DEPART);
    if (_logger) _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);

    if (_ecn) {
        pkt->set_flags(pkt->flags() | ECN_CE);        
    }
    pkt->sendOn();

    _serv = Q_NONE;
  
    //cout << "E[ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ]" << endl;

    if (!_enqueued[Q_HI].empty()||!_enqueued[Q_LO].empty())
        beginService();
}

void
ECNPrioQueue::doNextEvent() {
    completeService();
}

ECNPrioQueue::queue_priority_t 
ECNPrioQueue::getPriority(Packet& pkt) {
    Packet::PktPriority pktprio = pkt.priority();
    switch (pktprio) {
    case Packet::PRIO_LO:
        return Q_LO;
    case Packet::PRIO_MID:
        // this queue supports two priorities - if you want three, use
        // a different queue, or change the packet to saw if it wants
        // LO or HI priority
        abort(); 
    case Packet::PRIO_HI:
        return Q_HI;
    case Packet::PRIO_NONE:
        // this packet didn't expect to see a priority queue - change
        // the packet to say what service it wants
        abort(); 
    }
    // can't get here
    abort();
}

void
ECNPrioQueue::receivePacket(Packet& pkt)
{
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_ARRIVE);
    queue_priority_t prio = getPriority(pkt);

    if (_queuesize[prio] + pkt.size() > _maxsize[prio]
        || ( (_queuesize[prio] + 2 * pkt.size() > _maxsize[prio]) && (rand()&0x01))) {
        // this is a droptail queue but drop randomly on the last slot to try and reduce simulator phase effects
        if (_logger) {
            _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
        }
        pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
        cout << "B[ " << _enqueued[Q_LO].size() << " " << _enqueued[Q_HI].size() << " ] DROP " 
             << pkt.flow().get_id() << endl;
        pkt.free();
        _num_drops++;
        return;
    } else {
        Packet* pkt_p = &pkt; // force a non-temporary reference in push
        _enqueued[prio].push(pkt_p);
        _queuesize[prio] += pkt.size();
    }
    
    if (_serv==Q_NONE) {
        beginService();
    }
}

mem_b 
ECNPrioQueue::queuesize() const {
    return _queuesize[Q_LO] + _queuesize[Q_HI];
}
