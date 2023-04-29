// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#include "swift.h"
#include <iostream>
#include <math.h>

#define KILL_THRESHOLD 5
////////////////////////////////////////////////////////////////
//  SWIFT SUBFLOW SOURCE
////////////////////////////////////////////////////////////////
uint32_t SwiftSubflowSrc::_default_cwnd = 12;

SwiftSubflowSrc::SwiftSubflowSrc(SwiftSrc& src, TrafficLogger* pktlogger, int sub_id)
    : EventSource(src.eventlist(), "swift_subflow_src"), _flow(pktlogger), _src(src), _pacer(*this, src.eventlist())
{
    cout << "subflow src constructor\n";
    _subflow_sink = NULL;
    _highest_sent = 0;
    _packets_sent = 0;
    _established = false;

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

    // swift cc init
    _swift_cwnd = _default_cwnd;  // initial window, in bytes  Note: values in paper are in packets; we're maintaining in bytes.
    _retransmit_cnt = 0;
    _can_decrease = true;
    _last_decrease = 0;  // initial value shouldn't matter if _can_decrease is true
    _pacing_delay = 0;  // start off not pacing as cwnd > 1 pkt
    _deferred_send = false; // true if we wanted to send, but no scheduler said no.

    // PLB init
    _plb_interval = timeFromSec(1);  // disable PLB til we've seen some traffic
    _path_index = 0;  
    _last_good_path = src.eventlist().now();
    
    _rtx_timeout_pending = false;
    _RFC2988_RTO_timeout = timeInf;

    _nodename = "swift_subsrc" + std::to_string(_src.get_id()) + "_" + std::to_string(sub_id);
}

void
SwiftSubflowSrc::update_rtt(simtime_picosec delay) {
    // calculate TCP-like RTO.  Not clear this is right for Swift
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

    if (_rto <_min_rto)
        _rto = _min_rto;
}

void
SwiftSubflowSrc::adjust_cwnd(simtime_picosec delay, SwiftAck::seq_t ackno) {
    //cout << "adjust_cwnd delay " << timeAsUs(delay) << endl;
    // swift init
    _prev_cwnd = _swift_cwnd;
    simtime_picosec now = eventlist().now(); 
    _can_decrease = (now - _last_decrease) >= _rtt;  // not clear if we should use smoothed RTT here.
    //can_decrease = true;

    //compute rtt
    update_rtt(delay);

    // Swift cwnd calculation.  Doing this here does it for every ack, no matter if we're in fast recovery or not.  Need to be careful.
    _retransmit_cnt = 0;
    simtime_picosec target_delay = _src.targetDelay(_swift_cwnd, *_route);
    //cout << timeAsUs(now) << " delay (us)" << timeAsUs(delay) << " target " << timeAsUs(target_delay) << endl;
    if (delay < target_delay) {
        //cout << "ackno " << ackno << " cwnd " << _swift_cwnd << " inf " <<  _inflate << " last_acked " <<  _last_acked << endl;
        int num_acked = ackno - _last_acked;
        //cout << "num_acked " << num_acked << endl;
        if (num_acked < 0)
            num_acked = 0;  // doesn't make sense to do additive increase with negative num_acked.  
        if (_swift_cwnd >= mss()) {
            _swift_cwnd = _swift_cwnd + (mss() * ai() * num_acked) / _swift_cwnd;
        } else {
            _swift_cwnd = _swift_cwnd + ai() * num_acked;
        }
    } else if (_can_decrease) {
        // multiplicative decrease
        _swift_cwnd = _swift_cwnd * max( 1 - beta() * (delay - target_delay) / delay, 1 - max_mdf());
    }

    
    if (_src.plb()) {
        simtime_picosec td = _src.targetDelay(0, *_route); // target delay ignoring cwnd component
        //cout << nodename() << " delay " << delay << "rep td " << td << endl;
        if (delay <= td) {
            // good delay!
            _last_good_path = now;
            _plb_interval = random()%(2*_rtt) + 5*_rtt;
        }

        // PLB (simple version)
        if (now - _last_good_path > _plb_interval) {
            cout << "moving " << timeAsUs(now) << " last_good " << timeAsUs(_last_good_path) << " RTT " << timeAsUs(_rtt) << endl;
            _last_good_path = now;
            move_path();
        }
    }
    /*
      if (_src.plb()) {
      simtime_picosec td = _src.targetDelay(0, *_route); // target delay ignoring cwnd component
      //cout << nodename() << " delay " << delay << "rep td " << td << endl;
      if (delay > td) {
      if (now - _last_good_path > _rtt) {
      _last_good_path = now;
      _decrease_count++;
      //cout << "RTT: " << timeAsMs(_rtt) << endl;
      }
      } else {
      // good delay!
      _last_good_path = now;
      _decrease_count = 0;
      }

      // PLB (simple version)
      if (_decrease_count > 5) {
      _decrease_count = 0;
      _last_good_path = now;
      move_path();
      }
      }
    */
}

