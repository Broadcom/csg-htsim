// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "pipe.h"
#include <iostream>
#include <sstream>

Pipe::Pipe(simtime_picosec delay, EventList& eventlist)
: EventSource(eventlist,"pipe"), _delay(delay)
{
    _count = 0;
    _next_insert = 0;
    _next_pop = 0;
    _size = 16; // initial size; we'll resize if needed
    _inflight_v.resize(_size);
    stringstream ss;
    ss << "pipe(" << delay/1000000 << "us)";
    _nodename= ss.str();
}

void
Pipe::receivePacket(Packet& pkt)
{
    //pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_ARRIVE);
    //if (_inflight.empty()){
    if (_count == 0){
        /* no packets currently inflight; need to notify the eventlist
           we've an event pending */
            eventlist().sourceIsPendingRel(*this,_delay);
    }
    _count++;
    if (_count == _size) {
        _inflight_v.resize(_size*2);
        if (_next_insert < _next_pop) {
                //   456789*123
                // NI *, NP 1
            for (int i=0; i < _next_insert; i++) {
                // move 4-9 into new space
                _inflight_v.at(_size+i) = _inflight_v.at(i);
            }
            _next_insert += _size;
        } else {
            // 123456789*
            // nothing to do
        }
        _size += _size;
    }
    _inflight_v[_next_insert].time = eventlist().now() + _delay;
    _inflight_v[_next_insert].pkt = &pkt;
    _next_insert = (_next_insert +1) % _size;
    //_inflight.push_front(make_pair(eventlist().now() + _delay, &pkt));
}

void
Pipe::doNextEvent() {
    //if (_inflight.size() == 0) 
    if (_count == 0) 
            return;

    //Packet *pkt = _inflight.back().second;
    //_inflight.pop_back();
    Packet *pkt = _inflight_v[_next_pop].pkt;
    _next_pop = (_next_pop +1) % _size;
    _count--;
    pkt->flow().logTraffic(*pkt, *this,TrafficLogger::PKT_DEPART);

    // tell the packet to move itself on to the next hop
    pkt->sendOn();

    //if (!_inflight.empty()) {
    if (_count > 0) {
        // notify the eventlist we've another event pending
        simtime_picosec nexteventtime = _inflight_v[_next_pop].time;
        _eventlist.sourceIsPending(*this, nexteventtime);
    }
}
