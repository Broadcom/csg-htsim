// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include <iostream>
#include <iomanip>
#include "loggers.h"


// LoggedManager is a way to keep track of all the Logged instances
// that have been created so we can dump a map of IDs to Names to help
// interpret the IDs in the logfiles.

LoggedManager::LoggedManager() {};

void LoggedManager::add_logged(Logged* logged) {
    _idmap.push_back(logged);
}

void LoggedManager::dump_idmap() {
    std::ofstream fout("idmap.txt");
    for (size_t i = 0; i < _idmap.size(); i++) {
        fout << _idmap[i]->get_id() << " " << _idmap[i]->_name << endl;
    }
    fout.close();
}

LoggedManager Logged::_logged_manager;

string Logger::event_to_str(RawLogEvent& event) {
    return event.str();
}

QueueLoggerFactory::QueueLoggerFactory(Logfile* lg, QueueLoggerType logtype, EventList& eventlist)
    :_logfile(lg), _logger_type(logtype), _eventlist(eventlist)
{
};

QueueLogger *QueueLoggerFactory::createQueueLogger() {
    QueueLogger* queue_logger = 0;
    switch(_logger_type) {
    case LOGGER_SIMPLE:
        queue_logger = new QueueLoggerSimple();
        _logfile->addLogger(*queue_logger);
        break;
    case LOGGER_SAMPLING:
        queue_logger = new QueueLoggerSampling(_sample_period, _eventlist);
        _logfile->addLogger(*queue_logger);
        break;
    case MULTIQUEUE_SAMPLING:
        abort(); // we can't do this - don't know the ID
        break;
    case LOGGER_EMPTY:
        queue_logger = new QueueLoggerEmpty(_sample_period, _eventlist);
        _logfile->addLogger(*queue_logger);
        break;
    }
    assert(queue_logger);
    _loggers.push_back(queue_logger);
    return queue_logger;
}

void QueueLoggerSimple::logQueue(BaseQueue& queue, QueueLogger::QueueEvent ev, 
                                 Packet& pkt) {
    _logfile->writeRecord(Logger::QUEUE_EVENT,
                          queue.get_id(), ev, 
                          (double)queue.queuesize(),
                          pkt.flow().get_id(),
                          pkt.id()); 
}

string QueueLoggerSimple::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    assert(event._type == Logger::QUEUE_EVENT);
    ss << " ID " << event._id;
    switch(event._ev) {
    case QueueLogger::PKT_ENQUEUE:
        ss << " Ev ENQUEUE";
        break;
    case QueueLogger::PKT_DROP:
        ss << " Ev DROP";
        break;
    case QueueLogger::PKT_SERVICE:
        ss << " Ev SERVICE";
        break;
    case QueueLogger::PKT_TRIM:
        ss << " Ev TRIM";
        break;
    case QueueLogger::PKT_BOUNCE:
        ss << " Ev BOUNCE";
        break;
    }
    ss << " Qsize " << (uint64_t)event._val1 
       << " FlowID " << (uint64_t)event._val2 
       << " PktID " << (uint64_t)event._val3;
    return ss.str();
}

QueueLoggerEmpty::QueueLoggerEmpty(simtime_picosec period, EventList& eventlist)
    : EventSource(eventlist,"QueuelogEmpty"), _last_transition(0), _total_busy(0), _period(period), _last_dump(0), _busy(false), _queue(0), _pkt_arrivals(0), _pkt_trims(0) {
    eventlist.sourceIsPendingRel(*this,0);
};

// log the fraction of time the link is busy/empty
void QueueLoggerEmpty::logQueue(BaseQueue& queue, QueueLogger::QueueEvent ev, 
                                Packet& pkt) {
    if (!_queue) {
        _queue = &queue;
    }
    switch(ev) {
    case PKT_ARRIVE:
        // it arrived, don't know its outcome yet
        _pkt_arrivals ++;
        break;
    case PKT_ENQUEUE:
        if (_busy == false) {
            // queue transitioned from empty to non-empty
            _last_transition = eventlist().now();
            _busy = true;
        }
        break;
    case PKT_DROP:
        break;
    case PKT_TRIM:
        _pkt_trims++;
        break;
    case PKT_BOUNCE:
        break;
    case PKT_UNQUEUE:
    case PKT_SERVICE:
        if (_queue->queuesize() == 0) {
            // queue transitioned from non-empty to empty
            assert(_busy);
            _total_busy += eventlist().now() - _last_transition;
            _busy = false;
            _last_transition = eventlist().now();
        }
        break;
    }

}

void
QueueLoggerEmpty::doNextEvent() 
{
    eventlist().sourceIsPendingRel(*this,_period);
    if (_busy) {
        _total_busy += eventlist().now() - _last_transition;
    }
    if (_queue) {
        double trim_frac = 0;
        if (_pkt_arrivals > 0) {
            trim_frac = ((double)_pkt_trims)/_pkt_arrivals;
        }
        cout << eventlist().now() << " " << _queue->nodename() << " " << _total_busy << " " << eventlist().now() - _last_dump <<
            " " << ((double)_total_busy) / (eventlist().now() - _last_dump) << " " << trim_frac << endl;
    }
    reset_count();
    _last_dump = eventlist().now();
}

void QueueLoggerEmpty::reset_count() {
    _last_transition = eventlist().now();
    _total_busy = 0;
    _pkt_trims = 0;
    _pkt_arrivals = 0;
    if (_queue) {
        _busy = (_queue->queuesize() > 0);
    } else {
        // we've not seen any packets yet, so don't know the queue,
        // but it must be empty
        _busy = false;
    }
}

string QueueLoggerEmpty::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << "QueueLoggerEmpty::event_to_str TBD\n";
    return ss.str();
}

QueueLoggerSampling::QueueLoggerSampling(simtime_picosec period, 
                                         EventList &eventlist)
    : EventSource(eventlist,"QueuelogSampling"),
      _queue(NULL), _lastlook(0), _period(period), _lastq(0), 
      _seenQueueInD(false), _cumidle(0), _cumarr(0), _cumdrop(0)
{        
    eventlist.sourceIsPendingRel(*this,0);
}

void
QueueLoggerSampling::doNextEvent() 
{
    eventlist().sourceIsPendingRel(*this,_period);
    if (_queue==NULL) return;
    mem_b queuebuff = _queue->maxsize();
    if (!_seenQueueInD) { // queue size hasn't changed in the past D time units
        _logfile->writeRecord(QUEUE_APPROX, _queue->get_id(), QUEUE_RANGE,
                              (double)_lastq, (double)_lastq, (double)_lastq);
        _logfile->writeRecord(QUEUE_APPROX, _queue->get_id(), QUEUE_OVERFLOW, 0, 0,
                              (double)queuebuff);
    }
    else { // queue size has changed
        _logfile->writeRecord(QUEUE_APPROX, _queue->get_id(), QUEUE_RANGE,
                              (double)_lastq, (double)_minQueueInD,
                              (double)_maxQueueInD);
        _logfile->writeRecord(QUEUE_APPROX,_queue->get_id(), QUEUE_OVERFLOW,
                              -(double)_lastIdledInD, (double)_lastDroppedInD,
                              (double)queuebuff);
    }
    _seenQueueInD=false;
    simtime_picosec now = eventlist().now();
    simtime_picosec dt_ps = now - _lastlook;
    _lastlook = now;
    // if the queue is empty, we've just been idling
    if ((_queue!=NULL) && (_queue->queuesize()==0)) 
        _cumidle += timeAsSec(dt_ps); 
    _logfile->writeRecord(QUEUE_RECORD, _queue->get_id(), CUM_TRAFFIC, _cumarr,
                          _cumidle, _cumdrop);
}