void
SwiftSubflowSrc::applySwiftLimits() {
    // we call this whenever we've changed cwnd to enforce bounds, just before we actually use the cwnd
    if (_swift_cwnd < _src._min_cwnd) {
        cout << "hit min cwnd, was" << _swift_cwnd << " now " << _src._min_cwnd << endl;
        _swift_cwnd = _src._min_cwnd;
    } else if (_swift_cwnd > _src._max_cwnd) {
        cout << "hit max cwnd " << _swift_cwnd << " > " << _src._max_cwnd << endl;
        _swift_cwnd = _src._max_cwnd;
    }
    if (_swift_cwnd < _prev_cwnd) {
        _last_decrease = eventlist().now();
    }
    if (_swift_cwnd < mss()) {
        _pacing_delay = (_rtt * mss())/_swift_cwnd;
        //cout << "swift_cwnd " << ((double)_sub->_swift_cwnd)/_mss << " pacing " << timeAsUs(_pacing_delay) << "us" << endl; 
    } else {
        _pacing_delay = 0;
    }
}

void
SwiftSubflowSrc::handle_ack(SwiftAck::seq_t ackno) {
    simtime_picosec now = eventlist().now();
    if (ackno > _last_acked) { // a brand new ack
        _RFC2988_RTO_timeout = now + _rto;// RFC 2988 5.3
    
        if (ackno >= _highest_sent) {
            
            _highest_sent = ackno;
            //cout << timeAsUs(now) << " " << nodename() << " highest_sent now  " << _highest_sent << endl;
            _RFC2988_RTO_timeout = timeInf;// RFC 2988 5.2
        }

        if (!_in_fast_recovery) {
            // best behaviour: proper ack of a new packet, when we were expecting it.
            // clear timers.  swift has already calculated new cwnd.
            _last_acked = ackno;
            _dupacks = 0;
            _src.log(this, SwiftLogger::SWIFT_RCV);
            applySwiftLimits();
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
            // in NewReno, we'd reset the cwnd here, but I think in swift we continue with the delay-adjusted cwnd.

            _inflate = 0;
            _last_acked = ackno;
            _dupacks = 0;
            _in_fast_recovery = false;

            _src.log(this, SwiftLogger::SWIFT_RCV_FR_END);
            _retransmit_cnt = 0;
            if (_can_decrease) {
                _swift_cwnd = (1 - _src._max_mdf) * _swift_cwnd;
                _last_decrease = now;
            }
            applySwiftLimits();
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
        _src.log(this, SwiftLogger::SWIFT_RCV_FR);
        retransmit_packet();
        applySwiftLimits();
        send_packets();
        return;
    }
    // It's a dup ack
    if (_in_fast_recovery) { // still in fast recovery; hopefully the prodigal ACK is on it's way
        _inflate += mss();
        if (_inflate > _swift_cwnd) {
            // this is probably bad
            _inflate = _swift_cwnd;
            cout << "hit inflate limit" << endl;
        }
        _src.log(this, SwiftLogger::SWIFT_RCV_DUP_FR);
        send_packets();
        return;
    }
    // Not yet in fast recovery. What should we do instead?
    _dupacks++;

    if (_dupacks!=3)  { // not yet serious worry
        _src.log(this, SwiftLogger::SWIFT_RCV_DUP);
        applySwiftLimits();
        send_packets();
        return;
    }
    // _dupacks==3
    if (_last_acked < _recoverq) {  
        /* See RFC 3782: if we haven't recovered from timeouts
           etc. don't do fast recovery */
        _src.log(this, SwiftLogger::SWIFT_RCV_3DUPNOFR);
        return;
    }
  
    // begin fast recovery
  
    //only count drops in CA state
    _drops++;
    applySwiftLimits();
    retransmit_packet();
    _in_fast_recovery = true;
    _recoverq = _highest_sent; // _recoverq is the value of the
    // first ACK that tells us things
    // are back on track
    _src.log(this, SwiftLogger::SWIFT_RCV_DUP_FASTXMIT);
}
        
// Note: the data sequence number is the number of Byte1 of the packet, not the last byte.
int 
SwiftSubflowSrc::send_packets() {
    uint32_t c = _swift_cwnd + _inflate;
    int sent_count = 0;
    //cout << eventlist().now() << " " << nodename() << " cwnd " << _swift_cwnd << " + " << _inflate << endl;
    if (!_established){
        //send SYN packet and wait for SYN/ACK
        Packet * p  = SwiftPacket::new_syn_pkt(_flow, *(_route), 0, 1);
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
            assert(_src._app_limited == -1);
        }
        return sent_count;
    }
    
    if (_src._app_limited >= 0 && _rtt > 0) {
        uint64_t d = (uint64_t)_src._app_limited * _rtt/1000000000;
        if (c > d) {
            c = d;
        }

        if (c==0){
            //      _RFC2988_RTO_timeout = timeInf;
        }
    }

    while ((_last_acked + c >= _highest_sent + mss()) && _src.more_data_available()) {

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
SwiftSubflowSrc::send_next_packet() {
    // ask the scheduler if we can send now
    if (_src.queuesize(_flow.flow_id()) > 2) {
        // no, we can't send.  We'll be called back when we can.
        _deferred_send = true;
        return false;
    }
    _deferred_send = false;
    
    // keep track of DSN associated with SeqNo, so we can retransmit
    SwiftPacket::seq_t dsn;
    if (_src.more_data_available()) {
        dsn = _src.get_next_dsn();
    } else {
        // cout << timeAsUs(eventlist().now()) << " " << nodename() << " no more data" << endl;
        return false;
    }

    _dsn_map[_highest_sent+1] = dsn;
    
    SwiftPacket* p = SwiftPacket::newpkt(_flow, *_route, _highest_sent+1, dsn, mss());
    //cout << timeAsUs(eventlist().now()) << " " << nodename() << " sent " << _highest_sent+1 << "-" << _highest_sent+mss() << " dsn " << dsn << endl;
    _highest_sent += mss();  
    _packets_sent += mss();

    p->flow().logTraffic(*p, _src, TrafficLogger::PKT_CREATESEND);
    p->set_ts(eventlist().now());
    p->sendOn();
    _pacer.just_sent();
    return true;
}

void
SwiftSubflowSrc::send_callback() {
    if (_deferred_send == false) {
        // no need to be here
        return;
    }

    _deferred_send = false;
    // We had previously wanted to send but queue said no.  Now it says yes.
    send_packets();
}

void 
SwiftSubflowSrc::retransmit_packet() {
    cout << timeAsUs(eventlist().now()) << " " << nodename() << " retransmit_packet " << endl;
    if (!_established){
        assert(_highest_sent == 1);

        Packet* p  = SwiftPacket::new_syn_pkt(_flow, *_route, 1, 1);
        p->sendOn();

        cout << "Resending SYN, waiting for SYN/ACK" << endl;
        return;        
    }
    SwiftPacket::seq_t dsn = _dsn_map[_last_acked+1];
    //cout << timeAsUs(eventlist().now()) << " sending seqno " << _last_acked+1 << endl;
    SwiftPacket* p = SwiftPacket::newpkt(_flow, *_route, _last_acked+1, dsn, mss());

    p->flow().logTraffic(*p, _src, TrafficLogger::PKT_CREATESEND);
    p->set_ts(eventlist().now());
    p->sendOn();

    _packets_sent += mss();

    if(_RFC2988_RTO_timeout == timeInf) {// RFC2988 5.1
        _RFC2988_RTO_timeout = eventlist().now() + _rto;
    }
}

void
SwiftSubflowSrc::receivePacket(Packet& pkt) 
{
    simtime_picosec ts_echo;
    SwiftAck *p = (SwiftAck*)(&pkt);
    SwiftAck::seq_t ackno = p->ackno();
    SwiftAck::seq_t ds_ackno = p->ds_ackno();
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);
  
    ts_echo = p->ts_echo();
    p->free();

    //cout << timeAsUs(eventlist().now()) << " " << nodename() << " recvack  " << ackno << " ts_echo " << timeAsUs(ts_echo) << endl;
    //cout << timeAsUs(eventlist().now()) << " " << nodename() << " highest_sent was  " << _sub->_highest_sent << endl;

    if (ackno < _last_acked) {
        //cout << "O seqno" << ackno << " last acked "<< _sub->_last_acked;
        return;
    }

    if (ackno==0){
        //assert(!_sub->_established);
        _established = true;
    } else if (ackno>0 && !_established) {
        cout << "Should be _established " << ackno << endl;
        assert(false);
    }

    assert(ackno >= _last_acked);  // no dups or reordering allowed in this simple simulator
    simtime_picosec delay = eventlist().now() - ts_echo;
    adjust_cwnd(delay, ackno);

    _src.update_dsn_ack(ds_ackno);

    handle_ack(ackno);
}

void
SwiftSubflowSrc::rtx_timer_hook(simtime_picosec now, simtime_picosec period) {
    //cout << timeAsUs(eventlist().now()) << " " << nodename() << " rtx_timer_hook" << endl;
    if (now <= _RFC2988_RTO_timeout || _RFC2988_RTO_timeout==timeInf) 
        return;

    if (_highest_sent == 0) 
        return;

    cout << timeAsUs(eventlist().now()) << " " << nodename() << " At " << now/(double)1000000000<< " RTO " << _rto/1000000000 << " MDEV " 
         << _mdev/1000000000 << " RTT "<< _rtt/1000000000 << " SEQ " << _last_acked / mss() << " HSENT "  << _highest_sent 
         << " CWND "<< _swift_cwnd/mss() << " FAST RECOVERY? " <<         _in_fast_recovery << " Flow ID " 
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

void SwiftSubflowSrc::doNextEvent() {
    if(_rtx_timeout_pending) {
        _rtx_timeout_pending = false;

        _src.log(this, SwiftLogger::SWIFT_TIMEOUT);

        /*
          if (_in_fast_recovery) {
          uint32_t flightsize = _highest_sent - _last_acked;
          _cwnd = min(_ssthresh, flightsize + _mss);
          }

          deflate_window();

          _cwnd = _mss;
        */
        _in_fast_recovery = false;
        _recoverq = _highest_sent;

        if (_established)
            _highest_sent = _last_acked + mss();

        _dupacks = 0;
        _retransmit_cnt++;
        if (_retransmit_cnt >= _src._rtx_reset_threshold) {
            _swift_cwnd = _src._min_cwnd;
        } else if (eventlist().now() - _last_decrease >= _rtt) {
            _swift_cwnd *= (1.0 -_src._max_mdf);
        }

        retransmit_packet();
    }
}

void 
SwiftSubflowSrc::connect(SwiftSink& sink, const Route& routeout, const Route& routeback, uint32_t flow_id, BaseScheduler* scheduler) {
    _subflow_sink = sink.connect(_src, *this, routeback);
    Route* new_route = routeout.clone(); // make a copy, as we may be switching routes and don't want to append the sink more than once
    new_route->push_back(_subflow_sink);
    _route = new_route;
    _flow.set_id(get_id()); // identify the packet flow with the source that generated it
    cout << "connect, flow id is now " << _flow.get_id() << endl;
    assert(scheduler);
    scheduler->add_src(_flow.flow_id(), this);
}

void
SwiftSubflowSrc::move_path() {
    cout << timeAsUs(eventlist().now()) << " " << nodename() << " td move_path\n";
    if (_src._paths.size() == 0) {
        cout << nodename() << " cant move_path\n";
        return;
    }
    _path_index++;
    // if we've moved paths so often we've run out of paths, I want to know
    assert(_path_index < _src._paths.size()); 
    Route* new_route = _src._paths[_path_index]->clone();
    new_route->push_back(_subflow_sink);
    _route = new_route;
}

void
SwiftSubflowSrc::reroute(const Route &routeout) {
    Route* new_route = routeout.clone();
    new_route->push_back(_subflow_sink);
    _route = new_route;
}

// cosmetic helper functions (don't want to replicate this data in all subflows) 
EventList& SwiftSubflowSrc::eventlist() const {return _src.eventlist();}
uint16_t SwiftSubflowSrc::mss() const {return _src.mss();}
double SwiftSubflowSrc::ai() const {return _src.ai();}
double SwiftSubflowSrc::beta() const {return _src.beta();}
double SwiftSubflowSrc::max_mdf() const {return _src.max_mdf();}




////////////////////////////////////////////////////////////////
//  SWIFT SOURCE
////////////////////////////////////////////////////////////////

SwiftSrc::SwiftSrc(SwiftRtxTimerScanner& rtx_scanner, SwiftLogger* logger, TrafficLogger* pktlogger, 
                   EventList &eventlst)
    : EventSource(eventlst,"swift"),  _logger(logger), _traffic_logger(pktlogger), _rtx_timer_scanner(&rtx_scanner)
{
    _mss = Packet::data_packet_size();
    _scheduler = NULL;
    _maxcwnd = 0xffffffff;//200*_mss;
    _flow_size = ((uint64_t)1)<<63;
    _stop_time = 0;
    _stopped = false;
    _app_limited = -1;
    _highest_dsn_sent = 0;

    // swift cc init
    _ai = 1.0;  // increase constant.  Value is a guess
    _beta = 0.8;   // decrease constant.  Value is a guess
    _max_mdf = 0.5; // max multiplicate decrease factor.  Value is a guess                                                      
    _base_delay = timeFromUs((uint32_t)20);    // configured base target delay.  To be confirmed by experiment - reproduce fig 17
    _h = _base_delay/6.55;            // path length scaling constant.  Value is a guess, will be clarified by experiment
    _rtx_reset_threshold = 5; // value is a guess
    _min_cwnd = 10;  // guess - if we go less than 10 bytes, we probably get into rounding 
    _max_cwnd = 1000 * _mss;  // maximum cwnd we can use.  Guess - how high should we allow cwnd to go?  Presumably something like B*target_delay?

    // PLB init
    _plb = false; // enable using enable_plb()

    // flow scaling
    _fs_range = 5 * _base_delay;
    _fs_min_cwnd = 0.1;  // note: in packets                                                                                                                         
    _fs_max_cwnd = 100;   // note: in packets
    _fs_alpha = _fs_range / ((1.0 / sqrt(_fs_min_cwnd)) -  (1.0 / sqrt(_fs_max_cwnd)));
    //double a = 1.0 / sqrt(_fs_min_cwnd);
    //double b = 1.0 / sqrt(_fs_max_cwnd);
    //cout << "a " << a << " b " << b << " range " << _fs_range << endl;
    //cout << _fs_range / ((1.0 / sqrt(_fs_min_cwnd)) -  (1.0 / sqrt(_fs_max_cwnd))) << endl;
                
    //cout << "_fs_alpha: " << _fs_alpha << endl;
    _fs_beta = - _fs_alpha / sqrt(_fs_max_cwnd);
}

void
SwiftSrc::log(SwiftSubflowSrc* sub, SwiftLogger::SwiftEvent event) {
    if (_logger) 
        _logger->logSwift(*sub, event);
}

int
SwiftSrc::queuesize(int flow_id) {
    return _scheduler->src_queuesize(flow_id);
}

bool
SwiftSrc::check_stoptime() {
    if (_stop_time && eventlist().now() >= _stop_time) {
        _stopped = true;
        _stop_time = 0;
    }
    if (_stopped) {
        return true;
    }
    return false;
}

void SwiftSrc::set_app_limit(int pktps) {
    if (_app_limited==0 && pktps){
        for (size_t i = 0; i < _subs.size(); i++) {
            _subs[i]->_swift_cwnd = _mss;
        }
    }
    _app_limited = pktps;
    // XXX not clear what to do here for app_limited multipath
    for (size_t i = 0; i < _subs.size(); i++) {
        _subs[i]->send_packets();
    }
}

void
SwiftSrc::set_cwnd(uint32_t cwnd) {
    for (size_t i = 0; i < _subs.size(); i++) {
        _subs[i]->_swift_cwnd = cwnd;
        // cout << eventlist().now() << " " << _subs[i]->nodename() << " set_cwnd, cwnd " << _subs[i]->_swift_cwnd << endl;
    }
    SwiftSubflowSrc::_default_cwnd = cwnd;
}

void
SwiftSrc::set_hdiv(double hdiv) {
    _h = _base_delay/hdiv;            // path length scaling constant.  Value is a guess, will be clarified by experiment
}

void
SwiftSrc::set_paths(vector<const Route*>* rt_list){
    size_t no_of_paths = rt_list->size();
    _paths.resize(no_of_paths);
    for (size_t i=0; i < no_of_paths; i++){
        Route* rt_tmp = new Route(*(rt_list->at(i)));
        if (!_scheduler) {
            _scheduler = dynamic_cast<BaseScheduler*>(rt_tmp->at(0));
            assert(_scheduler);
        } else {
            // sanity check all paths share the same scheduler.  If we ever want to use multiple NICs, this will need fixing
            assert(_scheduler == dynamic_cast<BaseScheduler*>(rt_tmp->at(0)));
        }
        rt_tmp->set_path_id(i, rt_list->size());
        _paths[i] = rt_tmp;
    }
    permute_paths();
    for (size_t i = 0; i < _subs.size(); i++) {
        _subs[i]->_path_index = 0;
        _subs[i]->reroute(*_paths[i]);
    }
}

void
SwiftSrc::permute_paths() {
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
SwiftSrc::startflow() {
    for (size_t i = 0; i < _subs.size(); i++) {
        if (_subs[i]->_established)
            continue; // don't start twice
        _subs[i]->_established = true; // send data from the start
        //cout << eventlist().now() << " " << _subs[i]->nodename() << " (sub" << i << ") started, cwnd " << _subs[i]->_swift_cwnd << endl;
        _subs[i]->send_packets();
    }
}

bool SwiftSrc::more_data_available() const {
    if (_stop_time && eventlist().now() >= _stop_time) {
        return false;
    }
    return _highest_dsn_sent + mss() <= _flow_size;
}

void 
SwiftSrc::connect(const Route& routeout, const Route& routeback, SwiftSink& sink, 
                  simtime_picosec starttime) {

    _scheduler = dynamic_cast<BaseScheduler*>(routeout.at(0));
    assert(_scheduler); // first element of the route must be a SwiftScheduler

    SwiftSubflowSrc* sub = new SwiftSubflowSrc(*this, _traffic_logger, _subs.size());
    _subs.push_back(sub);
    // Note: if we call set_paths after connect, this route will not (immediately) be used
    sub->connect(sink, routeout, routeback, get_id(), _scheduler);
    _rtx_timer_scanner->registerSubflow(*sub);
    _sink=&sink;

    eventlist().sourceIsPending(*this,starttime);
    // cout << "starttime " << timeAsUs(starttime) << endl;
}

void 
SwiftSrc::multipath_connect(SwiftSink& sink, simtime_picosec starttime, uint32_t no_of_subflows) {
    _sink=&sink;
    for (uint32_t i = 0; i < no_of_subflows; i++) {
        SwiftSubflowSrc* subflow = new SwiftSubflowSrc(*this, _traffic_logger, _subs.size());
        _subs.push_back(subflow);
        assert(_paths.size() > 0);
        if (_paths.size() < no_of_subflows) {
            cout << "More subflows requested than paths available - there will be duplicates\n";
        }
        const Route* routeout = _paths[i%_paths.size()];
        const Route* routeback = _paths[i%_paths.size()]->reverse();
        assert(routeback);
        subflow->connect(sink, *routeout, *routeback, get_id(), _scheduler);
        _rtx_timer_scanner->registerSubflow(*subflow);
    }
    eventlist().sourceIsPending(*this,starttime);
    // cout << "starttime " << timeAsUs(starttime) << endl;
}

#define ABS(X) ((X)>0?(X):-(X))

simtime_picosec
SwiftSrc::targetDelay(uint32_t cwnd, const Route& route) {
    // note fs_delay can be negative, so don't use simtime_picosec here!
    double fs_delay = _fs_alpha/sqrt(cwnd/_mss) + _fs_beta;  // _sub->_swift_cwnd is in bytes
    //cout << "fs_delay " << fs_delay << " range " << _fs_range << " _sub->_swift_cwnd/_mss " << _sub->_swift_cwnd/_mss << " sqrt " << sqrt(_sub->_swift_cwnd/_mss) << " beta " << _fs_beta << endl;
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

    simtime_picosec hop_delay = route.hop_count() * _h;
    return _base_delay + fs_delay + hop_delay;
}

void SwiftSrc::update_dsn_ack(SwiftAck::seq_t ds_ackno) {
    //cout << "Flow " << _name << " dsn ack " << ds_ackno << endl;
    if (ds_ackno >= _flow_size){
        cout << "Flow " << _name << " finished at " << timeAsUs(eventlist().now()) << " total bytes " << ds_ackno << endl;
    }
}

uint32_t
SwiftSrc::drops() {
    int drops = 0;
    for (size_t i = 0; i < _subs.size(); i++) {
        drops += _subs[i]->_drops;
    }
    return drops;
}


void SwiftSrc::doNextEvent() {
    // cout << "Starting flow" << endl;
    startflow();
}

////////////////////////////////////////////////////////////////
//  SWIFT PACER
////////////////////////////////////////////////////////////////

/* XXX currently it's one subflow per pacer - maybe should be shared via carousel */
SwiftPacer::SwiftPacer(SwiftSubflowSrc& sub, EventList& event_list)
    : EventSource(event_list,"swift_pacer"), _sub(&sub), _interpacket_delay(0) {
    _last_send = eventlist().now();
}

void
SwiftPacer::schedule_send(simtime_picosec delay) {
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
SwiftPacer::cancel() {
    _interpacket_delay = 0;
    _next_send = 0;
    eventlist().cancelPendingSource(*this);
}

// called when we're in window-mode to update the send time so it's always correct if we
// then go into paced mode
void
SwiftPacer::just_sent() {
    _last_send = eventlist().now();
}

void
SwiftPacer::doNextEvent() {
    assert(eventlist().now() == _next_send);
    _sub->send_next_packet();
    _last_send = eventlist().now();
    //cout << "sending paced packet" << endl;

    if (_sub->pacing_delay() > 0) {
        // schedule the next packet send
        schedule_send(_sub->pacing_delay());
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
//  SWIFT SUBFLOW SINK
////////////////////////////////////////////////////////////////

SwiftSubflowSink::SwiftSubflowSink(SwiftSink& sink) 
    : DataReceiver("subflow_sink"), _cumulative_ack(0), _packets(0), _sink(sink)
{
}

void 
SwiftSubflowSink::connect(SwiftSubflowSrc& src, const Route& route_back) {
    _subflow_src = &src;
    Route *rt = route_back.clone();
    rt->push_back(&src);
    _route = rt;
    _cumulative_ack = 0;
    _drops = 0;
}

// Note: _cumulative_ack is the last byte we've ACKed.
// seqno is the first byte of the new packet.
void
SwiftSubflowSink::receivePacket(Packet& pkt) {
    SwiftPacket *p = (SwiftPacket*)(&pkt);
    SwiftPacket::seq_t seqno = p->seqno();
    simtime_picosec ts = p->ts();
    //cout << "receivePacket seq" << seqno << endl;
    int size = p->size(); // TODO: the following code assumes all packets are the same size
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);

    _packets+= p->size();

    if (seqno == _cumulative_ack+1) { // it's the next expected seq no
        _cumulative_ack = seqno + size - 1;
        // are there any additional received packets we can now ack?
        while (!_received.empty() && (_received.front() == _cumulative_ack+1) ) {
            _received.pop_front();
            _cumulative_ack += size;
        }
    } else if (seqno < _cumulative_ack+1) {
        // it is before the next expected sequence - must be a spurious retransmit.
        // We want to see if this happens - it generally shouldn't
        cout << "Spurious retransmit received!\n";
    } else {
        // it's not the next expected sequence number
        if (_received.empty()) {
            _received.push_front(seqno);
            //it's a drop - in this simulator there are no reorderings.
            // [Note: if we ever add multipath, fix this!]
            _drops += (size + seqno-_cumulative_ack-1)/size;
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
                    break;
                }
            }
        }
    }

    _sink.receivePacket(pkt);
    p->free();

    // whatever the cumulative ack does (eg filling holes), the echoed TS is always from
    // the packet we just received
    send_ack(ts);
}

void 
SwiftSubflowSink::send_ack(simtime_picosec ts) {
    const Route* rt = _route;
    
    SwiftAck *ack = SwiftAck::newpkt(_subflow_src->flow(), *rt, 0, _cumulative_ack, _sink._cumulative_data_ack, ts);

    ack->flow().logTraffic(*ack,*this,TrafficLogger::PKT_CREATESEND);
    ack->sendOn();
}

const string&
SwiftSubflowSink::nodename() {
    return _sink.nodename();
}

uint64_t
SwiftSubflowSink::cumulative_ack() {
    // this is needed by some loggers.  If we ever need it, figure out what it should really return
    if (_sink._src) {
        return _cumulative_ack + _received.size()*(_sink._src->_mss);
    } else {
        return 0;
    }
} 

uint32_t
SwiftSubflowSink::drops(){
    if (_sink._src) {
        return _sink._src->drops();
    } else {
        return 0;
    }
}


////////////////////////////////////////////////////////////////
//  SWIFT SINK
////////////////////////////////////////////////////////////////

SwiftSink::SwiftSink() 
    : DataReceiver("SwiftSink"), _cumulative_data_ack(0), _buffer_logger(NULL)
{
    _src = 0;
    _nodename = "swiftsink";
}

SwiftSubflowSink*
SwiftSink::connect(SwiftSrc& src, SwiftSubflowSrc& subflow_src, const Route& route_back) {
    _src = &src;
    SwiftSubflowSink* subflow_sink = new SwiftSubflowSink(*this);
    _subs.push_back(subflow_sink);
    subflow_sink->connect(subflow_src, route_back);
    return subflow_sink;
}

void
SwiftSink::receivePacket(Packet& pkt) {
    SwiftPacket *p = (SwiftPacket*)(&pkt);
    SwiftPacket::seq_t dsn = p->dsn();
    int size = p->size(); // TODO: the following code assumes all packets are the same size
    //cout << "SwiftSink received dsn " << dsn << endl;
    if (dsn == _cumulative_data_ack+1) {
        _cumulative_data_ack = dsn + size - 1;
        while (!_dsn_received.empty() && (*(_dsn_received.begin()) == _cumulative_data_ack+1) ) {
            _dsn_received.erase(_dsn_received.begin());
            _cumulative_data_ack+= size;
            if (_buffer_logger) _buffer_logger->logBuffer(ReorderBufferLogger::BUF_DEQUEUE);
        }
    } else if (dsn < _cumulative_data_ack+1) {
        // cout << "Dup DSN received!\n";
    } else {
        // hole in sequence space
        _dsn_received.insert(dsn);
        if (_buffer_logger) {
            // cout << "BUF_ENQUEUE\n";
            _buffer_logger->logBuffer(ReorderBufferLogger::BUF_ENQUEUE);
        }
    }
}
 
uint64_t
SwiftSink::cumulative_ack() {
    // this is needed by some loggers.  If we ever need it, figure out what it should really return
    return _src ? _cumulative_data_ack + _src->_mss : 0;
} 

uint32_t
SwiftSink::drops(){
    return _src ? _src->drops() : 0;
}

////////////////////////////////////////////////////////////////
//  SWIFT RETRANSMISSION TIMER
////////////////////////////////////////////////////////////////

SwiftRtxTimerScanner::SwiftRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist)
    : EventSource(eventlist,"RtxScanner"), _scanPeriod(scanPeriod) {
    eventlist.sourceIsPendingRel(*this, _scanPeriod);
}

void 
SwiftRtxTimerScanner::registerSubflow(SwiftSubflowSrc &subflow_src) {
    _subflows.push_back(&subflow_src);
}

void SwiftRtxTimerScanner::doNextEvent() {
    simtime_picosec now = eventlist().now();
    subflows_t::iterator i;
    for (i = _subflows.begin(); i!=_subflows.end(); i++) {
        (*i)->rtx_timer_hook(now,_scanPeriod);
    }
    eventlist().sourceIsPendingRel(*this, _scanPeriod);
}
