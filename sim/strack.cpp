// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#include "strack.h"
#include <iostream>
#include <math.h>

#define KILL_THRESHOLD 5
uint32_t STrackSrc::_default_cwnd = 12;

////////////////////////////////////////////////////////////////
//  STRACK SOURCE
////////////////////////////////////////////////////////////////

STrackSrc::STrackSrc(STrackRtxTimerScanner& rtx_scanner, STrackLogger* logger,
                     TrafficLogger* pktlogger, 
                     EventList &eventlst)
    : EventSource(eventlst,"strack"),
      _flow(pktlogger),  _pacer(*this, eventlist()), _logger(logger),
     _traffic_logger(pktlogger), _rtx_timer_scanner(&rtx_scanner)
{
    _mss = Packet::data_packet_size();
    _scheduler = NULL;
    _maxcwnd = 0xffffffff;
    _flow_size = ((uint64_t)1)<<63;
    _stop_time = 0;
    _stopped = false;
    _app_limited = -1;
    _highest_sent = 0;
    _packets_sent = 0;
    _established = false;

    // strack cc init

    _base_rtt = 0; // base RTT over which to measure achieved BDP
    _rx_count = 0; // accumulates achieved_BDP
    _achieved_BDP = 0; // amount received in a base_rtt, or zero if invalid
    _last_BDP_update = 0; // the last time to updated achieved_BDP
    _last_active = 0; // the last time we got an ack
    _alpha = 0.25;  // increase constant.  Value from Rong
    _beta = 0.8;   // decrease constant.  Value is a guess
    _max_mdf = 0.5; // max multiplicate decrease factor.  Value is a guess                                                      
    _base_delay = timeFromUs((uint32_t)20);    // configured base target delay.  To be confirmed by experiment - reproduce fig 17
    _h = _base_delay/6.55;            // path length scaling constant.  Value is a guess, will be clarified by experiment
    _rtx_reset_threshold = 5; // value is a guess
    _min_cwnd = 10;  // guess - if we go less than 10 bytes, we probably get into rounding 
    _max_cwnd = 1000 * _mss;  // maximum cwnd we can use.  Guess - how high should we allow cwnd to go?  Presumably something like B*target_delay?

    // flow scaling
    _fs_range = 5 * _base_delay;
    _fs_min_cwnd = 0.1;  // note: in packets
    _fs_max_cwnd = 100;   // note: in packets
    _fs_alpha = _fs_range / ((1.0 / sqrt(_fs_min_cwnd)) -  (1.0 / sqrt(_fs_max_cwnd)));
    _fs_beta = - _fs_alpha / sqrt(_fs_max_cwnd);

    _last_acked = 0;
    _dupacks = 0;
    _rtt = 0;
    _rto = timeFromMs(1);
    _min_rto = timeFromUs((uint32_t)100);
    _mdev = 0;
    _recoverq = 0;
    _in_fast_recovery = false;
    _inflate = 0;
    _drops = 0;

    // strack cc init
    _strack_cwnd = _default_cwnd;  // initial window, in bytes  Note: values in paper are in packets; we're maintaining in bytes.
    _retransmit_cnt = 0;
    _can_decrease = true;
    _last_decrease = 0;  // initial value shouldn't matter if _can_decrease is true
    _pacing_delay = 0;  // start off not pacing as cwnd > 1 pkt
    _deferred_send = false; // true if we wanted to send, but no scheduler said no.

    _rtx_timeout_pending = false;
    _RFC2988_RTO_timeout = timeInf;

    _sink = NULL;
    _nodename = "strack_src" + std::to_string(get_id());
}

void
STrackSrc::update_rtt(simtime_picosec delay) {
    assert(_base_rtt > 0);  // MUST be set before startup
    // calculate TCP-like RTO.  Not clear this is right for STrack
    if (delay!=0){
        if (_rtt>0){
            uint64_t abs;
            if (delay > _rtt)
                abs = delay - _rtt;
            else
                abs = _rtt - delay;

            _mdev = 3 * _mdev / 4 + abs/4;
            _rtt = 7*_rtt/8 + delay/8;
            _rto = _rtt + 4*_mdev;
        } else {
            _rtt = delay;
            _mdev = delay/2;
            _rto = _rtt + 4*_mdev;
        }
    }
    assert(_rtt > _base_rtt); // if it's not, config is probably wrong
    if (_rto <_min_rto)
        _rto = _min_rto;
}