void
QueueLoggerSampling::logQueue(BaseQueue& queue, QueueEvent ev, Packet &pkt) {
    if (_queue==NULL) _queue=&queue;
    assert(&queue==_queue);
    _lastq = queue.queuesize();

    if (!_seenQueueInD) {
        _seenQueueInD=true;
        _minQueueInD=queue.queuesize();
        _maxQueueInD=_minQueueInD;
        _lastDroppedInD=0;
        _lastIdledInD=0;
        _numIdledInD=0;
        _numDropsInD=0;
    } else {
        _minQueueInD=min(_minQueueInD,queue.queuesize());
        _maxQueueInD=max(_maxQueueInD,queue.queuesize());
    }
    simtime_picosec now = eventlist().now();
    simtime_picosec dt_ps = now-_lastlook;
    double dt = timeAsSec(dt_ps);
    _lastlook = now;
    switch(ev) {
    case PKT_SERVICE: // we've just been working
        break;
    case PKT_ENQUEUE:
        _cumarr += timeAsSec(queue.drainTime(&pkt));
        if (queue.queuesize() > pkt.size()) // we've just been working
            {}
        else { // we've just been idling 
            mem_b idledwork = queue.serviceCapacity(dt_ps);
            _cumidle += dt; 
            _lastIdledInD = idledwork;
            _numIdledInD++;
        }
        break;
    case PKT_DROP: // assume we've just been working
        { 
            assert(queue.queuesize() >= pkt.size()); // it is possible to
            // drop when queue is
            // idling, but this
            // logger can't make
            // sense of it
            double localdroptime = timeAsSec(queue.drainTime(&pkt));
            _cumarr += localdroptime;
            _cumdrop += localdroptime;
            _lastDroppedInD = pkt.size();
            _numDropsInD++;
            break;
        }
    case PKT_TRIM: 
    case PKT_BOUNCE:
    case PKT_UNQUEUE:
    case PKT_ARRIVE:
        /* we don't currently do anything with this */
        break;
    }
}

string QueueLoggerSampling::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    switch(event._type) {
    case Logger::QUEUE_APPROX:
        ss << " Type QUEUE_APPROX";
        ss << " ID " << event._id;
        switch(event._ev) {
        case QUEUE_RANGE:
            ss << " Ev RANGE LastQ " << (int)event._val1 << " MinQ " << (int)event._val2
               << " MaxQ " << (int)event._val3;
            if (event._name!="") ss << " Name " << event._name;
            break;
        case QUEUE_OVERFLOW:
            ss << " Ev OVERLOW LastIdled " << (int)event._val1 
               << " LastDropped " << (int)event._val2 << " QueueBuf " << (int)event._val3;
            if (event._name!="") ss << " Name " << event._name;
            break;
        default:
            ss << " Unknown Event " << event._ev;
        }
        break;
    case Logger::QUEUE_RECORD:
        ss << " Type QUEUE_APPROX";
        ss << " ID " << event._id;
        assert(event._ev == QueueLogger::CUM_TRAFFIC);
        ss << " Ev CUM_TRAFFIC CumArr " << (int)event._val1
           << " CumIdle " << (int)event._val2 << " CumDrop " << (int)event._val3;
        if (event._name!="") ss << " Name " << event._name;
        break;
    default:
        ss << "Unknown record type: " << event._type;
    }
    return ss.str();
}


MultiQueueLoggerSampling::MultiQueueLoggerSampling(id_t id, simtime_picosec period, 
                                                   EventList &eventlist)
    : EventSource(eventlist,"MultiQueuelogSampling"),
      _id(id), _period(period),
      _seenQueueInD(false), _currentQueueSizeBytes(0), _currentQueueSizePkts(0)
{        
    eventlist.sourceIsPendingRel(*this,0);
}

void
MultiQueueLoggerSampling::doNextEvent() 
{
    eventlist().sourceIsPendingRel(*this,_period);
    if (!_seenQueueInD) { // queue size hasn't changed in the past D time units
        _logfile->writeRecord(QUEUE_APPROX, _id, QUEUE_RANGE, (double)_currentQueueSizeBytes,
                              (double)_currentQueueSizeBytes, (double)_currentQueueSizeBytes);
    } else { // queue size has changed
        _logfile->writeRecord(QUEUE_APPROX, _id, QUEUE_RANGE, (double)_currentQueueSizeBytes,
                              (double)_minQueueInD, (double)_maxQueueInD);
    }
    _seenQueueInD=false;
}

void
MultiQueueLoggerSampling::logQueue(BaseQueue& queue, QueueEvent ev, Packet &pkt) {
    switch(ev) {
    case PKT_ENQUEUE:
        _currentQueueSizeBytes += pkt.size();
        _currentQueueSizePkts ++;
        //cout << get_id() << " EN size " << pkt.size() << " Queue " << queue.nodename() << " total " << _currentQueueSizeBytes << " qs " << queue.queuesize() << " id " << queue.get_id() << endl;

        assert (queue.queuesize () <= _currentQueueSizeBytes);
        break;
    case PKT_TRIM:
        //_currentQueueSizeBytes += pkt.si
        //_currentQueueSizePkts ++;
        //cout << get_id() << " TR size " << pkt.size() << endl;
        break;
    case PKT_SERVICE:
        _currentQueueSizeBytes -= pkt.size();
        _currentQueueSizePkts--;
        //cout << get_id() << " SE size " << -pkt.size() << endl;
        //cout << get_id() << " SE size " << pkt.size() << " Queue " << queue.nodename() << " total " << _currentQueueSizeBytes << " qs " << queue.queuesize() << " id " << queue.get_id() << endl;

        break;
    case PKT_UNQUEUE:
        _currentQueueSizeBytes -= pkt.size();
        _currentQueueSizePkts--;
        //cout << get_id() << " UN size " << -pkt.size() << endl;
        break;
    case PKT_DROP: 
    case PKT_BOUNCE:
    case PKT_ARRIVE:
        // doesn't change queue size
        break;
    }
    if (!_seenQueueInD) {
        _seenQueueInD=true;
        _minQueueInD=_currentQueueSizeBytes;
        _maxQueueInD=_minQueueInD;
    } else {
        _minQueueInD=min(_minQueueInD, _currentQueueSizeBytes);
        _maxQueueInD=max(_maxQueueInD, _currentQueueSizeBytes);
    }
    assert(_currentQueueSizePkts >= 0);
    assert(_currentQueueSizeBytes >= 0);
}

string MultiQueueLoggerSampling::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    switch(event._type) {
    case Logger::QUEUE_APPROX:
        ss << " Type QUEUE_APPROX";
        ss << " ID " << event._id;
        switch(event._ev) {
        case QUEUE_RANGE:
            ss << " Ev RANGE LastQ " << (int)event._val1 << " MinQ " << (int)event._val2
               << " MaxQ " << (int)event._val3;

            if (event._name!="") ss << " Name " << event._name;
            break;
        default:
            ss << " Unknown Event " << event._ev;
        }
        break;
    default:
        ss << "Unknown record type: " << event._type;
    }
    return ss.str();
}

void FlowEventLoggerSimple::logEvent(PacketFlow& flow, Logged& location, FlowEvent ev, mem_b bytes, uint64_t pkts) {
    _logfile->writeRecord(Logger::FLOW_EVENT,
                          location.get_id(),
                          ev,
                          flow.get_id(),
                          bytes, pkts);
}

