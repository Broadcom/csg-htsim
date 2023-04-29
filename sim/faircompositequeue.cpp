// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "faircompositequeue.h"
#include <math.h>

#include <iostream>
#include <sstream>

FairCompositeQueue::FairCompositeQueue(linkspeed_bps bitrate, mem_b maxsize, EventList& eventlist, 
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
  _num_stripped = 0;
  _num_bounced = 0;

  _queuesize_high = _queuesize_low = 0;
  _serv = QUEUE_INVALID;
  stringstream ss;
  ss << "compqueue(" << bitrate/1000000 << "Mb/s," << maxsize << "bytes)";
  _nodename = ss.str();
}

void FairCompositeQueue::beginService(){
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
FairCompositeQueue::completeService(){
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
    if (pkt->type() == NDPACK)
        _num_acks++;
    else if (pkt->type() == NDPNACK)
        _num_nacks++;
    else if (pkt->type() == NDPPULL)
        _num_pulls++;
    else {
        //cout << "Hdr: type=" << pkt->type() << endl;
        _num_headers++;
    }
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
FairCompositeQueue::doNextEvent() {
  completeService();
}

void
FairCompositeQueue::receivePacket(Packet& pkt)
{
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_ARRIVE);
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
                
                tuple<Packet*,uint64_t> booted_pkt = _enqueued_low.front();
                _enqueued_low.pop_front();
                _queuesize_low -= get<0>(booted_pkt)->size();

                //cout << "A [ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ] STRIP" << endl;
                //cout << "booted_pkt->size(): " << booted_pkt->size();
                booted_pkt->strip_payload();
                _num_stripped++;
                booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_TRIM);
                if (_logger) _logger->logQueue(*this, QueueLogger::PKT_TRIM, pkt);
                
                if (_queuesize_high+booted_pkt->size() > _maxsize){
                    if (booted_pkt->reverse_route()  && booted_pkt->bounced() == false) {
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
                        cout << "Dropped\n";
                        booted_pkt->flow().logTraffic(*booted_pkt,*this,TrafficLogger::PKT_DROP);
                        booted_pkt->free();
                        if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
                    }
                }  
                else {
                    _enqueued_high.push_front(booted_pkt);
                    _queuesize_high += booted_pkt->size();
                }
            }
            
            assert(_queuesize_low+pkt.size()<= _maxsize);
            
            _enqueued_low.push_front(&pkt);
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
    
    if (_queuesize_high+pkt.size() > _maxsize){
        //drop header
        //cout << "drop!\n";
        if (pkt.reverse_route()  && pkt.bounced() == false) {
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
            cout << "B[ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ] DROP " 
                     << pkt.flow().id << endl;
            pkt.free();
            _num_drops++;
            return;
        }
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

mem_b 
FairCompositeQueue::queuesize() {
    return _queuesize_low + _queuesize_high;
}
