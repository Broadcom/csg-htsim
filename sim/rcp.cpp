// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#include "rcp.h"

////////////////////////////////////////////////////////////////
//  RCP SOURCE
////////////////////////////////////////////////////////////////

RcpSrc::RcpSrc(RcpLogger* logger, TrafficLogger* pktlogger, 
               EventList &eventlist)
    : EventSource(eventlist,"rcp"),  _logger(logger), _flow(pktlogger)
{
    _maxcwnd = 30000;
    _highest_sent = 0;
    _effcwnd = 0;
    _ssthresh = 0xffffffff;
    _last_acked = 0;
    _dupacks = 0;
    _rtt = timeFromMs(3000);
    _mss = RcpPacket::DEFAULTDATASIZE;
    _recoverq = 0;
    _in_fast_recovery = false;
}

void 
RcpSrc::startflow() {
    _cwnd = _mss;
    _unacked = _cwnd;
    send_packets();
}

void 
TcpSrc::connect(route_t& routeout, route_t& routeback, TcpSink& sink, simtime_picosec starttime) 
{
    _route = &routeout;
    _sink = &sink;
    _flow.id = id; // identify the packet flow with the TCP source that generated it
    _sink->connect(*this, routeback);
    eventlist().sourceIsPending(*this,starttime);
}


void
TcpSrc::receivePacket(Packet& pkt) 
{
    TcpAck *p = (TcpAck*)(&pkt);
    TcpAck::seq_t seqno = p->ackno();
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);
    p->free();
    assert(seqno >= _last_acked);  // no dups or reordering allowed in this simple simulator
    if (seqno > _last_acked) { // a brand new ack
        if (!_in_fast_recovery) { // best behaviour: proper ack of a new packet, when we were expecting it
            _last_acked = seqno;
            _dupacks = 0;
            inflate_window();
            _unacked = _cwnd;
            _effcwnd = _cwnd;
            if (_logger) _logger->logTcp(*this, TcpLogger::TCP_RCV);
            send_packets();
            return;
        }
        // We're in fast recovery, i.e. one packet has been
        // dropped but we're pretending it's not serious
        if (seqno >= _recoverq) { 
            // got ACKs for all the "recovery window": resume
            // normal service
            uint32_t flightsize = _highest_sent - seqno;
            _cwnd = min(_ssthresh, flightsize + _mss);
            _unacked = _cwnd;
            _effcwnd = _cwnd;
            _last_acked = seqno;
            _dupacks = 0;
            _in_fast_recovery = false;
            if (_logger) _logger->logTcp(*this, TcpLogger::TCP_RCV_FR_END);
            send_packets();
            return;
        }
        // In fast recovery, and still getting ACKs for the
        // "recovery window"

        // This is dangerous. It means that several packets
        // got lost, not just the one that triggered FR.
        uint32_t new_data = seqno - _last_acked;
        _last_acked = seqno;
        if (new_data < _cwnd) _cwnd -= new_data; else _cwnd=0;
        _cwnd += _mss;
        if (_logger) _logger->logTcp(*this, TcpLogger::TCP_RCV_FR);
        retransmit_packet(); 
        send_packets();
        return;
    }
    // It's a dup ack
    if (_in_fast_recovery) { // still in fast recovery; hopefully the prodigal ACK is on it's way
        _cwnd += _mss;
        if (_cwnd>_maxcwnd) _cwnd = _maxcwnd;
        // When we restart, the window will be set to
        // min(_ssthresh, flightsize+_mss), so keep track of
        // this
        _unacked = min(_ssthresh, _highest_sent-_recoverq+_mss); 
        if (_last_acked+_cwnd >= _highest_sent+_mss) _effcwnd=_unacked; // starting to send packets again
        if (_logger) _logger->logTcp(*this, TcpLogger::TCP_RCV_DUP_FR);
        send_packets();
        return;
    }
    // Not yet in fast recovery. What should we do instead?
    _dupacks++;
    if (_dupacks!=3) { // not yet serious worry
        if (_logger) _logger->logTcp(*this, TcpLogger::TCP_RCV_DUP);
        send_packets();
        return;
    }
    // _dupacks==3
    if (_last_acked < _recoverq) {  //See RFC 3782: if we haven't
        //recovered from timeouts
        //etc. don't do fast recovery
        if (_logger) _logger->logTcp(*this, TcpLogger::TCP_RCV_3DUPNOFR);
        return;
    }
    // begin fast recovery
    _ssthresh = max(_cwnd/2, (uint32_t)(2 * _mss));
    retransmit_packet();
    _cwnd = _ssthresh + 3 * _mss;
    _unacked = _ssthresh;
    _effcwnd = 0;
    _in_fast_recovery = true;
    _recoverq = _highest_sent; // _recoverq is the value of the
    // first ACK that tells us things
    // are back on track
    if (_logger) _logger->logTcp(*this, TcpLogger::TCP_RCV_DUP_FASTXMIT);
}