string FlowEventLoggerSimple::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    assert(event._type == Logger::FLOW_EVENT);
    ss << " Type FLOW_EVENT SrcID " << event._id;
    switch((FlowEventLogger::FlowEvent)event._ev) {
    case START:
        ss << " Ev START ";
        ss << " FlowID " << (uint64_t)event._val1;
        ss << " Flowsize " << (uint64_t)event._val2;
        break;
    case FINISH:
        ss << " Ev FINISH";
        ss << " FlowID " << (uint64_t)event._val1;
        ss << " Bytes " << (uint64_t)event._val2;
        ss << " Pkts " << (uint64_t)event._val3;
        break;
    }
    return ss.str();
}

void TrafficLoggerSimple::logTraffic(Packet& pkt, Logged& location, 
                                     TrafficEvent ev) {
    _logfile->writeRecord(Logger::TRAFFIC_EVENT,
                          location.get_id(),
                          ev,
                          pkt.flow().get_id(),
                          pkt.id(),
                          0); 
}

string TrafficLoggerSimple::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    assert(event._type == Logger::TRAFFIC_EVENT);
    ss << " Type TRAFFIC ID " << event._id;
    switch((TrafficLogger::TrafficEvent)event._ev) {
    case PKT_ARRIVE:
        ss << " Ev ARRIVE ";
        break;
    case PKT_DEPART:
        ss << " Ev DEPART ";
        break;
    case PKT_CREATESEND:
        ss << " Ev CREATESEND ";
        break;
    case PKT_CREATE:
        ss << " Ev CREATE ";
        break;
    case PKT_SEND:
        ss << " Ev SEND ";
        break;
    case PKT_DROP:
        ss << " Ev DROP ";
        break;
    case PKT_RCVDESTROY:
        ss << " Ev RCV ";
        break;
    case PKT_TRIM:
        ss << " Ev TRIM ";
        break;
    case PKT_BOUNCE:
        ss << " Ev BOUNCE ";
        break;
    }
    ss << " FlowID " << (uint64_t)event._val1 
       << " PktID " << (uint64_t)event._val2;
    return ss.str();
}

void TcpTrafficLogger::logTraffic(Packet& pkt, Logged& location, 
                                  TrafficEvent ev) {
    _logfile->writeRecord(Logger::TCP_TRAFFIC,
                          location.get_id(),
                          ev,
                          pkt.flow().get_id(),
                          pkt.id(),
                          0); 
}

string TcpTrafficLogger::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    assert(event._type == Logger::TCP_TRAFFIC);
    ss << " Type TCPTRAFFIC ID " << event._id;
    switch((TrafficLogger::TrafficEvent)event._ev) {
    case PKT_ARRIVE:
        ss << " Ev ARRIVE ";
        break;
    case PKT_DEPART:
        ss << " Ev DEPART ";
        break;
    case PKT_CREATESEND:
        ss << " Ev CREATESEND ";
        break;
    case PKT_CREATE:
        ss << " Ev CREATE ";
        break;
    case PKT_SEND:
        ss << " Ev SEND ";
        break;
    case PKT_DROP:
        ss << " Ev DROP ";
        break;
    case PKT_RCVDESTROY:
        ss << " Ev RCV ";
        break;
    case PKT_TRIM: // doesn't make sense for TCP
        ss << " Ev TRIM ";
        break;
    case PKT_BOUNCE: // doesn't make sense for TCP
        ss << " Ev BOUNCE ";
        break;
    }
    ss << " FlowID " << (uint64_t)event._val1 
       << " PktID " << (uint64_t)event._val2;
    return ss.str()
        ;
}

void SwiftTrafficLogger::logTraffic(Packet& pkt, Logged& location, 
                                    TrafficEvent ev) {
    _logfile->writeRecord(Logger::SWIFT_TRAFFIC,
                          location.get_id(),
                          ev,
                          pkt.flow().get_id(),
                          pkt.id(),
                          0); 
}

string SwiftTrafficLogger::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    assert(event._type == Logger::TCP_TRAFFIC);
    ss << " Type TCPTRAFFIC ID " << event._id;
    switch((TrafficLogger::TrafficEvent)event._ev) {
    case PKT_ARRIVE:
        ss << " Ev ARRIVE ";
        break;
    case PKT_DEPART:
        ss << " Ev DEPART ";
        break;
    case PKT_CREATESEND:
        ss << " Ev CREATESEND ";
        break;
    case PKT_CREATE:
        ss << " Ev CREATE ";
        break;
    case PKT_SEND:
        ss << " Ev SEND ";
        break;
    case PKT_DROP:
        ss << " Ev DROP ";
        break;
    case PKT_RCVDESTROY:
        ss << " Ev RCV ";
        break;
    case PKT_TRIM: // doesn't make sense for Swift
        ss << " Ev TRIM ";
        break;
    case PKT_BOUNCE: // doesn't make sense for Swift
        ss << " Ev BOUNCE ";
        break;
    }
    ss << " FlowID " << (uint64_t)event._val1 
       << " PktID " << (uint64_t)event._val2;
    return ss.str()
        ;
}

void STrackTrafficLogger::logTraffic(Packet& pkt, Logged& location, 
                                     TrafficEvent ev) {
    _logfile->writeRecord(Logger::STRACK_TRAFFIC,
                          location.get_id(),
                          ev,
                          pkt.flow().get_id(),
                          pkt.id(),
                          0); 
}

string STrackTrafficLogger::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    assert(event._type == Logger::TCP_TRAFFIC);
    ss << " Type TCPTRAFFIC ID " << event._id;
    switch((TrafficLogger::TrafficEvent)event._ev) {
    case PKT_ARRIVE:
        ss << " Ev ARRIVE ";
        break;
    case PKT_DEPART:
        ss << " Ev DEPART ";
        break;
    case PKT_CREATESEND:
        ss << " Ev CREATESEND ";
        break;
    case PKT_CREATE:
        ss << " Ev CREATE ";
        break;
    case PKT_SEND:
        ss << " Ev SEND ";
        break;
    case PKT_DROP:
        ss << " Ev DROP ";
        break;
    case PKT_RCVDESTROY:
        ss << " Ev RCV ";
        break;
    case PKT_TRIM: // doesn't make sense for Strack
        ss << " Ev TRIM ";
        break;
    case PKT_BOUNCE: // doesn't make sense for Strack
        ss << " Ev BOUNCE ";
        break;
    }
    ss << " FlowID " << (uint64_t)event._val1 
       << " PktID " << (uint64_t)event._val2;
    return ss.str()
        ;
}

#define NDP_IS_ACK 1<<31
#define NDP_IS_NACK 1<<30
#define NDP_IS_PULL 1<<29
#define NDP_IS_HEADER 1<<28
#define NDP_IS_LASTDATA 1<<27
void NdpTrafficLogger::logTraffic(Packet& pkt, Logged& location, 
                                  TrafficEvent ev) {
    Packet& p = static_cast<Packet&>(pkt);
    uint32_t val3=0; // ugly hack to store NDP-specific data in a double

    if (p.type() == NDPACK) {
        val3 |= NDP_IS_ACK;
    } else if (p.type() == NDPNACK) {
        val3 |= NDP_IS_NACK;
    } else if (p.type() == NDPPULL) {
        val3 |= NDP_IS_PULL;
    } else if (p.type() == NDP) {
        NdpPacket& np = static_cast<NdpPacket&>(pkt);
        if (np.last_packet()) {
            val3 |= NDP_IS_LASTDATA;
        }
        if (np.header_only()) {
            val3 |= NDP_IS_HEADER;
        }
    }
    
    _logfile->writeRecord(Logger::NDP_TRAFFIC,
                          location.get_id(),
                          ev,
                          p.flow().get_id(),
                          p.id(),
                          val3); 
}

