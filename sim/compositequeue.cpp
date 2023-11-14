// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "compositequeue.h"
#include <math.h>
#include <iostream>
#include <sstream>
#include "ecn.h"

CompositeQueue::CompositeQueue(linkspeed_bps bitrate, mem_b maxsize, EventList& eventlist, 
                               QueueLogger* logger)
    : Queue(bitrate, maxsize, eventlist, logger)
{
    _ratio_high = 100000;
    _ratio_low = 1;
    _crt = 0;
    _num_headers = 0;
    _num_packets = 0;
    _num_acks = 0;
    _num_nacks = 0;
    _num_pulls = 0;
    _num_drops = 0;
    _num_stripped = 0;
    _num_bounced = 0;
    _ecn_minthresh = maxsize*2; // don't set ECN by default
    _ecn_maxthresh = maxsize*2; // don't set ECN by default

    _return_to_sender = false;

    _queuesize_high = _queuesize_low = 0;
    _serv = QUEUE_INVALID;
    stringstream ss;
    ss << "compqueue(" << bitrate/1000000 << "Mb/s," << maxsize << "bytes)";
    _nodename = ss.str();
}

void CompositeQueue::beginService(){
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

bool CompositeQueue::decide_ECN() {
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
CompositeQueue::completeService(){
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
        if (pkt->type() == NDPACK)
            _num_acks++;
        else if (pkt->type() == NDPNACK)
            _num_nacks++;
        else if (pkt->type() == NDPPULL)
            _num_pulls++;
        else {
            //cout << "Hdr: type=" << pkt->type() << endl;
            _num_headers++;
            //ECN mark on deque of a header, if low priority queue is still over threshold
            if (decide_ECN()) {
                pkt->set_flags(pkt->flags() | ECN_CE);
            }
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
CompositeQueue::doNextEvent() {
    completeService();
}

void
CompositeQueue::receivePacket(Packet& pkt)
{
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_ARRIVE);
    if (_logger) _logger->logQueue(*this, QueueLogger::PKT_ARRIVE, pkt);

    if (!pkt.header_only()){
        if (_queuesize_low+pkt.size() <= _maxsize  || drand()<0.5) {
            //regular packet; don't drop the arriving packet

            // we are here because either the queue isn't full or,
            // it might be full and we randomly chose an
            // enqueued packet to trim
            
            if (_queuesize_low+pkt.size()>_maxsize){
                // we're going to drop an existing packet from the queue
                if (_enqueued_low.empty()){
                    //cout << "QUeuesize " << _queuesize_low << " packetsize " << pkt.size() << " maxsize " << _maxsize << endl;
                    assert(0);
                }
                //take last packet from low prio queue, make it a header and place it in the high prio queue
                        
                Packet* booted_pkt = _enqueued_low.pop_front();
                _queuesize_low -= booted_pkt->size();
                if (_logger) _logger->logQueue(*this, QueueLogger::PKT_UNQUEUE, *booted_pkt);

                //cout << "A [ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ] STRIP" << endl;
                //cout << "booted_pkt->size(): " << booted_pkt->size();
                booted_pkt->strip_payload();
                _num_stripped++;
                booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_TRIM);
                if (_logger) _logger->logQueue(*this, QueueLogger::PKT_TRIM, pkt);
                        
                if (_queuesize_high+booted_pkt->size() > 2*_maxsize){
                    if (_return_to_sender && booted_pkt->reverse_route()  && booted_pkt->bounced() == false) {
                        //return the packet to the sender
                        if (_logger) _logger->logQueue(*this, QueueLogger::PKT_BOUNCE, *booted_pkt);
                        booted_pkt->flow().logTraffic(pkt,*this,TrafficLogger::PKT_BOUNCE);
                        //XXX what to do with it now?
#if 0
                        printf("Bounce2 at %s\n", _nodename.c_str());
                        printf("Fwd route:\n");
                        print_route(*(booted_pkt->route()));
                        printf("nexthop: %d\n", booted_pkt->nexthop());
#endif
                        booted_pkt->bounce();
#if 0
                        printf("\nRev route:\n");
                        print_route(*(booted_pkt->reverse_route()));
                        printf("nexthop: %d\n", booted_pkt->nexthop());
#endif
                        _num_bounced++;
                        booted_pkt->sendOn();
                    } else {    
                        booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_DROP);
                        booted_pkt->free();
                        if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
                    }
                }  
                else {
                    _enqueued_high.push(booted_pkt);
                    _queuesize_high += booted_pkt->size();
                    if (_logger) _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, *booted_pkt);
                }
            }

            //assert(_queuesize_low+pkt.size()<= _maxsize);
            Packet* pkt_p = &pkt;
            _enqueued_low.push(pkt_p);
            _queuesize_low += pkt.size();
            if (_logger) _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);
            
            if (_serv==QUEUE_INVALID) {
                beginService();
            }
            
            //cout << "BL[ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ]" << endl;
            
            return;
        } else {
            //strip packet the arriving packet - low priority queue is full
            //cout << "B [ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ] STRIP" << endl;
            pkt.strip_payload();
            _num_stripped++;
            pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_TRIM);
            if (_logger) _logger->logQueue(*this, QueueLogger::PKT_TRIM, pkt);
        }
    }
    assert(pkt.header_only());
    
    if (_queuesize_high+pkt.size() > 2*_maxsize){
        //drop header
        //cout << "drop!\n";
        if (_return_to_sender && pkt.reverse_route()  && pkt.bounced() == false) {
            //return the packet to the sender
            if (_logger) _logger->logQueue(*this, QueueLogger::PKT_BOUNCE, pkt);
            pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_BOUNCE);
            //XXX what to do with it now?
#if 0
            printf("Bounce1 at %s\n", _nodename.c_str());
            printf("Fwd route:\n");
            print_route(*(pkt.route()));
            printf("nexthop: %d\n", pkt.nexthop());
#endif
            pkt.bounce();
#if 0
            printf("\nRev route:\n");
            print_route(*(pkt.reverse_route()));
            printf("nexthop: %d\n", pkt.nexthop());
#endif
            _num_bounced++;
            pkt.sendOn();
            return;
        } else {
            if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
            pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_DROP);
            //cout << "B[ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ] DROP " 
            //     << pkt.flow().get_id() << endl;
            pkt.free();
            _num_drops++;
            return;
        }
    }
    
    
    //if (pkt.type()==NDP)
    //  cout << "H " << pkt.flow().str() << endl;
    Packet* pkt_p = &pkt;
    _enqueued_high.push(pkt_p);
    _queuesize_high += pkt.size();
    if (_logger) _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);
    
    //cout << "BH[ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ]" << endl;
    
    if (_serv==QUEUE_INVALID) {
        beginService();
    }
}

mem_b 
CompositeQueue::queuesize() const {
    return _queuesize_low + _queuesize_high;
}