void
TcpSrc::inflate_window() {
    int newly_acked = (_last_acked + _cwnd) - _highest_sent;
    // be very conservative - possibly not the best we can do, but
    // the alternative has bad side effects.
    if (newly_acked > _mss) newly_acked = _mss; 
    if (newly_acked < 0)
        return;
    if (_cwnd < _ssthresh) { //slow start
        int increase = min(_ssthresh - _cwnd, (uint32_t)newly_acked);
        _cwnd += increase;
        newly_acked -= increase;
    }
    // additive increase
    _cwnd += (newly_acked * _mss) / _cwnd;  //XXX beware large windows, when this increase gets to be very small
}

void 
TcpSrc::send_packets() {
    while ( _last_acked + _cwnd >= _highest_sent + _mss) {
        TcpPacket* p = TcpPacket::newpkt(_flow, *_route, _highest_sent+1, _mss);
        p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);
        _highest_sent += _mss;  //XX beware wrapping
        _last_sent_time = eventlist().now();
        p->sendOn();
    }
}

void 
TcpSrc::retransmit_packet() {
    TcpPacket* p = TcpPacket::newpkt(_flow, *_route, _last_acked+1, _mss);
    p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);
    _last_sent_time = eventlist().now();
    p->sendOn();
}

void
TcpSrc::rtx_timer_hook(simtime_picosec now) {
    if (_highest_sent == 0) return;
    if (now <= _last_sent_time + timeFromMs(1000)) return;
    // RTX timer expired
    if (_logger) _logger->logTcp(*this, TcpLogger::TCP_TIMEOUT);
    if (_in_fast_recovery) {
        uint32_t flightsize = _highest_sent - _last_acked;
        _cwnd = min(_ssthresh, flightsize + _mss);
    }
    _ssthresh = max(_cwnd/2, (uint32_t)(2 * _mss));
    _cwnd = _mss;
    _unacked = _cwnd;
    _effcwnd = _cwnd;
    _in_fast_recovery = false;
    _recoverq = _highest_sent;
    _highest_sent = _last_acked + _mss;
    _dupacks = 0;
    retransmit_packet();
    //reset rtx timer
    _last_sent_time = now;
}


////////////////////////////////////////////////////////////////
//  TCP SINK
////////////////////////////////////////////////////////////////

TcpSink::TcpSink() 
    : Logged("sink") {}

void 
TcpSink::connect(TcpSrc& src, route_t& route)
{
    _src = &src;
    _route = &route;
    _cumulative_ack = 0;
}

void
TcpSink::receivePacket(Packet& pkt) {
    TcpPacket *p = (TcpPacket*)(&pkt);
    TcpPacket::seq_t seqno = p->seqno();
    int size = p->size(); // TODO: the following code assumes all packets are the same size
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);
    p->free();

    if (seqno == _cumulative_ack+1) { // it's the next expected seq no
        _cumulative_ack = seqno + size - 1;
        /* are there any additional received packets we can now ack? */
        while (!_received.empty() && (_received.front() == _cumulative_ack+1) ) {
            _received.pop_front();
            _cumulative_ack+= size;
        }
    } 
    else if (seqno < _cumulative_ack+1) {} //must have been a bad retransmit
    else { // it's not the next expected sequence number
        if (_received.empty()) {
            _received.push_front(seqno);
        } 
        else if (seqno > _received.back()) { // likely case
            _received.push_back(seqno);
        } 
        else { // uncommon case - it fills a hole
            list<uint32_t>::iterator i;
            for (i = _received.begin(); i != _received.end(); i++) {
                if (seqno == *i) break; // it's a bad retransmit
                if (seqno < (*i)) {
                    _received.insert(i, seqno);
                    break;
                }
            }
        }
    }
    send_ack();
}        

void 
TcpSink::send_ack() {
    TcpAck *ack = TcpAck::newpkt(_src->_flow, *_route, 0, _cumulative_ack);
    ack->flow().logTraffic(*ack,*this,TrafficLogger::PKT_CREATESEND);
    ack->sendOn();
}


////////////////////////////////////////////////////////////////
//  TCP RETRANSMISSION TIMER
////////////////////////////////////////////////////////////////

TcpRtxTimerScanner::TcpRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist)
    : EventSource(eventlist,"RtxScanner"), 
      _scanPeriod(scanPeriod)
{
    eventlist.sourceIsPendingRel(*this, _scanPeriod);
}

void 
TcpRtxTimerScanner::registerTcp(TcpSrc &tcpsrc)
{
    _tcps.push_back(&tcpsrc);
}

void
TcpRtxTimerScanner::doNextEvent() 
{
    simtime_picosec now = eventlist().now();
    tcps_t::iterator i;
    for (i = _tcps.begin(); i!=_tcps.end(); i++) {
        (*i)->rtx_timer_hook(now);
    }
    eventlist().sourceIsPendingRel(*this, _scanPeriod);
}
