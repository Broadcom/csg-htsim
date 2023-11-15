// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "callback_pipe.h"
#include <iostream>
#include <sstream>

CallbackPipe::CallbackPipe(simtime_picosec delay, EventList& eventlist,PacketSink* c)
    : Pipe(delay, eventlist)
{
    stringstream ss;
    ss << "callbackpipe(" << delay/1000000 << "us)";
    _nodename= ss.str();

    //if callback is NULL, this is send the packet back to its current hop - careful with loops!
    _callback = c;
}

void
CallbackPipe::doNextEvent() {
    //if (_inflight.size() == 0) 
    if (_count == 0) 
        return;

    //Packet *pkt = _inflight.back().second;
    //_inflight.pop_back();
    Packet *pkt = _inflight_v[_next_pop].pkt;
    _next_pop = (_next_pop +1) % _size;
    _count--;    

    // tell the packet to move itself on to the next hop
    if (_callback)
            _callback->receivePacket(*pkt);
    else
            pkt->currentHop()->receivePacket(*pkt);

    //if (!_inflight.empty()) {
    if (_count > 0) {
        // notify the eventlist we've another event pending
        //simtime_picosec nexteventtime = _inflight.back().first;
        simtime_picosec nexteventtime = _inflight_v[_next_pop].time;
        _eventlist.sourceIsPending(*this, nexteventtime);
    }
}
