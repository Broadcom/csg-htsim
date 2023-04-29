// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "queue_lossless.h"
#include <math.h>
#include <iostream>
#include "switch.h"

LosslessQueue::LosslessQueue(linkspeed_bps bitrate, mem_b maxsize, 
                             EventList& eventlist, QueueLogger* logger, Switch* sw)
    : Queue(bitrate,maxsize,eventlist,logger), 
      _state_send(READY),
      _state_recv(READY)
{
    //assume worst case: PAUSE frame waits for one MSS packet to be sent to other switch, and there is 
    //an MSS just beginning to be sent when PAUSE frame arrives; this means 2 packets per incoming
    //port, and we must have buffering for all ports except this one (assuming no one hop cycles!)

    setSwitch(sw);

    if (sw)
        sw->addPort(this);

    _sending = 0;
    _high_threshold = maxsize;
    _low_threshold = 0;
}


void
LosslessQueue::initThresholds(){
    _high_threshold = _maxsize - (_switch->portCount())*Packet::data_packet_size()*5;

    assert(_high_threshold>0);

    _low_threshold = 2*Packet::data_packet_size();
    assert(_high_threshold > _low_threshold);
}


void
LosslessQueue::receivePacket(Packet& pkt) 
{
    //is this a PAUSE frame? 
    if (pkt.type()==ETH_PAUSE){
        EthPausePacket* p = (EthPausePacket*)&pkt;

        if (p->sleepTime()>0){
            //remote end is telling us to shut up.
            //assert(_state_send == READY);
            if (_sending)
                //we have a packet in flight
                _state_send = PAUSE_RECEIVED;
            else
                _state_send = PAUSED;

            //cout << timeAsMs(eventlist().now()) << " " << _name << " PAUSED "<<endl;            
        }
        else {
            //we are allowed to send!
            _state_send = READY;
            //cout << timeAsMs(eventlist().now()) << " " << _name << " GO "<<endl;

            //start transmission if we have packets to send!
            if(_enqueued.size()>0&&!_sending)
                beginService();
        }
        
        pkt.free();
        return;
    }

    /* normal packet, enqueue it */

    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);
    bool queueWasEmpty = _enqueued.empty();

    Packet* pkt_p = &pkt;
    _enqueued.push(pkt_p);
    _queuesize += pkt.size();

    //send PAUSE notifications if that is the case!
    if (_queuesize > _high_threshold && _state_recv!=PAUSED){
        _state_recv = PAUSED;
        _switch->sendPause(this,1000);
    }

    //if (_state_recv==PAUSED)
    //cout << timeAsMs(eventlist().now()) << " queue " << _name << " switch (" << _switch->_name << ") "<< " recv when paused pkt " << pkt.type() << " sz " << _queuesize << endl;        

    if (_queuesize > _maxsize){
        cout << " Queue " << _name << " switch (" << _switch->nodename() << ") "<< " LOSSLESS not working! I should have dropped this packet" << endl;
    }

    if (_logger) 
        _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);

    if (queueWasEmpty && _state_send == READY) {
        /* schedule the dequeue event */
        assert(_enqueued.size()==1);
        beginService();
    }
}

void LosslessQueue::beginService(){
    assert(_state_send==READY&&!_sending);
    Queue::beginService();
    _sending = 1;
}

void LosslessQueue::completeService(){
    /* dequeue the packet */
    assert(!_enqueued.empty());
    //Packet* pkt = _enqueued.back();
    //_enqueued.pop_back();
    Packet* pkt = _enqueued.pop();
    _queuesize -= pkt->size();
    
    pkt->flow().logTraffic(*pkt, *this, TrafficLogger::PKT_DEPART);

    if (_logger) 
        _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);

    /* tell the packet to move on to the next pipe */
    pkt->sendOn();

    _sending = 0;

    if (_state_send == PAUSE_RECEIVED)
        _state_send = PAUSED;

    //unblock if that is the case
    if (_queuesize < _low_threshold && _state_recv == PAUSED) {
        _switch->sendPause(this,0);
        _state_recv = READY;
    }

    if (!_enqueued.empty()) {
        if (_state_send == READY)
            /* start packet transmission, schedule the next dequeue event */
            beginService();
    }
}