void
STrackSrc::adjust_cwnd(simtime_picosec delay, STrackAck::seq_t ackno) {
    //cout << "adjust_cwnd delay " << timeAsUs(delay) << endl;
    // strack init
    _prev_cwnd = _strack_cwnd;
    simtime_picosec now = eventlist().now(); 
    _can_decrease = (now - _last_decrease) >= _rtt;  // not clear if we should use smoothed RTT here.

    //compute rtt
    update_rtt(delay);

    // STrack cwnd calculation.  Doing this here does it for every ack, no matter if we're in fast recovery or not.  Need to be careful.
    simtime_picosec target_delay = targetDelay(*_route);
    if (delay < target_delay) {
        _strack_cwnd = _alpha * (target_delay - _rtt) / _strack_cwnd;
    } else if (_can_decrease) {
        // don't decrease more than once per RTT
        if (delay > target_delay * 2 && _achieved_BDP > 0) {
            _strack_cwnd = _achieved_BDP;
        } else {
            // multiplicative decrease, as in Swift
            _strack_cwnd = _strack_cwnd * max( 1 - beta() * (delay - target_delay) / delay, 1 - max_mdf());
        }
        _last_decrease = now;
    }
}

void
STrackSrc::applySTrackLimits() {
    // we call this whenever we've changed cwnd to enforce bounds, just before we actually use the cwnd
    if (_strack_cwnd < _min_cwnd) {
        cout << "hit min cwnd, was" << _strack_cwnd << " now " << _min_cwnd << endl;
        _strack_cwnd = _min_cwnd;
    } else if (_strack_cwnd > _max_cwnd) {
        cout << "hit max cwnd " << _strack_cwnd << " > " << _max_cwnd << endl;
        _strack_cwnd = _max_cwnd;
    }
    if (_strack_cwnd < _prev_cwnd) {
        _last_decrease = eventlist().now();
    }
    if (_strack_cwnd < mss()) {
        _pacing_delay = (_rtt * mss())/_strack_cwnd;
        //cout << "strack_cwnd " << ((double)_sub->_strack_cwnd)/_mss << " pacing " << timeAsUs(_pacing_delay) << "us" << endl; 
    } else {
        _pacing_delay = 0;
    }
}

void
STrackSrc::handle_ack(STrackAck::seq_t ackno) {
    simtime_picosec now = eventlist().now();
    if (ackno > _last_acked) { // a brand new ack
        _RFC2988_RTO_timeout = now + _rto;// RFC 2988 5.3
    
        if (ackno >= _highest_sent) {
            _highest_sent = ackno;
            _RFC2988_RTO_timeout = timeInf;// RFC 2988 5.2
        }

        if (!_in_fast_recovery) {
            // best behaviour: proper ack of a new packet, when we were expecting it.
            // clear timers.  strack has already calculated new cwnd.
            _last_acked = ackno;
            _dupacks = 0;
            log(STrackLogger::STRACK_RCV);
            applySTrackLimits();
            send_packets();
            return;
        }
        // We're in fast recovery, i.e. one packet has been
        // dropped but we're pretending it's not serious
        if (ackno >= _recoverq) { 
            // got ACKs for all the "recovery window": resume
            // normal service

            //uint32_t flightsize = _highest_sent - ackno;
            //_cwnd = min(_ssthresh, flightsize + _mss);
            // in NewReno, we'd reset the cwnd here, but I think in strack we continue with the delay-adjusted cwnd.

            _inflate = 0;
            _last_acked = ackno;
            _dupacks = 0;
            _in_fast_recovery = false;

            log(STrackLogger::STRACK_RCV_FR_END);
            _retransmit_cnt = 0;
            if (_can_decrease) {
                _strack_cwnd = (1 - _max_mdf) * _strack_cwnd;
                _last_decrease = now;
            }
            applySTrackLimits();
            send_packets();
            return;
        }
        // In fast recovery, and still getting ACKs for the
        // "recovery window"
        // This is dangerous. It means that several packets
        // got lost, not just the one that triggered FR.
        uint32_t new_data = ackno - _last_acked;
        if (new_data < _inflate) {
            _inflate -= new_data;
        } else {
            _inflate = 0;
        }
        _last_acked = ackno;
        _inflate += mss();
        log(STrackLogger::STRACK_RCV_FR);
        retransmit_packet();
        applySTrackLimits();
        send_packets();
        return;
    }
    // It's a dup ack
    if (_in_fast_recovery) { // still in fast recovery; hopefully the prodigal ACK is on it's way
        _inflate += mss();
        if (_inflate > _strack_cwnd) {
            // this is probably bad
            _inflate = _strack_cwnd;
            cout << "hit inflate limit" << endl;
        }
        log(STrackLogger::STRACK_RCV_DUP_FR);
        send_packets();
        return;
    }
    // Not yet in fast recovery. What should we do instead?
    _dupacks++;

    if (_dupacks!=3)  { // not yet serious worry
        log(STrackLogger::STRACK_RCV_DUP);
        applySTrackLimits();
        send_packets();
        return;
    }
    // _dupacks==3
    if (_last_acked < _recoverq) {  
        /* See RFC 3782: if we haven't recovered from timeouts
           etc. don't do fast recovery */
        log(STrackLogger::STRACK_RCV_3DUPNOFR);
        return;
    }
  
    // begin fast recovery
  
    //only count drops in CA state
    _drops++;
    applySTrackLimits();
    retransmit_packet();
    _in_fast_recovery = true;
    _recoverq = _highest_sent; // _recoverq is the value of the
    // first ACK that tells us things
    // are back on track
    log(STrackLogger::STRACK_RCV_DUP_FASTXMIT);
}
        
