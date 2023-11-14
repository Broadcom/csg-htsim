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
#include "aeolusqueue.h"
#include <math.h>
#include <iostream>
#include <sstream>
#include "ecn.h"
#include "eqdspacket.h"

AeolusQueue::AeolusQueue(linkspeed_bps bitrate, mem_b maxsize, mem_b specsize, EventList& eventlist, QueueLogger* logger)
    : Queue(bitrate, maxsize, eventlist, logger)
{
    _ratio_high = 100000;
    _ratio_low = 1;
    _crt = 0;
    _num_packets = 0;
    _num_speculative_packets = 0;
    _num_prio_packets = 0;
    _num_drops = 0;
    _ecn_minthresh = maxsize*2; // don't set ECN by default
    _ecn_maxthresh = maxsize*2; // don't set ECN by default

    _speculative_thresh = specsize;
    assert (_speculative_thresh <= _maxsize);

    _queuesize_high = _queuesize_low = 0;
    _serv = QUEUE_INVALID;
    stringstream ss;
    ss << "aeolusqueue(" << bitrate/1000000 << "Mb/s," << maxsize << "bytes)";
    _nodename = ss.str();
}

void AeolusQueue::beginService(){
    if (!_enqueued_high.empty()&&!_enqueued_low.empty()){
        _crt++;

        if (_crt >= (_ratio_high+_ratio_low))
            _crt = 0;

        if (_crt< _ratio_high){
            _serv = QUEUE_HIGH;
            eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_high.back()));
        } else {
            assert(_crt < _ratio_high+_ratio_low);
            _serv = QUEUE_LOW;
            eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_low.back()));      
        }
        return;
    }

    if (!_enqueued_high.empty()){
        _serv = QUEUE_HIGH;
        eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_high.back()));
    } else if (!_enqueued_low.empty()){
        _serv = QUEUE_LOW;
        eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_low.back()));
    }
    else {
        assert(0);
        _serv = QUEUE_INVALID;
    }
}

bool AeolusQueue::decide_ECN() {
    //ECN mark on deque
    if (_queuesize_low > _ecn_maxthresh) {
        return true;
    } else if (_queuesize_low > _ecn_minthresh) {
        uint64_t p = (0x7FFFFFFF * (_queuesize_low - _ecn_minthresh))/(_ecn_maxthresh - _ecn_minthresh);
        if ((uint64_t)random() < p) {
            return true;
        }
    }
    return false;
}

void
AeolusQueue::completeService(){
    Packet* pkt;
    if (_serv==QUEUE_LOW){
        assert(!_enqueued_low.empty());
        pkt = _enqueued_low.pop();
        _queuesize_low -= pkt->size();

        //ECN mark on deque
        if (decide_ECN()) {
            pkt->set_flags(pkt->flags() | ECN_CE);
        }
    
        if (_logger) _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);
        _num_packets++;
    } else if (_serv==QUEUE_HIGH) {
        assert(!_enqueued_high.empty());
        pkt = _enqueued_high.pop();
        _queuesize_high -= pkt->size();
        if (_logger) _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);

        _num_prio_packets++;
        //unclear if we should set ECN for high priority packets!
        if (decide_ECN()) {
            pkt->set_flags(pkt->flags() | ECN_CE);
        }
    } else {
        assert(0);
    }
    
    pkt->flow().logTraffic(*pkt,*this,TrafficLogger::PKT_DEPART);
    pkt->sendOn();

    //_virtual_time += drainTime(pkt);
  
    _serv = QUEUE_INVALID;
  
    //cout << "E[ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ]" << endl;

    if (!_enqueued_high.empty()||!_enqueued_low.empty())
        beginService();
}

void
AeolusQueue::doNextEvent() {
    completeService();
}

void 
AeolusQueue::receivePacket(Packet& pkt) {
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_ARRIVE);
    if (_logger) _logger->logQueue(*this, QueueLogger::PKT_ARRIVE, pkt);

    assert(pkt.priority()!=Packet::PRIO_NONE);

    if (pkt.priority() == Packet::PRIO_HI){
        if (_queuesize_high+pkt.size() <= _maxsize) { //admit
            Packet* pkt_p = &pkt;
            _enqueued_high.push(pkt_p);
            _queuesize_high += pkt.size();

            if (_logger) _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);

        } else {             //high priority packet, doesn't fit - drop
            cout << "Aeolus high priority queue " << str() << "size " << _queuesize_high << " dropped packet " << endl;
            pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_DROP);
            if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
            pkt.free();
            return;
        }  
    } else {
        if ( (pkt.priority() == Packet::PRIO_MID && _queuesize_low + pkt.size() < _maxsize) ||
            (pkt.priority() == Packet::PRIO_LO && _queuesize_low + pkt.size() < _speculative_thresh) ){ //admit
            Packet* pkt_p = &pkt;
            _enqueued_low.push(pkt_p);
            _queuesize_low += pkt.size();
            
            if (_logger) _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);
        }
        else {
            EqdsDataPacket* p;
            p = dynamic_cast<EqdsDataPacket*>(&pkt);
            cout << "Aeolus queue " << str() << "size " << _queuesize_low << " dropped packet " << ((pkt.priority()==Packet::PRIO_LO)?"Speculative":"Regular") << " packet " << (p!=NULL?p->epsn():0) << " flow " << pkt.flow().str() << endl;
            pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_DROP);
            if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
            pkt.free();
            return;
        }
    }
        
    if (_serv==QUEUE_INVALID) {
        beginService();
    }
}

mem_b 
AeolusQueue::queuesize() const {
    return _queuesize_low + _queuesize_high;
}
