// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include <sstream>
#include <math.h>
#include "meter.h"


Meter::Meter(linkspeed_bps bitrate, mem_b maxsize, EventList& eventlist)
  : EventSource(eventlist,"meter"), 
    _maxsize(maxsize), _bitrate(bitrate)
{
    _queuesize = 0;

    _endpesitrim = 0;

    _endsemipesitrim = 0;

    _last_update = 0;
    _credit = _maxsize;
    
    _rr = 0;
    _ps_per_byte = (simtime_picosec)((pow(10.0, 12.0) * 8) / _bitrate);
    stringstream ss;
    ss << "meter(" << bitrate/1000000 << "Mb/s," << maxsize << "bytes), psperbyte " << _ps_per_byte;

    _nodename = ss.str();
}


void
Meter::beginService()
{
    /* schedule the next dequeue event */
    assert(!_enqueued.empty());
    eventlist().sourceIsPendingRel(*this, drainTime(_enqueued.back()));
}

void
Meter::completeService()
{
    /* dequeue the packet */
    assert(!_enqueued.empty());
    simtime_picosec p = _enqueued.back();
    _enqueued.pop_back();
    _queuesize -= p;

    if (!_enqueued.empty()) {
        /* schedule the next dequeue event */
        beginService();
    }
}

void
Meter::doNextEvent() 
{
    completeService();
}


meter_output
Meter::execute(Packet& pkt) 
{
    simtime_picosec service_rate = _ps_per_byte;

    if (eventlist().now() < _endpesitrim){
        service_rate = service_rate * 4;
    }
    else if (eventlist().now() < _endsemipesitrim){
        service_rate = service_rate * 2;
    }

    simtime_picosec delta = eventlist().now() - _last_update;

    
    
    _credit += delta/service_rate;

    if (_credit>=_maxsize)
        _credit = _maxsize;

    //cout << "Adding credit " << delta/service_rate << "total " << _credit << "delta " << delta << " service rate " << service_rate << " Times " << eventlist().now() << " " << _endpesitrim << " " << _endsemipesitrim << endl;

    _last_update = eventlist().now();
    
    if (pkt.size() > _credit){
        return METER_RED;
    }
    else
        _credit -= pkt.size();

    return METER_GREEN;
}

mem_b 
Meter::queuesize() {
    return _queuesize;
}

simtime_picosec
Meter::serviceTime() {
    return _queuesize * _ps_per_byte;
}
