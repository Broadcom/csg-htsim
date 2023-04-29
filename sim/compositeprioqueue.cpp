// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "compositeprioqueue.h"
#include <math.h>

#include <iostream>
#include <sstream>

CompositePrioQueue::CompositePrioQueue(linkspeed_bps bitrate, mem_b maxsize, EventList& eventlist, 
                                       QueueLogger* logger)
    : Queue(bitrate, maxsize, eventlist, logger)
{
    _ratio_high = 10;
    _ratio_low = 1;
    _crt = 0;
    _num_headers = 0;
    _num_packets = 0;
    _num_acks = 0;
    _num_nacks = 0;
    _num_pulls = 0;
    _num_drops = 0;
    _stripped = 0;
    _dropped = 0;

    _enqueued_path_lens.resize(MAX_PATH_LEN+1);
    for (uint32_t i=0; i < MAX_PATH_LEN+1; i++) {
        // zero the counters of which path length packets are queued
        _enqueued_path_lens[i] = 0;
    }
    _max_path_len_queued = 0;

    _queuesize_high = _queuesize_low = 0;
    _serv = QUEUE_INVALID;
    stringstream ss;
    ss << "compqueue(" << bitrate/1000000 << "Mb/s," << maxsize << "bytes)";
    _nodename = ss.str();
}

void CompositePrioQueue::beginService(){
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

void
CompositePrioQueue::completeService(){
    Packet* pkt;

    if (_serv==QUEUE_LOW){
        pkt = dequeue_low_packet();
    } else if (_serv==QUEUE_HIGH) {
        pkt = dequeue_high_packet();
    } else {
        assert(0);
    }
    
    pkt->flow().logTraffic(*pkt,*this,TrafficLogger::PKT_DEPART);
    if (_logger) _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);
    pkt->sendOn();

    _serv = QUEUE_INVALID;
  
    //cout << "E[ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ]" << endl;

    if (!_enqueued_high.empty()||!_enqueued_low.empty())
        beginService();
}

void
CompositePrioQueue::doNextEvent() {
    completeService();
}

void
CompositePrioQueue::receivePacket(Packet& pkt)
{
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_ARRIVE);
    if (!pkt.header_only()){
        if (_queuesize_low+pkt.size() <= _maxsize
            || ((pkt.path_len() == _enqueued_low.front()->path_len()) && drand()<0.5)
            || ((pkt.path_len() < _max_path_len_queued)) ) {
            //regular packet; don't drop the arriving packet

            // we are here because either:
            // 1. the queue isn't full or,
            //
            // 2. it might be full and the arriving packet and the last enqueued
            // packet have equal path length and we randomly chose the
            // enqueued one to trim, or
            //
            // 3. it might be full and the arriving packet has a shorter
            // path length than some enqueued packet.

            if (_queuesize_low+pkt.size()>_maxsize){
                assert(!_enqueued_low.empty());
                // we will take a packet from low prio queue, make it
                //a header and place it in the high prio queue

                if (pkt.path_len() < _max_path_len_queued) {
                    // we're going to drop the low priority packet
                    cout << "Trim1 " << pkt.path_len() << " max " << _max_path_len_queued << "\n";
                    trim_low_priority_packet(pkt.path_len());
                } else {
                    // we're here because the last packet in the queue
                    // had the same path length and the coin-toss came
                    // up tails.  Drop the last packet in the queue.
                    cout << "Trim2 " << pkt.path_len() << " max " << _max_path_len_queued << "\n";
                    trim_low_priority_packet(pkt.path_len()-1);
                }
            }
            
            assert(_queuesize_low+pkt.size()<= _maxsize);
            
            enqueue_packet(pkt);
            if (_logger) _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);
            
            if (_serv==QUEUE_INVALID) {
                beginService();
            }
            
            //cout << "BL[ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ]" << endl;
            
            return;
        } else {
            //strip packet the arriving packet - low priority queue is full
            cout << "B [ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ] STRIP" << endl;
            pkt.strip_payload();
            _stripped++;
            pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_TRIM);
            if (_logger) _logger->logQueue(*this, QueueLogger::PKT_TRIM, pkt);
        }
    }
    assert(pkt.header_only());
    
    if (_queuesize_high+pkt.size() > _maxsize){
        //drop header
        if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
        pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_DROP);
        cout << "D[ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ] DROP " 
             << pkt.flow().get_id() << endl;
        pkt.free();
        _num_drops++;
        return;
    }
    
    
    //if (pkt.type()==NDP)
    //  cout << "H " << pkt.flow().str() << endl;
    
    _enqueued_high.push_front(&pkt);
    _queuesize_high += pkt.size();
    
    //cout << "BH[ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ]" << endl;
    
    if (_serv==QUEUE_INVALID) {
        beginService();
    }
}