string NdpTrafficLogger::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    assert(event._type == Logger::NDP_TRAFFIC);
    ss << " Type NDPTRAFFIC ID " << event._id;
    switch((TrafficLogger::TrafficEvent)event._ev) {
    case PKT_ARRIVE:
        ss << " Ev ARRIVE ";
        break;
    case PKT_DEPART:
        ss << " Ev DEPART ";
        break;
    case PKT_CREATESEND:
        ss << " Ev CREATESEND ";
        break;
    case PKT_CREATE:
        ss << " Ev CREATE ";
        break;
    case PKT_SEND:
        ss << " Ev SEND ";
        break;
    case PKT_DROP:
        ss << " Ev DROP ";
        break;
    case PKT_RCVDESTROY:
        ss << " Ev RCV ";
        break;
    case PKT_TRIM:
        ss << " Ev TRIM ";
        break;
    case PKT_BOUNCE:
        ss << " Ev BOUNCE ";
        break;
    }
    ss << " FlowID " << (uint64_t)event._val1;
    uint32_t val3i = (uint32_t)event._val3;
    if (val3i & NDP_IS_ACK) { 
        ss << " Ptype ACK"
           << " Ackno " << (uint64_t)event._val2;
    } else if (val3i & NDP_IS_NACK) {
        ss << " Ptype NACK"
           << " Ackno " << (uint64_t)event._val2;
    } else if (val3i & NDP_IS_PULL) {
        ss << " Ptype PULL"
           << " Ackno " << (uint64_t)event._val2;
    } else {
        ss << " Ptype DATA"
           << " Seqno " << (uint64_t)event._val2;
        if (val3i & NDP_IS_LASTDATA) {
            ss << " flag LASTDATA";
        }
    }
    if (val3i & NDP_IS_HEADER) 
        ss << " Psize HEADER";
    else
        ss << " Psize FULL";
    return ss.str();
}


#define ROCE_IS_ACK 1<<31
#define ROCE_IS_NACK 1<<30
#define ROCE_IS_HEADER 1<<28
#define ROCE_IS_LASTDATA 1<<27

void RoceTrafficLogger::logTraffic(Packet& pkt, Logged& location, TrafficEvent ev) {
    RocePacket& p = static_cast<RocePacket&>(pkt);
    uint32_t val3=0; // ugly hack to store data in a double

    if (p.type() == ROCEACK) {
        val3 |= ROCE_IS_ACK;
    } else if (p.type() == ROCENACK) {
        val3 |= ROCE_IS_NACK;
    } else if (p.type() == ROCE && p.last_packet()) {
        val3 |= ROCE_IS_LASTDATA;
    }

    if (p.header_only())
        val3 |= ROCE_IS_HEADER;
    
    _logfile->writeRecord(Logger::ROCE_TRAFFIC,
                          location.get_id(),
                          ev,
                          p.flow().get_id(),
                          p.id(),
                          val3); 
}

string RoceTrafficLogger::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    assert(event._type == Logger::ROCE_TRAFFIC);
    ss << " Type ROCETRAFFIC ID " << event._id;
    switch((TrafficLogger::TrafficEvent)event._ev) {
    case PKT_ARRIVE:
        ss << " Ev ARRIVE ";
        break;
    case PKT_DEPART:
        ss << " Ev DEPART ";
        break;
    case PKT_CREATESEND:
        ss << " Ev CREATESEND ";
        break;
    case PKT_CREATE:
        ss << " Ev CREATE ";
        break;
    case PKT_SEND:
        ss << " Ev SEND ";
        break;
    case PKT_DROP:
        ss << " Ev DROP ";
        break;
    case PKT_RCVDESTROY:
        ss << " Ev RCV ";
        break;
    case PKT_TRIM:
    case PKT_BOUNCE:
        abort(); // shouldn't happen with RoCE
    }
    ss << " FlowID " << (uint64_t)event._val1;
    uint32_t val3i = (uint32_t)event._val3;
    if (val3i & ROCE_IS_ACK) { 
        ss << " Ptype ACK"
           << " Ackno " << (uint64_t)event._val2;
    } else if (val3i & ROCE_IS_NACK) {
        ss << " Ptype NACK"
           << " Ackno " << (uint64_t)event._val2;
    } else {
        ss << " Ptype DATA"
           << " Seqno " << (uint64_t)event._val2;
        if (val3i & ROCE_IS_LASTDATA) {
            ss << " flag LASTDATA";
        }
    }
    if (val3i & ROCE_IS_HEADER) 
        ss << " Psize HEADER";
    else
        ss << " Psize FULL";
    return ss.str();
}



#define HPCC_IS_ACK 1<<31
#define HPCC_IS_NACK 1<<30
#define HPCC_IS_HEADER 1<<28
#define HPCC_IS_LASTDATA 1<<27

void HPCCTrafficLogger::logTraffic(Packet& pkt, Logged& location, TrafficEvent ev) {
    HPCCPacket& p = static_cast<HPCCPacket&>(pkt);
    uint32_t val3=0; // ugly hack to store data in a double

    if (p.type() == HPCCACK) {
        val3 |= HPCC_IS_ACK;
    } else if (p.type() == HPCCNACK) {
        val3 |= HPCC_IS_NACK;
    } else if (p.type() == HPCC && p.last_packet()) {
        val3 |= HPCC_IS_LASTDATA;
    }

    if (p.header_only())
        val3 |= HPCC_IS_HEADER;
    
    _logfile->writeRecord(Logger::HPCC_TRAFFIC,
                          location.get_id(),
                          ev,
                          p.flow().get_id(),
                          p.id(),
                          val3); 
}

string HPCCTrafficLogger::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    assert(event._type == Logger::HPCC_TRAFFIC);
    ss << " Type HPCCTRAFFIC ID " << event._id;
    switch((TrafficLogger::TrafficEvent)event._ev) {
    case PKT_ARRIVE:
        ss << " Ev ARRIVE ";
        break;
    case PKT_DEPART:
        ss << " Ev DEPART ";
        break;
    case PKT_CREATESEND:
        ss << " Ev CREATESEND ";
        break;
    case PKT_CREATE:
        ss << " Ev CREATE ";
        break;
    case PKT_SEND:
        ss << " Ev SEND ";
        break;
    case PKT_DROP:
        ss << " Ev DROP ";
        break;
    case PKT_RCVDESTROY:
        ss << " Ev RCV ";
        break;
    case PKT_TRIM:
    case PKT_BOUNCE:
        abort(); // shouldn't happen with RoCE
    }
    ss << " FlowID " << (uint64_t)event._val1;
    uint32_t val3i = (uint32_t)event._val3;
    if (val3i & HPCC_IS_ACK) { 
        ss << " Ptype ACK"
           << " Ackno " << (uint64_t)event._val2;
    } else if (val3i & HPCC_IS_NACK) {
        ss << " Ptype NACK"
           << " Ackno " << (uint64_t)event._val2;
    } else {
        ss << " Ptype DATA"
           << " Seqno " << (uint64_t)event._val2;
        if (val3i & HPCC_IS_LASTDATA) {
            ss << " flag LASTDATA";
        }
    }
    if (val3i & HPCC_IS_HEADER) 
        ss << " Psize HEADER";
    else
        ss << " Psize FULL";
    return ss.str();
}

