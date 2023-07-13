// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "prioqueue.h"
#include <math.h>

#include <iostream>
#include <sstream>

CtrlPrioQueue::CtrlPrioQueue(linkspeed_bps bitrate, mem_b maxsize, EventList& eventlist, 
                               QueueLogger* logger)
  : Queue(bitrate, maxsize, eventlist, logger)
{
  _num_packets = 0;
  _num_acks = 0;
  _num_nacks = 0;
  _num_pulls = 0;
  _num_drops = 0;

  _queuesize_high = _queuesize_low = 0;
  _serv = QUEUE_INVALID;
  stringstream ss;
  ss << "compqueue(" << bitrate/1000000 << "Mb/s," << maxsize << "bytes)";
  _nodename = ss.str();
}

void CtrlPrioQueue::beginService(){
  if (!_enqueued_high.empty()){
    _serv = QUEUE_HIGH;
    eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_high.back()));
  } else if (!_enqueued_low.empty()){
    _serv = QUEUE_LOW;
    eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_low.back()));
  } else {
    assert(0);
    _serv = QUEUE_INVALID;
  }
}

void
CtrlPrioQueue::completeService(){
  Packet* pkt;

  if (_serv==QUEUE_LOW){
    assert(!_enqueued_low.empty());
    pkt = _enqueued_low.back();
    _enqueued_low.pop_back();
    _queuesize_low -= pkt->size();
    _num_packets++;
  } else if (_serv==QUEUE_HIGH) {
    assert(!_enqueued_high.empty());
    pkt = _enqueued_high.back();
    _enqueued_high.pop_back();
    _queuesize_high -= pkt->size();
    switch (pkt->type()) {
    case NDPACK:
    case NDPLITEACK:
        _num_acks++;
        break;
    case NDPNACK:
        _num_nacks++;
        break;
    case NDPPULL:
    case NDPLITEPULL:
        _num_pulls++;
        break;
    default:
        abort(); // add case statements above if this is hit
    }
  } else {
      assert(0);
  }
    
  pkt->flow().logTraffic(*pkt,*this,TrafficLogger::PKT_DEPART);
  if (_logger) _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);
  pkt->sendOn();

  //_virtual_time += drainTime(pkt);

  _serv = QUEUE_INVALID;
  
  //cout << "E[ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ]" << endl;

  if (!_enqueued_high.empty()||!_enqueued_low.empty())
    beginService();
}

void
CtrlPrioQueue::doNextEvent() {
  completeService();
}

CtrlPrioQueue::queue_priority_t 
CtrlPrioQueue::getPriority(Packet& pkt) {
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
CtrlPrioQueue::receivePacket(Packet& pkt)
{
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_ARRIVE);
    queue_priority_t prio = getPriority(pkt);
    mem_b* queuesize = 0;
    list<Packet*>* enqueued = 0;

    switch (prio) {
    case Q_LO:
        enqueued = &_enqueued_low;
        queuesize = &_queuesize_low;
        break;
    case Q_HI:
        enqueued = &_enqueued_high;
        queuesize = &_queuesize_high;
        break;
    default:
        abort();
    }

    if (*queuesize + pkt.size() > _maxsize) {
        Packet* dropped_pkt = 0;
        if (drand() < 0.5) {
            dropped_pkt = &pkt;
            cout << "drop arriving!\n";
        } else {
            cout << "drop last from queue!\n";
            dropped_pkt = enqueued->front();
            enqueued->pop_front();
            *queuesize -= dropped_pkt->size();
            enqueued->push_front(&pkt);
            *queuesize += pkt.size();
        }
        if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, *dropped_pkt);
        switch (pkt.type()) {
        case NDPLITERTS:
            cout << "RTS dropped ";
            break;
        case NDPLITE:
            cout << "Data dropped ";
            break;
        case NDPLITEACK:
            cout << "Ack dropped ";
            break;
        default:
            abort();
        }
        dropped_pkt->flow().logTraffic(*dropped_pkt,*this,TrafficLogger::PKT_DROP);
        cout << "B[ " << _enqueued_low.size() << " " << enqueued->size() << " ] DROP " 
             << dropped_pkt->flow().get_id() << endl;
        dropped_pkt->free();
        _num_drops++;
    } else {
        enqueued->push_front(&pkt);
        *queuesize += pkt.size();
    }
    
    if (_serv==QUEUE_INVALID) {
        beginService();
    }
}

mem_b 
CtrlPrioQueue::queuesize() const {
    return _queuesize_low + _queuesize_high;
}
