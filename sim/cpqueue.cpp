// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "cpqueue.h"
#include <math.h>

#include <iostream>
#include <sstream>

CutPayloadQueue::CutPayloadQueue(linkspeed_bps bitrate, mem_b maxsize, EventList& eventlist, 
                               QueueLogger* logger)
  : Queue(bitrate, maxsize, eventlist, logger)
{
  _num_headers = 0;
  _num_packets = 0;
  _num_acks = 0;
  _num_nacks = 0;
  _num_pulls = 0;
  _num_drops = 0;
  _num_stripped = 0;
  _threshold = _maxsize/2;

  stringstream ss;
  ss << "cpqueue(" << bitrate/1000000 << "Mb/s," << maxsize << "bytes)";
  _nodename = ss.str();
}

void
CutPayloadQueue::completeService(){
  Packet* pkt;
  assert(!_enqueued.empty());
  pkt = _enqueued.back();
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
  Queue::completeService();
}

void
CutPayloadQueue::receivePacket(Packet& pkt)
{
    bool queueWasEmpty = _enqueued.size()==0;

    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_ARRIVE);
    if (_queuesize+pkt.size() > _threshold) {
        //strip packet the arriving packet
        pkt.strip_payload();
        _num_stripped++;
        pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_TRIM);
        if (_logger) _logger->logQueue(*this, QueueLogger::PKT_TRIM, pkt);
    }

    if (_queuesize+pkt.size() > _maxsize) {
        if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
        pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_DROP);
        pkt.free();
        _num_drops++;
        return;
    }

    Packet* pkt_p = &pkt;
    _enqueued.push(pkt_p);
    _queuesize += pkt.size();

    if (_logger) _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);
    
    if (queueWasEmpty)
        beginService();
}