void TcpLoggerSimple::logTcp(TcpSrc &tcp, TcpEvent ev) {
    _logfile->writeRecord(Logger::TCP_EVENT, tcp.get_id(), 
                          ev,
                          tcp._cwnd, 
                          tcp._unacked, 
                          tcp._in_fast_recovery?tcp._ssthresh:tcp._cwnd);

    _logfile->writeRecord(Logger::TCP_STATE, tcp.get_id(), 
                          TcpLogger::TCPSTATE_CNTRL,
                          tcp._cwnd,
                          tcp._ssthresh,
                          tcp._recoverq);

    _logfile->writeRecord(Logger::TCP_STATE, tcp.get_id(), 
                          TcpLogger::TCPSTATE_SEQ, 
                          tcp._last_acked,
                          tcp._highest_sent,
                          tcp._RFC2988_RTO_timeout);
}

string TcpLoggerSimple::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    switch(event._type) {
    case TcpLogger::TCP_EVENT:
        ss  << " Type TCP  ID " << event._id << " Ev " << event._ev
            << "Cwnd " << (int)event._val1 << "Unacked " << (int)event._val2 
            << "SSthresh " << (int)event._val3;
        break;
    case TcpLogger::TCP_STATE:
        ss << "Ambiguous data - fix me!";
    default:
        ss << " Type " << event._type << " ID " << event._id 
           << " Ev " << event._ev << " VAL1 " << event._val1 
           << " VAL2 " << event._val2 << " VAL3 " << event._val3 << endl;        
    }
    return ss.str();
}
void SwiftLoggerSimple::logSwift(SwiftSubflowSrc &swift, SwiftEvent ev) {
    _logfile->writeRecord(Logger::SWIFT_EVENT, swift.get_id(), 
                          ev,
                          swift._swift_cwnd, 
                          swift._inflate, 
                          0);

    _logfile->writeRecord(Logger::SWIFT_STATE, swift.get_id(), 
                          SwiftLogger::SWIFTSTATE_CNTRL,
                          swift._swift_cwnd,
                          0,
                          swift._recoverq);

    _logfile->writeRecord(Logger::SWIFT_STATE, swift.get_id(), 
                          SwiftLogger::SWIFTSTATE_SEQ, 
                          swift._last_acked,
                          swift._highest_sent,
                          swift._RFC2988_RTO_timeout);
}

string SwiftLoggerSimple::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    switch(event._type) {
    case SwiftLogger::SWIFT_EVENT:
        ss  << " Type SWIFT  ID " << event._id << " Ev " << event._ev
            << "Cwnd " << (int)event._val1 << "Inflate " << (int)event._val2 
            << "SSthresh " << (int)event._val3;
        break;
    case SwiftLogger::SWIFT_STATE:
        ss << "Ambiguous data - fix me!";
    default:
        ss << " Type " << event._type << " ID " << event._id 
           << " Ev " << event._ev << " VAL1 " << event._val1 
           << " VAL2 " << event._val2 << " VAL3 " << event._val3 << endl;        
    }
    return ss.str();
}

void STrackLoggerSimple::logSTrack(STrackSrc &strack, STrackEvent ev) {
    _logfile->writeRecord(Logger::STRACK_EVENT, strack.get_id(), 
                          ev,
                          strack._strack_cwnd, 
                          strack._inflate, 
                          0);

    _logfile->writeRecord(Logger::STRACK_STATE, strack.get_id(), 
                          STrackLogger::STRACKSTATE_CNTRL,
                          strack._strack_cwnd,
                          0,
                          strack._recoverq);

    _logfile->writeRecord(Logger::STRACK_STATE, strack.get_id(), 
                          STrackLogger::STRACKSTATE_SEQ, 
                          strack._last_acked,
                          strack._highest_sent,
                          strack._RFC2988_RTO_timeout);
}

string STrackLoggerSimple::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    switch(event._type) {
    case STrackLogger::STRACK_EVENT:
        ss  << " Type STRACK  ID " << event._id << " Ev " << event._ev
            << "Cwnd " << (int)event._val1 << "Inflate " << (int)event._val2 
            << "SSthresh " << (int)event._val3;
        break;
    case STrackLogger::STRACK_STATE:
        ss << "Ambiguous data - fix me!";
    default:
        ss << " Type " << event._type << " ID " << event._id 
           << " Ev " << event._ev << " VAL1 " << event._val1 
           << " VAL2 " << event._val2 << " VAL3 " << event._val3 << endl;        
    }
    return ss.str();
}

AggregateTcpLogger::AggregateTcpLogger(simtime_picosec period, 
                                       EventList& eventlist)
    :EventSource(eventlist,"bunchofflows"), _period(period)
{
    eventlist.sourceIsPending(*this,period);
}

void
AggregateTcpLogger::monitorTcp(TcpSrc& tcp) {
    _monitoredTcps.push_back(&tcp);
}

void
AggregateTcpLogger::doNextEvent() {
    eventlist().sourceIsPending(*this, max(eventlist().now() + _period,
                                           _logfile->_starttime));
    double totunacked=0;
    double toteffcwnd=0;
    double totcwnd=0;
    int numflows=0;
    tcplist_t::iterator i;
    for (i = _monitoredTcps.begin(); i!=_monitoredTcps.end(); i++) {
        TcpSrc* tcp = *i;
        uint32_t cwnd = tcp->_cwnd;
        uint32_t unacked = tcp->_unacked;
        uint32_t effcwnd = tcp->_effcwnd;
        totcwnd += cwnd;
        toteffcwnd += effcwnd;
        totunacked += unacked;
        numflows++;
    }
    _logfile->writeRecord(Logger::TCP_RECORD, get_id(), TcpLogger::AVE_CWND,
                          totcwnd/numflows, totunacked/numflows,
                          toteffcwnd/numflows);
}

string AggregateTcpLogger::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    assert(event._type == TCP_RECORD);
    ss << " Type=TCP_RECORD ID=" << event._id;
    assert(event._ev == TcpLogger::AVE_CWND);
    ss << " Ev AVE_CWND Cwnd " << setprecision(2) << event._val1
       << " Unacked " << event._val2 << " EffCwnd " << event._val3;
    return ss.str();
}

void MultipathTcpLoggerSimple::logMultipathTcp(MultipathTcpSrc& mtcp, 
                                               MultipathTcpEvent ev){
    if (ev==MultipathTcpLogger::CHANGE_A) {
        _logfile->writeRecord(MultipathTcpLogger::MTCP, mtcp.get_id(),
                              ev, mtcp.a, mtcp._alfa, 0);
    } else if (ev==MultipathTcpLogger::RTT_UPDATE) {
        _logfile->writeRecord(MultipathTcpLogger::MTCP, mtcp.get_id(), 
                              ev, 
                              mtcp._subflows.front()->_rtt/1000000000,
                              mtcp._subflows.back()->_rtt/1000000000,
                              mtcp._subflows.front()->_mdev/1000000000);
    } else if (ev==MultipathTcpLogger::WINDOW_UPDATE) {
        _logfile->writeRecord(MultipathTcpLogger::MTCP, mtcp.get_id(),
                              ev,
                              mtcp._subflows.front()->effective_window(),
                              mtcp._subflows.back()->effective_window(),
                              0);
    }
}

string MultipathTcpLoggerSimple::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    ss << " Type MTCP ID " << event._id;
    switch(event._ev) {
    case MultipathTcpLogger::CHANGE_A:
        ss << " Ev CHANGE_A" << " ID " << event._id 
           << "A " << event._val1 << "Alfa " << event._val2;
        break;
    case MultipathTcpLogger::RTT_UPDATE:
        ss << " Ev RTT_UPD" << "ID " << event._id << " RTT1 " << (uint64_t)event._val1 
           << " RTT2 " << (uint64_t)event._val2 << " Mdev " << event._val3;
        break;
    case MultipathTcpLogger::WINDOW_UPDATE:
        ss << " Ev WIN_UPD" << "ID " << event._id << " Win1 " << (uint64_t)event._val1 
           << " Win2 " << (uint64_t)event._val2;
        break;
    case MultipathTcpLogger::RATE:
        // This is ugly - this is logged by
        // MultipathTcpLoggerSampling, but under the same type as
        // MultipathTcpLoggerSimple is using.  Should probably use a
        // different Type for this event.
        ss << " Ev RATE A " << event._val1 << " Gput " << (uint64_t)event._val2 
           << " Tput " << (uint64_t)event._val3;
        break;
    default:
        ss << " Unknown event: " << event._ev;
    }
    return ss.str();
}