// Note: the data sequence number is the number of Byte1 of the packet, not the last byte.
int 
STrackSrc::send_packets() {
    uint32_t c = _strack_cwnd + _inflate;
    int sent_count = 0;
    //cout << eventlist().now() << " " << nodename() << " cwnd " << _strack_cwnd << " + " << _inflate << endl;
    if (!_established){
        //send SYN packet and wait for SYN/ACK
        Packet * p  = STrackPacket::new_syn_pkt(_flow, *(_route), 0, 1);
        _highest_sent = 0;

        p->sendOn();

        if(_RFC2988_RTO_timeout == timeInf) {// RFC2988 5.1
            _RFC2988_RTO_timeout = eventlist().now() + _rto;
        }        
        //cout << "Sending SYN, waiting for SYN/ACK" << endl;
        return sent_count;
    }

    if (c < mss()) {
        // cwnd is too small to send one packet per RTT, so we will be in pacing mode
        assert(_established);
        //cout << eventlist().now() << " " << nodename() << " sub-packet cwnd!" << endl;

        // Enter pacing mode if we're not already there. If we are in
        // pacing mode, we don't reschedule - _pacing_delay will only
        // be applied for the next packet.  This is intended to mirror
        // what happens with the carosel, where a sent time is
        // calculated and then stuck to.  It might make more sense to
        // reschedule, as we've more recent information, but seems
        // like this isn't what Google does with hardware pacing.
        
        if (!_pacer.is_pending()) {
            _pacer.schedule_send(_pacing_delay);
            //cout << eventlist().now() << " " << nodename() << " pacer set for " << timeAsUs(_pacing_delay) << "us" << endl;

            // xxx this won't work with app_limited senders.  Fix this
            // if we want to simulate app limiting with pacing.
            assert(_app_limited == -1);
        }
        return sent_count;
    }
    
    if (_app_limited >= 0 && _rtt > 0) {
        uint64_t d = (uint64_t)_app_limited * _rtt/1000000000;
        if (c > d) {
            c = d;
        }

        if (c==0){
            //      _RFC2988_RTO_timeout = timeInf;
        }
    }

    while ((_last_acked + c >= _highest_sent + mss()) && more_data_available()) {

        if (_pacer.is_pending()) {
            //Our cwnd is now greater than one packet and we've passed
            //the tests to send in window mode, but we were in pacing
            //mode.  Cancel the pacing and return to window mode.
            _pacer.cancel();
        }
        bool sent = send_next_packet();
        if (sent) {
            sent_count++;
        } else {
            break;
        }

        if(_RFC2988_RTO_timeout == timeInf) {// RFC2988 5.1
            _RFC2988_RTO_timeout = eventlist().now() + _rto;
            //cout << timeAsUs(eventlist().now()) << " " << nodename() << " RTO at " << timeAsUs(_RFC2988_RTO_timeout) << "us" << endl;
        }
    }
    return sent_count;
}

