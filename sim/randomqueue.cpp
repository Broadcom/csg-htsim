// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "randomqueue.h"
#include <math.h>
#include <iostream>

RandomQueue::RandomQueue(linkspeed_bps bitrate, mem_b maxsize, 
                         EventList& eventlist, QueueLogger* logger, mem_b drop)
    : Queue(bitrate,maxsize,eventlist,logger), 
      _drop(drop),
      _buffer_drops(0)
{
    //cout << "RandomQueue::_maxsize " << _maxsize << endl;
    _drop_th = _maxsize - _drop;
    _plr = 0.0;
}

void RandomQueue::set_packet_loss_rate(double l){
    _plr = l;
}

void
RandomQueue::receivePacket(Packet& pkt) 
{
    double drop_prob = 0;
    int crt = _queuesize + pkt.size();

    if (_plr > 0.0 && drand() < _plr){
        //if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
        //pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_DROP);
        cout << "Random Drop" << endl;
        pkt.free();
        return;
    }

    if (crt > _drop_th)
        drop_prob = 0.1;
  
    //  cout << "Drop Prob "<<drop_prob<< " queue size "<< _queuesize/1000 << " queue id " << id << endl;

    if (crt > _maxsize || drand() < drop_prob) {
        /* drop the packet */
        if (_logger) _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
        pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
        //cout << "Buffer Drop" << endl;
        if (crt > _maxsize){
            _buffer_drops ++;
        }
        pkt.free();

        //cout << "Drop "<<drop_prob<< " queue size "<< _queuesize/1000 << " queue id " << _name << endl;

        return;
    }

    /* enqueue the packet */
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);
    bool queueWasEmpty = _enqueued.empty();
    Packet* pkt_p = &pkt;
    _enqueued.push(pkt_p);
    _queuesize += pkt.size();

    if (_logger) 
        _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);

    if (queueWasEmpty) {
        /* schedule the dequeue event */
        assert(_enqueued.size()==1);
        beginService();
    }
}