vector<TcpSink*> _tcp_sinks;
vector<MultipathTcpSink*> _mtcp_sinks;
vector<TcpSrc*> _tcp_sources;
vector<TcpSink*> _mtcp_sources;

MemoryLoggerSampling::MemoryLoggerSampling(simtime_picosec period, 
                                           EventList& eventlist):
    EventSource(eventlist,"MemorySampling"), _period(period)
{
    eventlist.sourceIsPendingRel(*this,0);
}

void MemoryLoggerSampling::monitorTcpSink(TcpSink* sink){
    _tcp_sinks.push_back(sink);
}

void MemoryLoggerSampling::monitorMultipathTcpSink(MultipathTcpSink* sink){
    _mtcp_sinks.push_back(sink);
}

void MemoryLoggerSampling::monitorTcpSource(TcpSrc* src){
    _tcp_sources.push_back(src);
}

void MemoryLoggerSampling::monitorMultipathTcpSource(MultipathTcpSrc* src){
    _mtcp_sources.push_back(src);
}

void MemoryLoggerSampling::doNextEvent(){
    uint64_t i;
    eventlist().sourceIsPendingRel(*this,_period);  
  
    //simtime_picosec now = eventlist().now();
  
    for (i=0; i<_tcp_sinks.size(); i++) {
        _logfile->writeRecord(Logger::TCP_MEMORY, _tcp_sinks[i]->get_id(),
                              TcpLogger::MEMORY, 0, 0,
                              _tcp_sinks[i]->_received.size()*1000);
    }

    for (i=0; i<_tcp_sources.size(); i++) {
        _logfile->writeRecord(Logger::TCP_MEMORY, _tcp_sources[i]->get_id(),
                              TcpLogger::MEMORY, 0, 0,
                              _tcp_sources[i]->_highest_sent - _tcp_sources[i]->_last_acked);
    }

#ifdef MODEL_RECEIVE_WINDOW
    for (i=0; i<_mtcp_sinks.size(); i++) {
        _logfile->writeRecord(Logger::MTCP, _mtcp_sinks[i]->get_id(),
                              MultipathTcpLogger::MEMORY, 0, 0,
                              _mtcp_sinks[i]->_received.size()*1000);
    }

    for (i=0; i<_mtcp_sources.size(); i++) {
        _logfile->writeRecord(Logger::MTCP,
                              _mtcp_sources[i]->get_id(),
                              MultipathTcpLogger::MEMORY, 0, 0,
                              _mtcp_sources[i]->_highest_sent - _mtcp_sources[i]->_last_acked);
    }
#endif
}

string MemoryLoggerSampling::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    switch(event._type) {
    case Logger::TCP_MEMORY:
        
        ss << " Type TCP_MEMORY ID " << event._id;

        switch(event._ev) {
        case TcpLogger::MEMORY:
            ss << " Ev MEMORY Bytes " << (int)event._val3;
            break;
        default:
            ss << " Unknown event sub type " << event._ev;
            break;
        }
        break;
    default:
        ss << " Unknown event type " << event._type;
    }
    return ss.str();
}


SinkLoggerSampling::SinkLoggerSampling(simtime_picosec period, 
                                       EventList& eventlist,
                                       Logger::EventType sink_type,
                                       int event_type):
    EventSource(eventlist,"SinkSampling"), _last_time(0), _period(period), 
    _sink_type(sink_type), _event_type(event_type)
{
    eventlist.sourceIsPendingRel(*this,0);
}

void SinkLoggerSampling::monitorMultipathSink(DataReceiver* sink){
    _sinks.push_back(sink);
    _last_seq.push_back(sink->cumulative_ack());
    //_last_sndbuf.push_back(sink->sndbuf());
    _last_rate.push_back(0);
    _multipath.push_back(1);

    TcpSrc* src = ((TcpSink*)sink)->_src;

    if (src!=NULL&&src->_mSrc!=NULL){
        if (_multipath_src.find(src->_mSrc)==_multipath_src.end()){
            _multipath_seq[src->_mSrc] = 0;
            _multipath_src[src->_mSrc] = 0;
        }
    }
}

void SinkLoggerSampling::monitorSink(DataReceiver* sink){
    _sinks.push_back(sink);
    _last_seq.push_back(sink->cumulative_ack());
    //_last_sndbuf.push_back(sink->sndbuf());
    _last_rate.push_back(0);
    _multipath.push_back(0);
}

void SinkLoggerSampling::doNextEvent(){
    eventlist().sourceIsPendingRel(*this,_period);  
    simtime_picosec now = eventlist().now();
    simtime_picosec delta = now - _last_time;
    _last_time = now;
    TcpAck::seq_t  deltaB;
    uint32_t deltaSnd = 0;
    double rate;

    for (uint64_t i = 0; i<_sinks.size(); i++){
        if (_last_seq[i] <= _sinks[i]->cumulative_ack()) {
            //this deals with resets for periodic sources
            deltaB = _sinks[i]->cumulative_ack() - _last_seq[i];
            //deltaSnd = _sinks[i]->sndbuf() - _last_sndbuf[i];
            if (delta > 0)
                rate = deltaB * 1000000000000.0 / delta;//Bps
            else 
                rate = 0;
            _logfile->writeRecord(_sink_type, _sinks[i]->get_id(),
                                  _event_type, _sinks[i]->cumulative_ack(), 
                                  deltaB>0?(deltaSnd * 100000 / deltaB):0, rate);

            _last_rate[i] = rate;

            if (_multipath[i]){
                TcpSrc* src = ((TcpSink*)_sinks[i])->_src;

                if (src->_mSrc!=NULL){
                    _multipath_src[src->_mSrc] += rate;
                }
            }
        }
        _last_seq[i] = _sinks[i]->cumulative_ack();
        //_last_sndbuf[i] = _sinks[i]->sndbuf();
    }

    multipath_map::iterator it;

    for (it = _multipath_src.begin(); it!=_multipath_src.end(); it++) {
        MultipathTcpSrc* mtcp = (MultipathTcpSrc*)(*it).first;
        double throughput = (double)(*it).second;
        double goodput = 0;


        if (mtcp->_sink){
            deltaB = mtcp->_sink->cumulative_ack() - (uint64_t)_multipath_seq[mtcp];
            goodput = deltaB * 1000000000000.0 / delta;//Bps
            _multipath_seq[mtcp] = mtcp->_sink->cumulative_ack();
        } else {
            goodput = _multipath_src[mtcp];
            throughput = _multipath_src[mtcp];
        }

        _logfile->writeRecord(Logger::MTCP, mtcp->get_id(),
                              MultipathTcpLogger::RATE, mtcp->a, goodput, 
                              throughput);

        _multipath_src[mtcp] = 0;
    }
}

TcpSinkLoggerSampling::TcpSinkLoggerSampling(simtime_picosec period, 
                                             EventList& eventlist):
    SinkLoggerSampling(period, eventlist, Logger::TCP_SINK, TcpLogger::RATE)
{
}
 