void 
CompositePrioQueue::enqueue_packet(Packet& pkt) {
    //add a packet to the low priority queue and update all the bookkeeping
    _enqueued_low.push_front(&pkt);
    _queuesize_low += pkt.size();
    if (_max_path_len_queued < pkt.path_len()) {
        _max_path_len_queued = pkt.path_len();
        if (_max_path_len_seen < _max_path_len_queued) {
            _max_path_len_seen = _max_path_len_queued;
        }
    }
    _enqueued_path_lens[pkt.path_len()]++;
    check_queued();
}

Packet* CompositePrioQueue::dequeue_low_packet() {
    assert(!_enqueued_low.empty());
    Packet *pkt = _enqueued_low.back();
    _enqueued_low.pop_back();
    _queuesize_low -= pkt->size();
    _num_packets++;

    // update all the priority housekeeping
    uint32_t path_len = pkt->path_len();
    assert(_enqueued_path_lens[path_len] > 0);
    _enqueued_path_lens[path_len]--;
    if ((path_len == _max_path_len_queued) && (_enqueued_path_lens[path_len] == 0)) {
        // we just dequeued the last packet with the longest path len
        find_max_path_len_queued();
    }
    check_queued();
    return pkt;
}

Packet* CompositePrioQueue::dequeue_high_packet() {
    assert(!_enqueued_high.empty());
    Packet *pkt = _enqueued_high.back();
    _enqueued_high.pop_back();
    _queuesize_high -= pkt->size();
    if (pkt->type() == NDPACK)
        _num_acks++;
    else if (pkt->type() == NDPNACK)
        _num_nacks++;
    else if (pkt->type() == NDPPULL)
        _num_pulls++;
    else {
        cout << "Hdr: type=" << pkt->type() << endl;
        _num_headers++;
    }
    check_queued();
    return pkt;
}


mem_b 
CompositePrioQueue::queuesize() const {
    return _queuesize_low + _queuesize_high;
}

void 
CompositePrioQueue::find_max_path_len_queued() {
    // we recalculate _max_path_len_queued because we dequeued
    // something that was of that path length, so we don't know if
    // that value is correct anymore.
    _max_path_len_queued = 0;
    if (_enqueued_low.empty())
        return;
    for (int32_t i = _max_path_len_seen; i >= 0; i--) {
        if (_enqueued_path_lens[i] > 0) {
            _max_path_len_queued = i;
            return;
        }
    }
    check_queued();
}

void
CompositePrioQueue::check_queued() {
    uint32_t maxpath = 0, total_queued = 0;
    for (uint32_t i = 0; i < MAX_PATH_LEN; i++) {
        if (_enqueued_path_lens[i] > 0) {
            maxpath = i;
            total_queued += _enqueued_path_lens[i];
        }
    }
    assert(maxpath == _max_path_len_queued);
    assert(total_queued <= 8);
}

void
CompositePrioQueue::trim_low_priority_packet(uint32_t prio) {
    // working from the tail of the queue, we trim the first packet we
    // find with path_len > prio
    assert(prio < _max_path_len_queued);
    list <Packet*>::iterator i;
    int c = -1;
    for (i = _enqueued_low.begin(); i != _enqueued_low.end(); i++) {
        c++;
        // ideally we'd have a faster way to find the packet to trim
        if ((*i)->path_len() > prio) {
            // we've found the packet to trim
            Packet* booted_pkt = *i;
            _enqueued_low.erase(i);
            _queuesize_low -= booted_pkt->size();

            // update all the priority housekeeping
            uint32_t path_len = booted_pkt->path_len();
            assert(_enqueued_path_lens[path_len] > 0);
            _enqueued_path_lens[path_len]--;
            if ((path_len == _max_path_len_queued) 
                && (_enqueued_path_lens[path_len] == 0)) {
                // we just dequeued the last packet with the longest path len
                find_max_path_len_queued();
            }
            check_queued();

            cout << "C [ " << _enqueued_low.size() << " " << _enqueued_high.size() 
                 << " ] STRIP" << endl;
            cout << "Arriving: " << prio << " booted: " << booted_pkt->path_len() << " posn: " << c << endl;
            booted_pkt->strip_payload();
            if (_queuesize_high+booted_pkt->size() > _maxsize){
                // there's no space in the header queue either
                _dropped++;
                booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_DROP);
                booted_pkt->free();
                if (_logger) 
                    _logger->logQueue(*this, QueueLogger::PKT_DROP, *booted_pkt);
            } else {
                _stripped++;
                booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_TRIM);
                _enqueued_high.push_front(booted_pkt);
                _queuesize_high += booted_pkt->size();
                if (_logger) 
                    _logger->logQueue(*this, QueueLogger::PKT_TRIM, *booted_pkt);
            }
            check_queued();
            return;
        }
    }
    // can't get here
    cout << "FAIL!, can't find packet with less than " << prio << "\n";
    for (i = _enqueued_low.begin(); i != _enqueued_low.end(); i++) {
        cout << "pathlen: " << (*i)->path_len() << endl;
    }
    for (uint32_t c = 0; c <= _max_path_len_seen; c++) {
        cout << "len: " << c << " count: " << _enqueued_path_lens[c] << endl;
    }
    abort();
}