bool
STrackSrc::send_next_packet() {
    // ask the scheduler if we can send now
    if (queuesize(_flow.flow_id()) > 2) {
        // no, we can't send.  We'll be called back when we can.
        _deferred_send = true;
        return false;
    }
    _deferred_send = false;
    //cout << "src " << get_id() << " sending " << _flow.flow_id() << " route " << _route << endl;
    STrackPacket* p = STrackPacket::newpkt(_flow, *_route, _highest_sent+1, mss());
    //cout << timeAsUs(eventlist().now()) << " " << nodename() << " sent " << _highest_sent+1 << "-" << _highest_sent+mss() << " dsn " << dsn << endl;
    _highest_sent += mss();  
    _packets_sent += mss();

    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);
    p->set_ts(eventlist().now());
    p->sendOn();
    _pacer.just_sent();
    return true;
}

void
STrackSrc::send_callback() {
    if (_deferred_send == false) {
        // no need to be here
        return;
    }

    _deferred_send = false;
    // We had previously wanted to send but queue said no.  Now it says yes.
    send_packets();
}

void 
STrackSrc::retransmit_packet() {
    cout << timeAsUs(eventlist().now()) << " " << nodename() << " retransmit_packet " << endl;
    if (!_established){
        assert(_highest_sent == 1);

        Packet* p  = STrackPacket::new_syn_pkt(_flow, *_route, 1, 1);
        p->sendOn();

        cout << "Resending SYN, waiting for SYN/ACK" << endl;
        return;        
    }
    //cout << timeAsUs(eventlist().now()) << " sending seqno " << _last_acked+1 << endl;
    STrackPacket* p = STrackPacket::newpkt(_flow, *_route, _last_acked+1, mss());

    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);
    p->set_ts(eventlist().now());
    p->sendOn();

    _packets_sent += mss();

    if(_RFC2988_RTO_timeout == timeInf) {// RFC2988 5.1
        _RFC2988_RTO_timeout = eventlist().now() + _rto;
    }
}

void
STrackSrc::receivePacket(Packet& pkt) 
{
    simtime_picosec ts_echo;
    STrackAck *p = (STrackAck*)(&pkt);
    STrackAck::seq_t ackno = p->ackno();
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);
  
    ts_echo = p->ts_echo();
    p->free();

    if (ackno < _last_acked) {
        //cout << "O seqno" << ackno << " last acked "<< _sub->_last_acked;
        return;
    }

    /// XXX this assumes exactly one ack received for each data packet received
    // If acks are lost, this will be wrong.
    uint32_t acked_bytes = _mss; // total number acked this time. 

    if (ackno==0){
        _established = true;
    } else if (ackno>0 && !_established) {
        cout << "Should be _established " << ackno << endl;
        assert(false);
    }
    _rx_count += acked_bytes;
    simtime_picosec now = eventlist().now();
    if (now - _last_BDP_update > _base_rtt) {
        // it's time to update our achieved_BDP
        if (now - _last_active < _base_rtt) {
            _achieved_BDP = _rx_count;
            _rx_count = 0;
        } else {
            // we didn't get any acks in the last RTT, so achieved BDP is not valid.
            _achieved_BDP = 0; 
            _rx_count = acked_bytes;
        }
        _last_BDP_update = now;
    }
    _last_active = now;

    assert(ackno >= _last_acked);  // no dups or reordering allowed in this simple simulator
    simtime_picosec delay = eventlist().now() - ts_echo;
    adjust_cwnd(delay, ackno);

    handle_ack(ackno);
}