string TcpSinkLoggerSampling::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    switch(event._type) {
    case Logger::TCP_SINK:
        assert(event._ev == TcpLogger::RATE);
        ss << " Type TCP_SINK ID " << event._id << " Ev RATE"
           << " CAck " << (uint64_t)event._val1 << " Rate " << (uint64_t)event._val3;
        // val2 seems to always be zero - maybe a bug
        break;
    case Logger::MTCP:
        // this case is probably unreachable, because RATE is logged under
        // Type=MTCP, whereas it really should have its own MTCP_SINK
        // type
        ss << " Type MTCP ID " << event._id;
        switch(event._ev) {
        case MultipathTcpLogger::RATE:
            ss << " Ev RATE A " << event._val1 << " Gput " << (uint64_t)event._val2 
               << " Tput " << (uint64_t)event._val3;
            break;
        default:
            ss << " Unknown event " << event._ev;
        }
        break;
    default:
        ss << "Unknown event " << event._type;
    }
    return ss.str();
}

SwiftSinkLoggerSampling::SwiftSinkLoggerSampling(simtime_picosec period,
                                                 EventList& eventlist):
    SinkLoggerSampling(period, eventlist, Logger::SWIFT_SINK, SwiftLogger::RATE)
{
}

void SwiftSinkLoggerSampling::doNextEvent(){
    eventlist().sourceIsPendingRel(*this,_period);
    simtime_picosec now = eventlist().now();
    simtime_picosec delta = now - _last_time;
    _last_time = now;
    TcpAck::seq_t  deltaB;
    uint32_t deltaSnd = 0;
    double rate;

    // if it's the first time we're here, reserve space for subflow sequence numbers                      
    if (_last_sub_seq.size() < _sinks.size()) {
        _last_sub_seq.resize(_sinks.size());
        for (size_t i = 0; i<_sinks.size(); i++){
            SwiftSink* sink = (SwiftSink*)_sinks[i];
            if (sink->_subs.size() > 1) {
                _last_sub_seq[i].resize(sink->_subs.size(), 0);
            }
        }
    }
    for (uint64_t i = 0; i<_sinks.size(); i++){
        if (_last_seq[i] <= _sinks[i]->cumulative_ack()) {
            //this deals with resets for periodic sources                                                 
            deltaB = _sinks[i]->cumulative_ack() - _last_seq[i];
            //deltaSnd = _sinks[i]->sndbuf() - _last_sndbuf[i];                                           
            if (delta > 0)
                rate = deltaB * 1000000000000.0 / delta;//Bps                                             
            else
                rate = 0;
            _logfile->writeRecord(_sink_type, _sinks[i]->get_id(),
                                  _event_type, _sinks[i]->cumulative_ack(),
                                  deltaB>0?(deltaSnd * 100000 / deltaB):0, rate);

            _last_rate[i] = rate;
        }
        _last_seq[i] = _sinks[i]->cumulative_ack();

        SwiftSink* sink = (SwiftSink*)_sinks[i];
        if (sink->_subs.size() > 1) {
            // log the subflows too
            for (size_t si = 0; si < sink->_subs.size(); si++) {
                SwiftSubflowSink* sub = sink->_subs[si];
                SwiftPacket::seq_t cum_ack = sub->cumulative_ack();
                deltaB = cum_ack - _last_sub_seq[i][si];
                if (delta > 0)
                    rate = deltaB * 1000000000000.0 / delta;//Bps
                else
                    rate = 0;
                _logfile->writeRecord(_sink_type, sub->get_id(),
                                      _event_type, sub->cumulative_ack(),
                                      deltaB>0?(deltaSnd * 100000 / deltaB):0, rate);
                _last_sub_seq[i][si] = cum_ack;
            }
        }
        //_last_sndbuf[i] = _sinks[i]->sndbuf();
    }
}

string SwiftSinkLoggerSampling::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    switch(event._type) {
    case Logger::SWIFT_SINK:
        assert(event._ev == SwiftLogger::RATE);
        ss << " Type SWIFT_SINK ID " << event._id << " Ev RATE"
           << " CAck " << (uint64_t)event._val1 << " Rate " << (uint64_t)event._val3;
        // val2 seems to always be zero - maybe a bug                                                     
        break;
    default:
        ss << "Unknown event " << event._type;
    }
    return ss.str();
}    


STrackSinkLoggerSampling::STrackSinkLoggerSampling(simtime_picosec period, 
                                                   EventList& eventlist):
    SinkLoggerSampling(period, eventlist, Logger::STRACK_SINK, STrackLogger::RATE)
{
    cout << "STrackSinkLoggerSampling(p=" << timeAsSec(period) << " init \n";
}

void STrackSinkLoggerSampling::doNextEvent(){
    eventlist().sourceIsPendingRel(*this,_period);  
    simtime_picosec now = eventlist().now();
    simtime_picosec delta = now - _last_time;
    _last_time = now;
    TcpAck::seq_t  deltaB;
    //uint32_t deltaSnd = 0;
    double rate;
    cout << "STrackSinkLoggerSampling(p=" << timeAsSec(_period) << "), t=" << timeAsSec(now) << "\n";
    cout << "size: " << _sinks.size() << endl;
    for (uint64_t i = 0; i<_sinks.size(); i++){
        STrackSink *sink = (STrackSink*)_sinks[i];
        cout << "total received " << sink->total_received() << endl;
        if (_last_seq[i] <= sink->total_received()) {
            deltaB = sink->total_received() - _last_seq[i];
            if (delta > 0)
                rate = deltaB * 1000000000000.0 / delta;//Bps
            else 
                rate = 0;

            _logfile->writeRecord(_sink_type, sink->get_id(),
                                  _event_type, sink->cumulative_ack(), 
                                  // deltaB>0?(deltaSnd * 100000 / deltaB):0
                                  sink->reorder_buffer_max(), rate);

            _last_rate[i] = rate;
        }
        _last_seq[i] = sink->total_received();
    }
}

string STrackSinkLoggerSampling::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    switch(event._type) {
    case Logger::STRACK_SINK:
        assert(event._ev == STrackLogger::RATE);
        ss << " Type STRACK_SINK ID " << event._id << " Ev RATE"
           << " CAck " << (uint64_t)event._val1 << " ReorderBuffer " << (uint64_t)event._val2 << " Rate " << (uint64_t)event._val3;
        // val2 seems to always be zero - maybe a bug
        break;
    default:
        ss << "Unknown event " << event._type;
    }
    return ss.str();
}

NdpSinkLoggerSampling::NdpSinkLoggerSampling(simtime_picosec period,
                                             EventList& eventlist):
    SinkLoggerSampling(period, eventlist, Logger::NDP_SINK, NdpLogger::RATE)
{
    cout << "NdpSinkLoggerSampling(p=" << timeAsSec(period) << " init \n";
}

void NdpSinkLoggerSampling::doNextEvent(){
    eventlist().sourceIsPendingRel(*this,_period);  
    simtime_picosec now = eventlist().now();
    simtime_picosec delta = now - _last_time;
    _last_time = now;
    TcpAck::seq_t  deltaB;
    //uint32_t deltaSnd = 0;
    double rate;
    //cout << "NdpSinkLoggerSampling(p=" << timeAsSec(_period) << "), t=" << timeAsSec(now) << "\n";
    for (uint64_t i = 0; i<_sinks.size(); i++){
        NdpSink *sink = (NdpSink*)_sinks[i];
        if (_last_seq[i] <= sink->total_received()) {
            deltaB = sink->total_received() - _last_seq[i];
            if (delta > 0)
                rate = deltaB * 1000000000000.0 / delta;//Bps
            else 
                rate = 0;

            _logfile->writeRecord(_sink_type, sink->get_id(),
                                  _event_type, sink->cumulative_ack(), 
                                  /*deltaB>0?(deltaSnd * 100000 / deltaB):0*/
                                  sink->reorder_buffer_size(), rate);

            _last_rate[i] = rate;
        }
        _last_seq[i] = sink->total_received();
    }
}