void
STrackSrc::rtx_timer_hook(simtime_picosec now, simtime_picosec period) {
    //cout << timeAsUs(eventlist().now()) << " " << nodename() << " rtx_timer_hook" << endl;
    if (now <= _RFC2988_RTO_timeout || _RFC2988_RTO_timeout==timeInf) 
        return;

    if (_highest_sent == 0) 
        return;

    cout << timeAsUs(eventlist().now()) << " " << nodename() << " At " << now/(double)1000000000<< " RTO " << _rto/1000000000 << " MDEV " 
         << _mdev/1000000000 << " RTT "<< _rtt/1000000000 << " SEQ " << _last_acked / mss() << " HSENT "  << _highest_sent 
         << " CWND "<< _strack_cwnd/mss() << " FAST RECOVERY? " <<         _in_fast_recovery << " Flow ID " 
         << str()  << endl;

    // here we can run into phase effects because the timer is checked
    // only periodically for ALL flows but if we keep the difference
    // between scanning time and real timeout time when restarting the
    // flows we should minimize them !
    if(!_rtx_timeout_pending) {
        _rtx_timeout_pending = true;

        // check the timer difference between the event and the real value
        simtime_picosec too_late = now - (_RFC2988_RTO_timeout);
 
        // careful: we might calculate a negative value if _rto suddenly drops very much
        // to prevent overflow but keep randomness we just divide until we are within the limit
        while(too_late > period) too_late >>= 1;

        // carry over the difference for restarting
        simtime_picosec rtx_off = (period - too_late)/200;
 
        eventlist().sourceIsPendingRel(*this, rtx_off);

        //reset our rtx timerRFC 2988 5.5 & 5.6

        _rto *= 2;
        //if (_rto > timeFromMs(1000))
        //  _rto = timeFromMs(1000);
        _RFC2988_RTO_timeout = now + _rto;
    }
}

void STrackSrc::doNextEvent() {
    cout << "src " << get_id() << " doNextEvent" << endl;
    if(_rtx_timeout_pending) {
        _rtx_timeout_pending = false;

        log(STrackLogger::STRACK_TIMEOUT);

        _in_fast_recovery = false;
        _recoverq = _highest_sent;

        if (_established)
            _highest_sent = _last_acked + mss();

        _dupacks = 0;

        // This backoff-on-retransmit mechanism comes from Swift.
        // XXX Do we need it for STrack?  
        _retransmit_cnt++;
        if (_retransmit_cnt >= _rtx_reset_threshold) {
            _strack_cwnd = _min_cwnd;
        } else if (eventlist().now() - _last_decrease >= _rtt) {
            _strack_cwnd *= (1.0 - _max_mdf);
        }

        retransmit_packet();
    } else {
        cout << "Starting flow, src" << get_id() << " route " << _route << endl;
        startflow();
    }        
}

void
STrackSrc::reroute(const Route &routeout) {
    Route* new_route = routeout.clone();
    new_route->push_back(_sink);
    _route = new_route;
}

void
STrackSrc::log(STrackLogger::STrackEvent event) {
    if (_logger) 
        _logger->logSTrack(*this, event);
}

int
STrackSrc::queuesize(int flow_id) {
    return _scheduler->src_queuesize(flow_id);
}

bool
STrackSrc::check_stoptime() {
    if (_stop_time && eventlist().now() >= _stop_time) {
        _stopped = true;
        _stop_time = 0;
    }
    if (_stopped) {
        return true;
    }
    return false;
}

void STrackSrc::set_app_limit(int pktps) {
    if (_app_limited==0 && pktps){
        _strack_cwnd = _mss;
    }
    _app_limited = pktps;
    send_packets();
}

void
STrackSrc::set_cwnd(uint32_t cwnd) {
    _strack_cwnd = cwnd;
    _default_cwnd = cwnd;
}

void
STrackSrc::set_hdiv(double hdiv) {
    _h = _base_delay/hdiv;            // path length scaling constant.  Value is a guess, will be clarified by experiment
}

void
STrackSrc::set_paths(vector<const Route*>* rt_list){
    size_t no_of_paths = rt_list->size();
    _paths.resize(no_of_paths);
    for (size_t i=0; i < no_of_paths; i++){
        Route* rt_tmp = new Route(*(rt_list->at(i)));
        if (!_scheduler) {
            _scheduler = dynamic_cast<BaseScheduler*>(rt_tmp->at(0));
            assert(_scheduler);
        } else {
            // sanity check all paths share the same scheduler.
            // If we ever want to use multiple NICs, this will need fixing
            assert(_scheduler == dynamic_cast<BaseScheduler*>(rt_tmp->at(0)));
        }
        rt_tmp->set_path_id(i, rt_list->size());
        _paths[i] = rt_tmp;
    }
    permute_paths();
    _path_index = 0;
    reroute(*_paths[0]);
}

void
STrackSrc::permute_paths() {
    // Fisher-Yates shuffle
    size_t len = _paths.size();
    for (size_t i = 0; i < len; i++) {
        size_t ix = random() % (len - i);
        const Route* tmppath = _paths[ix];
        _paths[ix] = _paths[len-1-i];
        _paths[len-1-i] = tmppath;
    }
}

void 
STrackSrc::startflow() {
    if (_established)
        return; // don't start twice
    _established = true; // send data from the start
    send_packets();
}

bool STrackSrc::more_data_available() const {
    if (_stop_time && eventlist().now() >= _stop_time) {
        return false;
    }
    return _highest_sent + mss() <= _flow_size;
}

void 
STrackSrc::connect(const Route& routeout, const Route& routeback, STrackSink& sink, 
                   simtime_picosec starttime) {

    _scheduler = dynamic_cast<BaseScheduler*>(routeout.at(0));
    assert(_scheduler); // first element of the route must be a Src Scheduler

    // Note: if we call set_paths after connect, this route will not (immediately) be used
    // sub->connect(sink, routeout, routeback, get_id(), _scheduler);
    sink.connect(*this, routeback);
    Route* cloned_rt = routeout.clone(); // make a copy, as we may be switching routes
                                         // and don't want to append the sink more than once
    cloned_rt->push_back(&sink);
    _route = cloned_rt;
    _flow.set_id(get_id()); // identify the packet flow with the source that generated it
    cout << "connect, src " << get_id() << " flow id is now " << _flow.get_id()  << endl;
    _scheduler->add_src(_flow.flow_id(), this);
    _rtx_timer_scanner->registerSrc(*this);
    _sink=&sink;

    eventlist().sourceIsPending(*this,starttime);
    cout << "starttime " << timeAsUs(starttime) << endl;
}


#define ABS(X) ((X)>0?(X):-(X))

simtime_picosec
STrackSrc::targetDelay(const Route& route) {
    /*
    // note fs_delay can be negative, so don't use simtime_picosec here!
    double fs_delay = _fs_alpha/sqrt(cwnd/_mss) + _fs_beta;  // _sub->_strack_cwnd is in bytes
    //cout << "fs_delay " << fs_delay << " range " << _fs_range << " _sub->_strack_cwnd/_mss " << _sub->_strack_cwnd/_mss << " sqrt " << sqrt(_sub->_strack_cwnd/_mss) << " beta " << _fs_beta << endl;
    //cout << "fs_alpha " << _fs_alpha << endl;
    
    if (fs_delay > _fs_range) {
        fs_delay = _fs_range;
    }
    if (fs_delay < 0.0) {
        fs_delay = 0.0;
    }

    if (cwnd == 0) {
        fs_delay = 0.0;
    }
    */
    simtime_picosec hop_delay = route.hop_count() * _h;
    return _base_delay + hop_delay;
}


////////////////////////////////////////////////////////////////
//  STRACK PACER
////////////////////////////////////////////////////////////////

/* XXX currently it's one src per pacer - maybe should be shared via carousel */
STrackPacer::STrackPacer(STrackSrc& src, EventList& event_list)
    : EventSource(event_list,"strack_pacer"), _src(&src), _interpacket_delay(0) {
    _last_send = eventlist().now();
}

void
STrackPacer::schedule_send(simtime_picosec delay) {
    _interpacket_delay = delay;
    _next_send = _last_send + _interpacket_delay;
    if (_next_send <= eventlist().now()) {
        // Tricky!  We're going in to pacing mode, but it's more than
        // the pacing delay since we last sent.  Presumably the best
        // thing is to immediately send, and then pacing will kick in
        // next time round.
        _next_send = eventlist().now();
        doNextEvent();
        return;
    }
    eventlist().sourceIsPending(*this, _next_send);
}

void
STrackPacer::cancel() {
    _interpacket_delay = 0;
    _next_send = 0;
    eventlist().cancelPendingSource(*this);
}

// called when we're in window-mode to update the send time so it's always correct if we
// then go into paced mode
void
STrackPacer::just_sent() {
    _last_send = eventlist().now();
}

void
STrackPacer::doNextEvent() {
    assert(eventlist().now() == _next_send);
    _src->send_next_packet();
    _last_send = eventlist().now();
    //cout << "sending paced packet" << endl;

    if (_src->pacing_delay() > 0) {
        // schedule the next packet send
        schedule_send(_src->pacing_delay());
        //cout << "rescheduling send " << timeAsUs(_src->_pacing_delay) << "us" << endl;
    } else {
        // the src is no longer in pacing mode, but we didn't get a
        // cancel.  A bit odd, but drop out of pacing.  Should we have
        // sent here?  Could add the check before sending?
        _interpacket_delay = 0;
        _next_send = 0;
    }
}