string NdpSinkLoggerSampling::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    switch(event._type) {
    case Logger::NDP_SINK:
        assert(event._ev == NdpLogger::RATE);
        ss << " Type NDP_SINK ID " << event._id << " Ev RATE"
           << " CAck " << (uint64_t)event._val1 << " ReorderBuffer " << (uint64_t)event._val2 << " Rate " << (uint64_t)event._val3;
        // val2 seems to always be zero - maybe a bug
        break;
    default:
        ss << "Unknown event " << event._type;
    }
    return ss.str();
}


RoceSinkLoggerSampling::RoceSinkLoggerSampling(simtime_picosec period, 
                                               EventList& eventlist):
    SinkLoggerSampling(period, eventlist, Logger::ROCE_SINK, RoceLogger::RATE)
{
    cout << "RoceSinkLoggerSampling(p=" << timeAsSec(period) << " init \n";
}

void RoceSinkLoggerSampling::doNextEvent(){
    eventlist().sourceIsPendingRel(*this,_period);  
    simtime_picosec now = eventlist().now();
    simtime_picosec delta = now - _last_time;
    _last_time = now;
    TcpAck::seq_t  deltaB;
    uint32_t deltaSnd = 0;
    double rate;

    for (uint64_t i = 0; i<_sinks.size(); i++){
        RoceSink *sink = (RoceSink*)_sinks[i];
        if (_last_seq[i] <= sink->total_received()) {
            deltaB = sink->total_received() - _last_seq[i];
            if (delta > 0)
                rate = deltaB * 1000000000000.0 / delta;//Bps
            else 
                rate = 0;
            _logfile->writeRecord(_sink_type, sink->get_id(),
                                  _event_type, sink->cumulative_ack(), 
                                  deltaB>0?(deltaSnd * 100000 / deltaB):0, rate);

            _last_rate[i] = rate;
        }
        _last_seq[i] = sink->total_received();
    }
}

string RoceSinkLoggerSampling::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    switch(event._type) {
    case Logger::ROCE_SINK:
        assert(event._ev == RoceLogger::RATE);
        ss << " Type ROCE_SINK ID " << event._id << " Ev RATE"
           << " CAck " << (uint64_t)event._val1 << " Rate " << (uint64_t)event._val3;
        // val2 seems to always be zero - maybe a bug
        break;
    default:
        ss << "Unknown event " << event._type;
    }
    return ss.str();
}


HPCCSinkLoggerSampling::HPCCSinkLoggerSampling(simtime_picosec period, 
                                               EventList& eventlist):
    SinkLoggerSampling(period, eventlist, Logger::HPCC_SINK, HPCCLogger::RATE)
{
    cout << "HPCCSinkLoggerSampling(p=" << timeAsSec(period) << " init \n";
}

void HPCCSinkLoggerSampling::doNextEvent(){
    eventlist().sourceIsPendingRel(*this,_period);  
    simtime_picosec now = eventlist().now();
    simtime_picosec delta = now - _last_time;
    _last_time = now;
    TcpAck::seq_t  deltaB;
    uint32_t deltaSnd = 0;
    double rate;

    for (uint64_t i = 0; i<_sinks.size(); i++){
        HPCCSink *sink = (HPCCSink*)_sinks[i];
        if (_last_seq[i] <= sink->total_received()) {
            deltaB = sink->total_received() - _last_seq[i];
            if (delta > 0)
                rate = deltaB * 1000000000000.0 / delta;//Bps
            else 
                rate = 0;
            _logfile->writeRecord(_sink_type, sink->get_id(),
                                  _event_type, sink->cumulative_ack(), 
                                  deltaB>0?(deltaSnd * 100000 / deltaB):0, rate);

            _last_rate[i] = rate;
        }
        _last_seq[i] = sink->total_received();
    }
}


string HPCCSinkLoggerSampling::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    switch(event._type) {
    case Logger::HPCC_SINK:
        assert(event._ev == RoceLogger::RATE);
        ss << " Type HPCC_SINK ID " << event._id << " Ev RATE"
           << " CAck " << (uint64_t)event._val1 << " Rate " << (uint64_t)event._val3;
        // val2 seems to always be zero - maybe a bug
        break;
    default:
        ss << "Unknown event " << event._type;
    }
    return ss.str();
}


void QcnLoggerSimple::logQcn(QcnReactor &src, QcnEvent ev, double var3) {
    if (ev!=QcnLogger::QCN_SEND)
        _logfile->writeRecord(Logger::QCN_EVENT,src.get_id(), ev,
                              src._currentRate,src._packetCycles,var3);
}

void QcnLoggerSimple::logQcnQueue(QcnQueue &src, QcnQueueEvent ev, 
                                  double var1, double var2, double var3) {
    _logfile->writeRecord(Logger::QCNQUEUE_EVENT,src.get_id(), ev,var1,var2,var3);
};

string QcnLoggerSimple::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    switch(event._type) {
    case Logger::QCN_EVENT:
        ss << "Type QCN Id " << event._id;
        switch(event._ev) {
        case QCN_SEND:
            ss << " Ev SEND";
            break;
        case QCN_INC:
            ss << " Ev INC";
            break;
        case QCN_DEC: 
            ss << " Ev DEC";
            break;
        case QCN_INCD:
            ss << " Ev INCD";
            break;
        case QCN_DECD:
            ss << " Ev DECD";
            break;
        default:
            ss << " Ev Unknown(" << event._ev << ")";
            break;
        }
        ss << " Rate " << (int)event._val1 << " PktCycles " << (int)event._val2
           << " Val3 " << event._val3;
        break;
    case Logger::QCNQUEUE_EVENT:
        ss << "Type QCNQUEUE Id " << event._id;
        switch(event._ev) {
        case QCN_FB:
            ss << " Ev FB";
            break;
        case QCN_NOFB:
            ss << " Ev NOFB";
            break;
        default:
            ss << " Ev Unknown(" << event._ev << ")";
            break;
        }
        ss << " Val1 " << event._val1 << " Val2 " << event._val2 
           << " Val3 " << event._val3;
    }
    return ss.str();
}


/****************************************************************************/
/* Reorder Buffer Logger */
/****************************************************************************/

ReorderBufferLoggerSampling::ReorderBufferLoggerSampling(simtime_picosec period,
                                                         EventList& eventlist)
    : EventSource(eventlist,"ReorderBufferLoggerSampling"),
      _period(period), _queue_len(0), _min_queue(0), _max_queue(0)
{
    eventlist.sourceIsPendingRel(*this,0);    
}

void ReorderBufferLoggerSampling::doNextEvent() {
    cout << "ReorderBufferLoggerSampling " << eventlist().now() << endl;
    eventlist().sourceIsPendingRel(*this,_period);
    _logfile->writeRecord(QUEUE_APPROX, get_id(), QueueLogger::QUEUE_RANGE,
                          (double)_queue_len, (double)_min_queue,
                          (double)_max_queue);
    _min_queue = _queue_len;
    _max_queue = _queue_len;
}

void
ReorderBufferLoggerSampling::logBuffer(BufferEvent ev) {
    switch (ev) {
    case BUF_ENQUEUE:
        _queue_len++;
        if (_queue_len > _max_queue) _max_queue = _queue_len;
        break;
    case BUF_DEQUEUE:
        _queue_len--;
        if (_queue_len < _min_queue) _min_queue = _queue_len;
        assert(_queue_len >= 0);
        break;
    };
}