////////////////////////////////////////////////////////////////
//  STRACK SINK
////////////////////////////////////////////////////////////////

STrackSink::STrackSink() 
    : DataReceiver("STrackSink"), _cumulative_ack(0), _buffer_logger(NULL)
{
    _src = 0;
    _ooo = 0;
    _nodename = "stracksink";
}

void
STrackSink::connect(STrackSrc& src, const Route& route_back) {
    _src = &src;
    Route* route = route_back.clone();
    route->push_back(&src);
    _route = route;
    _cumulative_ack = 0;
    _total_received = 0;
}

// Note: _cumulative_ack is the last byte we've ACKed.
// seqno is the first byte of the new packet.
void
STrackSink::receivePacket(Packet& pkt) {
    STrackPacket *p = (STrackPacket*)(&pkt);
    STrackPacket::seq_t seqno = p->seqno();
    simtime_picosec ts = p->ts();
    //cout << "receivePacket seq" << seqno << endl;
    int size = p->size(); // TODO: the following code assumes all packets are the same size
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);

    if (seqno == _cumulative_ack+1) { // it's the next expected seq no
        _cumulative_ack = seqno + size - 1;
        _total_received += size;
        // are there any additional received packets we can now ack?
        while (!_received.empty() && (_received.front() == _cumulative_ack+1) ) {
            _received.pop_front();
            _cumulative_ack += size;
            _total_received += size;
            if (_buffer_logger) _buffer_logger->logBuffer(ReorderBufferLogger::BUF_DEQUEUE);
        }
    } else if (seqno < _cumulative_ack+1) {
        // it is before the next expected sequence - must be a spurious retransmit.
        // We want to see if this happens - it generally shouldn't
        cout << "Spurious retransmit received!\n";
    } else {
        // it's not the next expected sequence number
        if (_received.empty()) {
            _received.push_front(seqno);
        } else if (seqno > _received.back()) {
            // likely case - new packet above a hole
            _received.push_back(seqno);
        } else {
            // uncommon case - it fills a hole, but not first hole
            list<uint64_t>::iterator i;
            for (i = _received.begin(); i != _received.end(); i++) {
                if (seqno == *i) break; // it's a bad retransmit
                if (seqno < (*i)) {
                    _received.insert(i, seqno);
                    if (_buffer_logger) _buffer_logger->logBuffer(ReorderBufferLogger::BUF_ENQUEUE);
                    break;
                }
            }
        }
    }
    if (_ooo < _received.size())
        _ooo = _received.size();

    p->free();

    // whatever the cumulative ack does (eg filling holes), the echoed TS is always from
    // the packet we just received
    send_ack(ts);
}

void 
STrackSink::send_ack(simtime_picosec ts) {
    STrackAck *ack = STrackAck::newpkt(_src->flow(), *_route, 0, _cumulative_ack, ts);
    ack->flow().logTraffic(*ack,*this,TrafficLogger::PKT_CREATESEND);
    ack->sendOn();
}

uint64_t
STrackSink::cumulative_ack() {
    // this is needed by some loggers.  If we ever need it, figure out what it should really return
    return _src ? _cumulative_ack + _src->_mss : 0;
} 

uint32_t
STrackSink::drops() {
    return _src ? _src->drops() : 0;
}

////////////////////////////////////////////////////////////////
//  STRACK RETRANSMISSION TIMER
////////////////////////////////////////////////////////////////

STrackRtxTimerScanner::STrackRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist)
    : EventSource(eventlist,"RtxScanner"), _scanPeriod(scanPeriod) {
    eventlist.sourceIsPendingRel(*this, _scanPeriod);
}

void 
STrackRtxTimerScanner::registerSrc(STrackSrc &src) {
    _srcs.push_back(&src);
}

void STrackRtxTimerScanner::doNextEvent() {
    simtime_picosec now = eventlist().now();
    srcs_t::iterator i;
    for (i = _srcs.begin(); i!=_srcs.end(); i++) {
        (*i)->rtx_timer_hook(now,_scanPeriod);
    }
    eventlist().sourceIsPendingRel(*this, _scanPeriod);
}
