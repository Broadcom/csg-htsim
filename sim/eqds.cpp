// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include <math.h>
#include "eqds.h"
#include "eqds_logger.h"
#include "circular_buffer.h"

using namespace std;

// Static stuff

// _path_entropy_size is the number of paths we spray across.  If you don't set it, it will default to all paths.
uint32_t EqdsSrc::_path_entropy_size = 256;
int EqdsSrc::_global_node_count = 0;

/* _min_rto can be tuned using setMinRTO. Don't change it here.  */
simtime_picosec EqdsSrc::_min_rto = timeFromUs((uint32_t)DEFAULT_EQDS_RTO_MIN);

mem_b EqdsSink::_bytes_unacked_threshold = 16384;
int EqdsSink::TGT_EV_SIZE = 7;

/* if you change _credit_per_pull, fix pktTime in the Pacer too - this assumes one pull per MTU */
EqdsBasePacket::pull_quanta EqdsSink::_credit_per_pull = 8; // uints of typically 512 bytes

/* this default will be overridden from packet size*/
uint16_t EqdsSrc::_hdr_size = 64;
uint16_t EqdsSrc::_mss = 4096;
uint16_t EqdsSrc::_mtu = _mss + _hdr_size;

bool EqdsSrc::_debug = false; 

// uncomment below - commented out for testing
//#define USE_CWND  

////////////////////////////////////////////////////////////////                                                                   
//  EQDS NIC
////////////////////////////////////////////////////////////////

EqdsNIC::EqdsNIC(EventList &eventList, linkspeed_bps linkspeed) :
    EventSource(eventList, "eqdsNIC")
{
    _linkspeed = linkspeed;
    _send_end_time = 0;
    _last_pktsize = 0;
    _num_queued_srcs = 0;
    _ratio_data = 1;
    _ratio_control = 20;
    _crt = 0;
}

// srcs call request_sending to see if they can send now.  If the
// answer is no, they'll be called back when it's time to send.
bool EqdsNIC::requestSending(EqdsSrc& src) {
    if (EqdsSrc::_debug) {
        cout << src.nodename() << " requestSending at " << timeAsUs(EventList::getTheEventList().now()) << endl;
    }
    if (_send_end_time >= eventlist().now()) {
        // we're already sending
        if (_num_queued_srcs == 0 && _control.empty()) {
            // need to schedule the callback
            eventlist().sourceIsPending(*this,_send_end_time);
        }
        _num_queued_srcs += 1;
        _active_srcs.push_back(&src);
        return false;
    }
    assert (_num_queued_srcs == 0 && _control.empty());
    return true;
}

// srcs call startSending when they are allowed to actually send
void EqdsNIC::startSending(EqdsSrc& src, mem_b pkt_size) {
    if (EqdsSrc::_debug) {
        cout << src.nodename() << " startSending at " << timeAsUs(EventList::getTheEventList().now()) << endl;
    }
    if (_num_queued_srcs > 0) {
        EqdsSrc *queued_src = _active_srcs.front();
        _active_srcs.pop_front();
        _num_queued_srcs--;
        assert(_num_queued_srcs >= 0);
        assert(queued_src == &src);
    }
    assert(eventlist().now() >= _send_end_time);

    _send_end_time = eventlist().now() + (pkt_size * 8 * timeFromSec(1.0))/_linkspeed;
    if (_num_queued_srcs > 0 || !_control.empty()) {
        eventlist().sourceIsPending(*this,_send_end_time);
    }
}

// srcs call cantSend when they previously requested to send, and now its their turn, they can't for some reason.
void EqdsNIC::cantSend(EqdsSrc& src) {
    if (EqdsSrc::_debug) {
        cout << src.nodename() << " cantSend at " << timeAsUs(EventList::getTheEventList().now()) << endl;
    }

    if (_num_queued_srcs == 0 && _control.empty()) {
        // it was an immediate send, so nothing to do if we can't send after all
        return;
    }
    if (_num_queued_srcs>0){
        _num_queued_srcs--;

        EqdsSrc *queued_src = _active_srcs.front();
        _active_srcs.pop_front();

        assert(queued_src == &src);
        assert(eventlist().now() >= _send_end_time);

        if (_num_queued_srcs > 0) {
            // give the next src a chance.
            queued_src = _active_srcs.front();
            queued_src->timeToSend();
            return;
        }
    }
    if (!_control.empty()){
        //need to send a control packet, since we didn't manage to send a data packet.
        Packet* p = _control.front();
        _control.pop_front();
        p->sendOn();

        simtime_picosec delta = ((simtime_picosec)p->size() * 8 * timeFromSec(1.0))/_linkspeed;

        if (EqdsSrc::_debug) cout << "NIC "<< this <<" send control of size " << p->size() << " duration " <<  timeAsUs(delta) << endl;
        _control_size -= p->size();

        _send_end_time = eventlist().now() + delta;

        if (_num_queued_srcs > 0 || !_control.empty()) {
            eventlist().sourceIsPending(*this,_send_end_time);
        }
    }
}

bool EqdsNIC::sendControlPacket(EqdsBasePacket* pkt){
    //pkt->sendOn();
    _control_size += pkt->size();
    _control.push_back(pkt);

    if (EqdsSrc::_debug) {
        cout << "NIC " << this << " request to send control packet of type " << pkt->str() << " control queue size " <<_control_size << " " << _control.size() << endl;
    }

    if (_send_end_time >= eventlist().now()) {
        if (EqdsSrc::_debug) {
            cout << "NIC sendControlPacket " << this << " already sending " << timeAsUs(_send_end_time) << " >= " << eventlist().now() << endl;
        }
        // we're in the process of sending but nobody is active, need to schedule next event
        if (_num_queued_srcs == 0 && _control.size()==1 ) {
            if (EqdsSrc::_debug) {
                cout << "NIC sendControlPacket" << this << " schedule event " << timeAsUs(_send_end_time) << " now is " << eventlist().now() << endl;
            }
            // need to schedule the callback, because noone else has done so.
            eventlist().sourceIsPending(*this,_send_end_time);
        }
    }
    else  {
        //send now!
        _send_end_time = eventlist().now();
        doNextEvent();
    }

    //could return false to mimick too many control packets.

    //return true to indicate that the packet was accepted for transmission
    return true;
}

void EqdsNIC::doNextEvent() {
    assert(eventlist().now() == _send_end_time);
    assert(_num_queued_srcs > 0 || !_control.empty());

    if (EqdsSrc::_debug)
        cout << "NIC " << this << " doNextEvent at " << timeAsUs(eventlist().now()) << endl;

    if (_num_queued_srcs>0 && !_control.empty()){
        _crt++;

        if (_crt >= (_ratio_control+_ratio_data))
            _crt = 0;

        if (EqdsSrc::_debug) {
            cout << "NIC " << this << " round robin time between srcs " << _num_queued_srcs << " and control " << _control.size() << " " << _crt;
        }

        if (_crt< _ratio_data){
            // it's time for the next source to send
            EqdsSrc *queued_src = _active_srcs.front();
            queued_src->timeToSend();

            if (EqdsSrc::_debug) cout << " send data " << endl;

            return;
        } 
        else {
            Packet* p = _control.front();
            _control.pop_front();
            p->sendOn();

            simtime_picosec delta = ((simtime_picosec)p->size() * 8 * timeFromSec(1.0))/_linkspeed;

            if (EqdsSrc::_debug) cout << "NIC "<< this <<" send control of size " << p->size() << " duration " <<  timeAsUs(delta) << endl;
            _control_size -= p->size();

            _send_end_time = eventlist().now() + delta;

            if (_num_queued_srcs > 0 || !_control.empty()) {
                eventlist().sourceIsPending(*this,_send_end_time);
            }
            return;
        } 
    }

    //either we have active sources or control packets, not both.

    if(_num_queued_srcs>0){
            EqdsSrc *queued_src = _active_srcs.front();
            queued_src->timeToSend();

            if (EqdsSrc::_debug) cout << "NIC " << this << " send data ONLY " << endl;
    }
    else {
        assert(!_control.empty());
        Packet* p = _control.front();
        _control.pop_front();

        if (EqdsSrc::_debug) cout << "NIC "<< this << " send control ONLY of size " << p->size() << " at " << timeAsUs(eventlist().now()) << endl;
        
        _control_size -= p->size();

        p->sendOn();
        _send_end_time = eventlist().now() + (p->size() * 8 * timeFromSec(1.0))/_linkspeed;

        if (_num_queued_srcs > 0 || !_control.empty()) {
            eventlist().sourceIsPending(*this,_send_end_time);
        }
        else if (EqdsSrc::_debug) cout << "NIC " << this << " do not reschedule " <<  timeAsUs(eventlist().now()) << endl;
    }
}

////////////////////////////////////////////////////////////////                                                                   
//  EQDS SRC
////////////////////////////////////////////////////////////////   

EqdsSrc::EqdsSrc(TrafficLogger *trafficLogger, EventList &eventList, EqdsNIC &nic, bool rts) :
    EventSource(eventList, "eqdsSrc"), _nic(nic), _flow(trafficLogger)
{
    _node_num = _global_node_count++;
    _nodename = "eqdsSrc " + to_string(_node_num);
    _rtx_timeout_pending = false;
    _rtx_timeout = timeInf;
    _rto_timer_handle = eventlist().nullHandle();
    _rtt = _min_rto;
    _mdev = 0;
    _rto = _min_rto;
    _logger = NULL;

    _maxwnd = 50 * _mtu;
    _cwnd = _maxwnd;
    _flow_size = 0;
    _done_sending = false;
    _backlog = 0;
    _unsent = 0;
    _cwnd = _maxwnd;
    _pull_target = 0;
    _pull = 0;
    _state = INITIALIZE_CREDIT;
    _speculating = false;
    _credit_pull = 0;
    _credit_spec = _maxwnd;
    _in_flight = 0;
    _highest_sent = 0;
    _send_blocked_on_nic = false;
    _no_of_paths = _path_entropy_size;
    _path_random = rand() % 0xffff; // random upper bits of EV
    _path_xor = rand() % _no_of_paths;
    _current_ev_index = 0;
    _max_penalty = 1;
    _last_rts = 0;

    // stats for debugging
    _new_packets_sent = 0;
    _rtx_packets_sent = 0;
    _rts_packets_sent = 0;
    _bounces_received = 0;

    // reset path penalties
    _ev_skip_bitmap.resize(_no_of_paths);
    for (uint32_t i = 0; i < _no_of_paths; i++) {
        _ev_skip_bitmap[i] = 0;
    }
    
    // by default, end silently
    _end_trigger = 0;

    _dstaddr = UINT32_MAX;
    _route = NULL;
    _mtu = Packet::data_packet_size();
    _mss = _mtu - _hdr_size;

    _debug_src = EqdsSrc::_debug;
    //if (_node_num == 490) _debug_src = true; // use this to enable debugging on one flow at a time
}

void EqdsSrc::connect(Route &routeout, Route &routeback, EqdsSink &sink, simtime_picosec start_time) {
    _route = &routeout;
    _sink = &sink;
    //_flow.set_id(get_id());  // identify the packet flow with the EQDS source that generated it
    _flow._name = _name;
   
    _sink->connect(this, &routeback);

    if (start_time != TRIGGER_START) {
        eventlist().sourceIsPending(*this,start_time);
    }
}

simtime_picosec EqdsSrc::computeDynamicRTO(simtime_picosec send_time) {
    //this code is never called, as per spec (fixed RTO setting).
    //keeping it here just in case we want to experiment at some point with dynamic RTO calculation. 

    simtime_picosec raw_rtt = eventlist().now() - send_time;

    assert(raw_rtt > 0);
    if (_rtt>0){
        simtime_picosec abs;
        if (raw_rtt > _rtt)
            abs = raw_rtt - _rtt;
        else
            abs = _rtt - raw_rtt;

        _mdev = 3 * _mdev / 4 + abs/4;
        _rtt = 7*_rtt/8 + raw_rtt/8;

        _rto = _rtt + 4*_mdev;
    } else {
        _rtt = raw_rtt;
        _mdev = raw_rtt/2;

        _rto = _rtt + 4*_mdev;
    }
    if (_rto < _min_rto)
        _rto = _min_rto * ((drand() * 0.5) + 0.75);


    if (_rto < _min_rto)
        _rto = _min_rto;

    if (_debug_src) {
        cout << "RTO for flow " << _flow.str() << " computed at " << timeAsUs(_rto) << " will be lower bounded to " << timeAsUs(_min_rto) << endl;
    }

    return _rto;
}

void EqdsSrc::receivePacket(Packet &pkt) {
    switch (pkt.type()) {
    case EQDSDATA:
        {
            _bounces_received++;
            // TBD - this is likely a Back-to-sender packet
            abort();
        }
    case EQDSRTS:
        {
            abort();
        }
    case EQDSACK:
        {
            processAck((const EqdsAckPacket&)pkt);
            pkt.free();
            return;
        }
    case EQDSNACK:
        {
            processNack((const EqdsNackPacket&)pkt);
            pkt.free();
            return;
        }
    case EQDSPULL:
        {
            processPull((const EqdsPullPacket&)pkt);
            pkt.free();
            return;
        }
    default:
        {
            abort();
        }
    }
}

void EqdsSrc::handleAckno(EqdsDataPacket::seq_t ackno) {
    auto i = _tx_bitmap.find(ackno);
    if (i == _tx_bitmap.end())
        return;
    //mem_b pkt_size = i->second.pkt_size;
    simtime_picosec send_time = i->second.send_time;

    //computeRTO(send_time);

    mem_b pkt_size = i->second.pkt_size;
    _in_flight -= pkt_size;
    assert(_in_flight >= 0);
    if (_debug_src) cout << _nodename << " handleAck " << ackno << " flow " << _flow.str() << endl;
    _tx_bitmap.erase(i);
    _send_times.erase(send_time);

    if (send_time == _rto_send_time) {
        recalculateRTO();
    }
}

void EqdsSrc::handleCumulativeAck(EqdsDataPacket::seq_t cum_ack) {
    // free up anything cumulatively acked
    while (!_rtx_queue.empty()) {
        auto seqno = _rtx_queue.begin()->first;

        if (seqno < cum_ack) {
            _rtx_queue.erase(_rtx_queue.begin());
        }
        else break;
    }

    auto i = _tx_bitmap.begin();
    while (i != _tx_bitmap.end()) {
        auto seqno = i->first;
        // cumulative ack is next expected packet, not yet received
        if (seqno >= cum_ack) {
            // nothing else acked
            break;
        }
        mem_b pkt_size = i->second.pkt_size;
        simtime_picosec send_time = i->second.send_time;

        //computeRTO(send_time);

        _in_flight -= pkt_size;
        assert(_in_flight >= 0);
        if (_debug_src) cout << _nodename << " handleCumAck " << seqno << " flow " << _flow.str() << endl;
        _tx_bitmap.erase(i);
        i = _tx_bitmap.begin();
        _send_times.erase(send_time);
        if (send_time == _rto_send_time) {
            recalculateRTO();
        }
    }
}

void EqdsSrc::handlePull(EqdsBasePacket::pull_quanta pullno) {
    if (pullno > _pull) {
        EqdsBasePacket::pull_quanta extra_credit = pullno - _pull;
        _credit_pull += EqdsBasePacket::unquantize(extra_credit);
        _pull = pullno;
    }
}

bool EqdsSrc::checkFinished(EqdsDataPacket::seq_t cum_ack) {
    // cum_ack gives the next expected packet
    if (_done_sending) {
        // if (EqdsSrc::_debug) cout << _nodename << " checkFinished done sending " << " cum_acc " << cum_ack << " mss " << _mss << " c*m " << cum_ack * _mss << endl;
        return true;
    }
    if (_debug_src) 
        cout << _nodename << " checkFinished " << " cum_acc " << cum_ack << " mss " << _mss << " RTS sent " << _rts_packets_sent << " total bytes " << (cum_ack - _rts_packets_sent) * _mss << " flow_size " << _flow_size << " done_sending " << _done_sending << endl;

    if ((((mem_b)cum_ack -_rts_packets_sent) * _mss) >= _flow_size) {
        cout << "Flow " << _name << " flowId " << flowId() << " " << _nodename << " finished at " << timeAsUs(eventlist().now()) << " total packets " << cum_ack << " RTS " << _rts_packets_sent << " total bytes " << ((mem_b)cum_ack - _rts_packets_sent) * _mss << endl;
        _state = IDLE;
        if (_end_trigger) {
            _end_trigger->activate();
        }
        if (_flow_logger) {
            _flow_logger->logEvent(_flow, *this, FlowEventLogger::FINISH, _flow_size, cum_ack);
        }
        _done_sending = true;
        return true;
    }
    return false;
}

void EqdsSrc::processAck(const EqdsAckPacket& pkt) {
    auto cum_ack = pkt.cumulative_ack();
    handleCumulativeAck(cum_ack);

    if (_debug_src) 
        cout << _nodename << " processAck cum_ack: " << cum_ack << " flow " << _flow.str() << endl;
 
    auto ackno = pkt.ref_ack();
    uint64_t bitmap = pkt.bitmap();
    if (_debug_src)
        cout << "    ref_ack: " << ackno << " bitmap: " << bitmap << endl;
    while (bitmap > 0) { 
        if (bitmap & 1) {
            if (_debug_src) 
                cout << "    Sack " << ackno << " flow " << _flow.str() << endl;

            handleAckno(ackno);
        }
        ackno++;
        bitmap >>= 1;
    }

    //auto pullno = pkt.pullno();
    //handlePull(pullno);

    // handle ECN echo
    if (pkt.ecn_echo()) {
        penalizePath(pkt.ev(), 1);
    }

    if (checkFinished(cum_ack)) {
        stopSpeculating();
        return;
    }

    stopSpeculating();
    sendIfPermitted();
}

void EqdsSrc::processNack(const EqdsNackPacket& pkt) {
    //auto pullno = pkt.pullno();
    //handlePull(pullno);

    auto nacked_seqno = pkt.ref_ack();
    if (_debug_src) 
        cout << _nodename << " processNack nacked: " << nacked_seqno << " flow " <<_flow.str() << endl;

    uint16_t ev = pkt.ev();
    // what should we do when we get a NACK with ECN_ECHO set?  Presumably ECE is superfluous?
    //bool ecn_echo = pkt.ecn_echo();

    // move the packet to the RTX queue
    auto i = _tx_bitmap.find(nacked_seqno);
    if (i == _tx_bitmap.end()) {
        if (_debug_src) 
            cout << "Didn't find NACKed packet in _active_packets flow " << _flow.str() << endl;

        // this abort is here because this is unlikely to happen in
        // simulation - when it does, it is usually due to a bug
        // elsewhere.  But if you discover a case where this happens
        // for real, remove the abort and uncomment the return below.
        abort();
        // this can happen when the NACK arrives later than a cumulative ACK covering the NACKed packet.
        //return;
    }
    mem_b pkt_size = i->second.pkt_size;
    
    assert(pkt_size >= _hdr_size); // check we're not seeing NACKed RTS packets.
    if (pkt_size == _hdr_size){
        _stats.rts_nacks ++;
    } 
    
    auto seqno = i->first;
    simtime_picosec send_time = i->second.send_time;

    //computeDynamicRTO(send_time);

    if (_debug_src) cout << _nodename << " erasing send record, seqno: " << seqno << " flow " << _flow.str() << endl;
    _tx_bitmap.erase(i);
    assert(_tx_bitmap.find(seqno) == _tx_bitmap.end()); // xxx remove when working

    _in_flight -= pkt_size;
    assert(_in_flight >= 0);
    
    _send_times.erase(send_time);
    queueForRtx(seqno, pkt_size);

    if (send_time == _rto_send_time) {
        recalculateRTO();
    }

    penalizePath(ev, 1);
    stopSpeculating();
    sendIfPermitted();
}

void EqdsSrc::processPull(const EqdsPullPacket& pkt) {
    auto pullno = pkt.pullno();
    if (_debug_src) cout << _nodename << " processPull " << pullno << " flow " << _flow.str() << endl;

    handlePull(pullno);

    stopSpeculating();
    sendIfPermitted();
}

void EqdsSrc::doNextEvent() {
    // a timer event fired.  Can either be a timeout, or the timed start of the flow.
    if (_rtx_timeout_pending) {
        clearRTO();
        assert(_logger == 0);

        if (_logger) _logger->logEqds(*this, EqdsLogger::EQDS_TIMEOUT);

	    rtxTimerExpired();
    } else {
        if (_debug_src) cout << "Starting flow " << _name << endl;                                                                                           
        startFlow();
    }
}

void EqdsSrc::setFlowsize(uint64_t flow_size_in_bytes) {
    _flow_size = flow_size_in_bytes;
}

void EqdsSrc::startFlow() {
    _cwnd = _maxwnd;
    _credit_spec = _maxwnd;
    if (_debug_src) cout << "startflow " <<  _flow._name <<  " CWND " << _cwnd << " at " << timeAsUs(eventlist().now()) << " flow " << _flow.str() << endl;
    if (_flow_logger) {
        _flow_logger->logEvent(_flow, *this, FlowEventLogger::START, _flow_size, 0);
    }
    clearRTO();
    _in_flight = 0;
    _pull_target = 0;
    _pull = 0;
    _unsent = _flow_size;
    _last_rts = 0;
    // backlog is total amount of data we expect to send, including headers
    _backlog = ceil(((double)_flow_size)/_mss) * _hdr_size + _flow_size;
    _state = SPECULATING; 
    _speculating = true;
    _send_blocked_on_nic = false;
    while (_send_blocked_on_nic == false && credit() > 0 && _unsent > 0) {
        if (_debug_src) cout << "requestSending 0 "<< " flow " << _flow.str() << endl;

        bool can_i_send = _nic.requestSending(*this);
        if (can_i_send) {
            // if we're here, there's no NIC queue
            mem_b sent_bytes = sendNewPacket();
            if (sent_bytes > 0) {
                _nic.startSending(*this, sent_bytes);
            } else {
                _nic.cantSend(*this);
            }
        } else {
            _send_blocked_on_nic = true;
            return;
        }
    }
}

mem_b EqdsSrc::credit() const {
    return _credit_pull + _credit_spec;
}

void EqdsSrc::stopSpeculating() {
    // we just got an ack, nack or pull.  We need to stop speculating

    _speculating = false;
    if (_backlog > 0 && _state == SPECULATING) {
        _state = COMMITTED;
    } 
}

bool EqdsSrc::spendCredit(mem_b pktsize, bool& speculative) {
    assert(credit() > 0);
    if (_credit_pull > 0) {
        assert(_state == COMMITTED);
        _credit_pull -= pktsize;
        speculative = false;
        return true;
    } else if (_speculating && _credit_spec > 0) {
        assert(_state == SPECULATING);
        _credit_spec -= pktsize;
        speculative = true;
        return true;
    } else {
        assert(_state == COMMITTED);
        // we're not going to be sending right now, but we need to
        // reduce speculative credit so that the pull target can
        // advance
        _credit_spec -= pktsize;
        return false;
    }
}

EqdsBasePacket::pull_quanta EqdsSrc::computePullTarget() {
    mem_b pull_target = _backlog;
    if (pull_target > _cwnd + _mtu) {
        pull_target = _cwnd + _mtu;
    }
    if (pull_target > _maxwnd) {
        pull_target = _maxwnd;
    }
    pull_target -= (_credit_pull + _credit_spec);
    EqdsBasePacket::pull_quanta quant_pull_target = EqdsBasePacket::quantize_ceil(pull_target) + _pull;
    if (_debug_src)
        cout << nodename() << " pull_target: " << EqdsBasePacket::unquantize(quant_pull_target) << " pull " << EqdsBasePacket::unquantize(_pull) << " diff " <<  EqdsBasePacket::unquantize(quant_pull_target - _pull) << endl;
    return quant_pull_target;
}

void EqdsSrc::sendIfPermitted() {
    // send if the NIC, credit and window allow.

    if (credit() <= 0) {
        // can send if we have *any* credit, but we don't
        return;
    }

    // how large will the packet be?
    mem_b pkt_size = 0;
    if (_rtx_queue.empty()) {
        if (_backlog == 0) {
            // nothing to retransmit, and no backlog.  Nothing to do here.
            if (_credit_pull > 0) {
                if (_debug_src) cout << "we have " << _credit_pull << " bytes of credit, but nothing to use it on"<< " flow " << _flow.str() << endl;
            }
            return;
        }
        mem_b payload_size = _mss;
        if (_unsent == 0)
            return;

        if (_unsent < payload_size) {
            payload_size = _unsent;
        }
        pkt_size = payload_size + _hdr_size;
    } else {
        pkt_size = _rtx_queue.begin()->second;
    }

#ifdef USE_CWND
    if (pkt_size > _cwnd) {
        return;
    }
#endif

    if (_send_blocked_on_nic) {
        // the NIC already knows we want to send
        return;
    }

    // we can send if the NIC lets us.
    if (_debug_src) cout << "requestSending 1\n";
    bool can_i_send = _nic.requestSending(*this);
    if (can_i_send) {
        mem_b sent_bytes = sendPacket();
        if (sent_bytes > 0) {
            _nic.startSending(*this, sent_bytes);
        } else {
            _nic.cantSend(*this);
        }
    } else {
        // we can't send yet, but NIC will call us back when we can
        _send_blocked_on_nic = true;
        return;
    }    
}

// if sendPacket got called, we have already asked the NIC for
// permission, and we've already got both credit and cwnd to send, so
// we will likely be sending something (sendNewPacket can return 0 if
// we only had speculative credit we're not allowed to use though)
mem_b EqdsSrc::sendPacket() {
    if (_rtx_queue.empty()) {
        return sendNewPacket();
    } else {
        return sendRtxPacket();
    }
}

void EqdsSrc::startRTO(simtime_picosec send_time) {
    if (!_rtx_timeout_pending) {
        // timer is not running - start it
        _rtx_timeout_pending = true;
        _rtx_timeout = send_time + _rto;
        _rto_send_time = send_time;

        if (_rtx_timeout < eventlist().now())
            _rtx_timeout = eventlist().now();
       
        if (_debug) cout << "Start timer at " << timeAsUs(eventlist().now()) << " source " << _flow.str() << " expires at " << timeAsUs(_rtx_timeout) << " flow " << _flow.str() << endl;

        _rto_timer_handle = eventlist().sourceIsPendingGetHandle(*this, _rtx_timeout);
        if (_rto_timer_handle == eventlist().nullHandle()) {
            // this happens when _rtx_timeout is past the configured simulation end time.
            _rtx_timeout_pending = false;
            if (_debug) cout << "Cancel timer because too late for flow " << _flow.str() << endl;
        }
    } else {
        // timer is already running
        if (send_time + _rto < _rtx_timeout) {
            // RTO needs to expire earlier than it is currently set
            cancelRTO();
            startRTO(send_time);
        }
    }
}

void EqdsSrc::clearRTO() {
    // clear the state
    _rto_timer_handle = eventlist().nullHandle();
    _rtx_timeout_pending = false;

    if (_debug) cout << "Clear RTO " << timeAsUs(eventlist().now()) << " source " << _flow.str() << endl;
}

void EqdsSrc::cancelRTO() {
    if (_rtx_timeout_pending) {
        // cancel the timer
        eventlist().cancelPendingSourceByHandle(*this, _rto_timer_handle);
        clearRTO();
    }
}

void EqdsSrc::penalizePath(uint16_t path_id, uint8_t penalty) {
    // _no_of_paths must be a power of 2
    uint16_t mask = _no_of_paths - 1;
    path_id &= mask;  // only take the relevant bits for an index
    _ev_skip_bitmap[path_id] += penalty;
    if (_ev_skip_bitmap[path_id] > _max_penalty) {
        _ev_skip_bitmap[path_id] = _max_penalty;
    }
}

uint16_t EqdsSrc::nextEntropy() {
    // _no_of_paths must be a power of 2
    uint16_t mask = _no_of_paths - 1;  
    uint16_t entropy = (_current_ev_index ^ _path_xor) & mask;
    while (_ev_skip_bitmap[entropy] > 0) {
        _ev_skip_bitmap[entropy]--;
        _current_ev_index++;
        if (_current_ev_index == _no_of_paths) {
            _current_ev_index = 0;
            _path_xor = rand() & mask;
        }
        entropy = (_current_ev_index ^ _path_xor) & mask;
    }

    // set things for next time
    _current_ev_index++;
    if (_current_ev_index == _no_of_paths) {
        _current_ev_index = 0;
        _path_xor = rand() & mask; 
    }
    
    entropy |= _path_random ^ (_path_random & mask); // set upper bits
    return entropy;
}

mem_b EqdsSrc::sendNewPacket() {
    if (_debug_src) cout << _nodename << " sendNewPacket highest_sent " << _highest_sent << " h*m " << _highest_sent * _mss << " backlog " << _backlog << " unsent " << _unsent << " flow " << _flow.str() << endl;
    assert(_unsent > 0);
    assert(((mem_b)_highest_sent - _rts_packets_sent) * _mss < _flow_size);
    mem_b payload_size = _mss;
    if (_unsent < payload_size) {
        payload_size = _unsent;
    }
    assert(payload_size > 0);
    mem_b full_pkt_size = payload_size + _hdr_size;

    // check we're allowed to send according to state machine
    assert(credit() > 0);
    bool speculative = false;
    bool can_send = spendCredit(full_pkt_size, speculative);
    if (!can_send) {
        // we can't send because we're not in speculative mode and only had speculative credit
        return 0;
    }

    _backlog -= full_pkt_size;
    assert(_backlog >= 0);
    _unsent -= payload_size;
    assert(_backlog >= _unsent);
    _in_flight += full_pkt_size;
    auto ptype = EqdsDataPacket::DATA_PULL;
    if (speculative) {
        ptype = EqdsDataPacket::DATA_SPEC;
    }
    _pull_target = computePullTarget();

    auto *p = EqdsDataPacket::newpkt(_flow, *_route, _highest_sent, full_pkt_size, ptype, _pull_target, /*unordered=*/true, _dstaddr);
    uint16_t ev = nextEntropy();
    p->set_pathid(ev);
    p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);

    if (_backlog == 0 || _credit_pull < 0)
        p->set_ar(true);

    createSendRecord(_highest_sent, full_pkt_size);
    if (_debug_src) cout << _flow.str() << " sending pkt " << _highest_sent << " size " << full_pkt_size << " pull target " << _pull_target << " ack request " << p->ar() << " at " << timeAsUs(eventlist().now())<<endl;
    p->sendOn();
    _highest_sent++;
    _new_packets_sent++;
    startRTO(eventlist().now());
    return full_pkt_size;
}

mem_b EqdsSrc::sendRtxPacket() {
    assert(!_rtx_queue.empty());
    auto seq_no = _rtx_queue.begin()->first;
    mem_b full_pkt_size = _rtx_queue.begin()->second;
    bool speculative = false;
    bool can_send = spendCredit(full_pkt_size, speculative);
    assert(!speculative); // I don't think this can happen, but remove this assert if we decide it can
    if (!can_send) {
        // we can't sent because we've only got speculative credit and we're not in speculating mode
        return 0;
    }
    
    _rtx_queue.erase(_rtx_queue.begin());
    _in_flight += full_pkt_size;
    auto *p = EqdsDataPacket::newpkt(_flow, *_route, seq_no, full_pkt_size,
                                     EqdsDataPacket::DATA_RTX, _pull_target, /*unordered=*/true, _dstaddr);
    uint16_t ev = nextEntropy();
    p->set_pathid(ev);
    p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);

    createSendRecord(seq_no, full_pkt_size);

    if (_debug_src) cout << _nodename << " sending rtx pkt " << seq_no << " size " << full_pkt_size << " flow " << _flow.str() << " at " << timeAsUs(eventlist().now())<< endl;
    p->set_ar(true);
    p->sendOn();
    _rtx_packets_sent++;
    startRTO(eventlist().now());
    return full_pkt_size;
}

void EqdsSrc::sendRTS() {
    if (_last_rts > 0 && eventlist().now() - _last_rts < _rtt) {
        // Don't send more than one RTS per RTT, or we can create an
        // incast of RTS.  Once per RTT is enough to restart things if we lost
        // a whole window.
        return;
    }
    if (_debug_src) cout << _nodename << " sendRTS, route: " << _route << " flow " << _flow.str() << " at " << timeAsUs(eventlist().now()) << " last RTS " << timeAsUs(_last_rts) << endl;
    createSendRecord(_highest_sent, _hdr_size);
    auto *p = EqdsRtsPacket::newpkt(_flow, *_route, _highest_sent, _hdr_size,
                                    _pull_target, _dstaddr);
    p->set_dst(_dstaddr);
    uint16_t ev = nextEntropy();
    p->set_pathid(ev);

    //p->sendOn();
    _nic.sendControlPacket(p);

    _highest_sent++;
    _rts_packets_sent++;
    _last_rts = eventlist().now();
    startRTO(eventlist().now());
}

void EqdsSrc::createSendRecord(EqdsBasePacket::seq_t seqno, mem_b full_pkt_size) {
    //assert(full_pkt_size > 64);
    if (_debug_src) cout << _nodename << " createSendRecord seqno: " << seqno << " size " << full_pkt_size << endl;
    assert(_tx_bitmap.find(seqno) == _tx_bitmap.end());
    _tx_bitmap.emplace(seqno, sendRecord(full_pkt_size, eventlist().now()));
    _send_times.emplace(eventlist().now(), seqno);
}

void EqdsSrc::queueForRtx(EqdsBasePacket::seq_t seqno, mem_b pkt_size) {
    assert(_rtx_queue.find(seqno) == _rtx_queue.end());
    _rtx_queue.emplace(seqno, pkt_size);
    sendIfPermitted();
}

void EqdsSrc::timeToSend() {
    if (_debug_src) cout << "timeToSend" << " flow " << _flow.str() << " at " << timeAsUs(eventlist().now()) << endl;

    if (_unsent == 0 && _rtx_queue.empty()){
        _nic.cantSend(*this);
        return; 
    }
    // time_to_send is called back from the EqdsNIC when it's time for
    // this src to send.  To get called back, the src must have
    // previously told the NIC it is ready to send by calling
    // EqdsNIC::requestSending()
    //
    // before returning, EqdsSrc needs to call either
    // EqdsNIC::startSending or EqdsNIC::cantSend from this function
    // to update the NIC as to what happened, so they stay in sync.
    _send_blocked_on_nic = false;

    mem_b full_pkt_size;
    // how much do we want to send?
    if (_rtx_queue.empty()) {
        // we want to send new data
        mem_b payload_size = _mss;
        if (_unsent < payload_size) {
            payload_size = _unsent;
        }
        assert(payload_size > 0);
        full_pkt_size = payload_size + _hdr_size;
    } else {
        // we want to retransmit
        full_pkt_size = _rtx_queue.begin()->second;
    }
#ifdef USE_CWND
    if (_cwnd < full_pkt_size) {
        if (_debug_src) cout << "cantSend\n";
        _nic.cantSend(*this);
        return;
    }
#endif

    // do we have enough credit?
    if (credit() <= 0) {
        if (_debug_src) cout << "cantSend"<< " flow " << _flow.str() << endl;;
        _nic.cantSend(*this);
        return;
    }

    // OK, we're probably good to send
    mem_b bytes_sent = 0;
    if (_rtx_queue.empty()) {
        bytes_sent = sendNewPacket();
    } else {
        bytes_sent = sendRtxPacket();
    }

    // let the NIC know we sent, so it can calculate next send time.
    if (bytes_sent > 0) {
        _nic.startSending(*this, full_pkt_size);
    } else {
        _nic.cantSend(*this);
        return;
    }

#ifdef USE_CWND
    if (_cwnd < full_pkt_size) {
        return;
    }
#endif
    // do we have enough credit to send again?
    if (credit() <= 0) {
        return;
    }

    if (_unsent == 0 && _rtx_queue.empty()) {
        // we're done - nothing more to send.
        assert(_backlog == 0);
        return;
    }

    // we're ready to send again.  Let the NIC know.
    assert(!_send_blocked_on_nic);
    if (_debug_src) cout << "requestSending2"<< " flow " << _flow.str() << endl;;
    bool can_i_send = _nic.requestSending(*this);
    // we've just sent - NIC will say no, but will call us back when we can send.
    assert(!can_i_send);
    _send_blocked_on_nic = true;
}

void EqdsSrc::recalculateRTO() {
    // we're no longer waiting for the packet we set the timer for -
    // figure out what the timer should be now.
    cancelRTO();
    if (_send_times.empty()) {
        // nothing left that we're waiting for
        return;
    }
    auto earliest_send_time = _send_times.begin()->first;
    startRTO(earliest_send_time);
}

void EqdsSrc::rtxTimerExpired() {
    assert(eventlist().now() == _rtx_timeout);
    clearRTO();

    auto first_entry = _send_times.begin();
    assert(first_entry != _send_times.end());
    auto seqno = first_entry->second;

    auto send_record = _tx_bitmap.find(seqno);
    assert(send_record != _tx_bitmap.end());
    mem_b pkt_size = send_record->second.pkt_size;

    //update flightsize?

    _send_times.erase(first_entry);
    if (_debug_src) cout << _nodename << " rtx timer expired for " << seqno << " flow " << _flow.str() << endl;
    _tx_bitmap.erase(send_record);
    recalculateRTO();

    if (!_rtx_queue.empty()) {
        // there's already a queue, so clearly we shouldn't just
        // resend right now.  But send an RTS (no more than once per
        // RTT) to cover the case where the receiver doesn't know
        // we're waiting.
        queueForRtx(seqno, pkt_size);
        sendRTS();
        
        if (_debug_src) 
            cout << "sendRTS 1"<< " flow " << _flow.str() << endl;;

        return;
    }

    // there's no queue, so maybe we could just resend now?
    queueForRtx(seqno, pkt_size);
    
#ifdef USE_CWND
    if (_cwnd < pkt_size) {
        // window won't allow us to send yet.
        sendRTS();
        return;
    }
#endif

    if (credit() <= 0) {
        // we don't have any credit to send.  Send an RTS (no more
        // than once per RTT) to cover the case where the receiver
        // doesn't know to send us credit
        if (_debug_src) 
            cout << "sendRTS 2"<< " flow " << _flow.str() << endl;

        sendRTS();
        return;
    }

    // we've got enough credit already to send this, so see if the NIC
    // is ready right now
    if (_debug_src) cout << "requestSending 4\n"<< " flow " << _flow.str() << endl;;

    bool can_i_send = _nic.requestSending(*this);
    if (can_i_send) {
        bool bytes_sent = sendRtxPacket();
        if (bytes_sent > 0) {
            _nic.startSending(*this, bytes_sent);
        } else {
            _nic.cantSend(*this);
            return;
        }
    }
}

void EqdsSrc::activate() {
    startFlow();
}

void EqdsSrc::setEndTrigger(Trigger& end_trigger) {
    _end_trigger = &end_trigger;
};

////////////////////////////////////////////////////////////////                                                                   
//  EQDS SINK                                                                                                                       
////////////////////////////////////////////////////////////////   

EqdsSink::EqdsSink(TrafficLogger *trafficLogger, EqdsPullPacer* pullPacer, EqdsNIC& nic) :
    DataReceiver("eqdsSink"),
    _nic(nic),
    _flow(trafficLogger), 
    _pullPacer(pullPacer), 
    _expected_epsn(0),
    _high_epsn(0),
    _retx_backlog(0),
    _latest_pull(0),
    _highest_pull_target(0),
    _received_bytes(0),
    _accepted_bytes(0),
    _end_trigger(NULL),
    _epsn_rx_bitmap(0),
    _out_of_order_count(0),
    _ack_request(false)
{
    _nodename = "eqdsSink";  // TBD: would be nice at add nodenum to nodename
    _stats = {0,0,0,0,0};
    _in_pull = false;
    _in_slow_pull = false;
}

EqdsSink::EqdsSink(TrafficLogger* trafficLogger, linkspeed_bps linkSpeed, double rate_modifier, uint16_t mtu, EventList &eventList, EqdsNIC& nic) :
    DataReceiver("eqdsSink"), 
    _nic(nic),
    _flow(trafficLogger), 
    _expected_epsn(0),
    _high_epsn(0),
    _retx_backlog(0),    
    _latest_pull(0),
    _highest_pull_target(0),
    _received_bytes(0),
    _accepted_bytes(0),
    _end_trigger(NULL),
    _epsn_rx_bitmap(0),
    _out_of_order_count(0),
    _ack_request(false)
{
    _pullPacer = new EqdsPullPacer(linkSpeed, rate_modifier, mtu, eventList);
    _stats = {0,0,0,0,0};
    _in_pull = false;
    _in_slow_pull = false;
} 

void EqdsSink::connect(EqdsSrc* src, Route* route){
    _src = src;
    _route = route;
}

void EqdsSink::handlePullTarget(EqdsBasePacket::seq_t pt){
    if (pt > _highest_pull_target){
        _highest_pull_target = pt;

        if (_retx_backlog==0 && !_in_pull){
            _in_pull = true;
            _pullPacer->requestPull(this);
        }
    }
}

/*void EqdsSink::handleReceiveBitmap(){

}*/

void EqdsSink::processData(const EqdsDataPacket& pkt){    
    bool force_ack = false;

    if (_src->debug())
        cout << " EqdsSink "<< _nodename << " src " << _src->nodename() << " processData: " << pkt.epsn()
             << " time " << timeAsNs(getSrc()->eventlist().now()) << " when expected epsn is " << _expected_epsn
             << " ooo count " << _out_of_order_count << " flow " << _src->flow()->str()  << endl;

    _accepted_bytes += pkt.size();

    handlePullTarget(pkt.pull_target());

    if (pkt.epsn() > _high_epsn){
        //highest_received is used to bound the sack bitmap. This is a 64 bit number in simulation, never wraps. 
        //In practice need to handle sequence number wrapping.
        _high_epsn = pkt.epsn();
    }

    //should send an ACK; if incoming packet is ECN marked, the ACK will be sent straight away; 
    //otherwise ack will be delayed until we have cumulated enough bytes / packets. 
    bool ecn = (bool)(pkt.flags() & ECN_CE);

    if (pkt.epsn() < _expected_epsn || _epsn_rx_bitmap[pkt.epsn()]) {
        if (EqdsSrc::_debug) cout << _nodename << " src " << _src->nodename() << " duplicate psn " << pkt.epsn() << endl;

        _stats.duplicates++;

        //sender is confused and sending us duplicates: ACK straight away.
        //this code is different from the proposed hardware implementation, as it keeps track of the ACK state of OOO packets.
        EqdsAckPacket* ack_packet = sack(pkt.path_id(),ecn?pkt.epsn():sackBitmapBase(pkt.epsn()),ecn);
        //ack_packet->sendOn();
        _nic.sendControlPacket(ack_packet);

        _accepted_bytes = 0;//careful about this one.
        return;
    }

    if (_received_bytes == 0) {
        force_ack = true;
    }
    //packet is in window, count the bytes we got. 
    //should only count for non RTS and non trimmed packets.
    _received_bytes += pkt.size() - EqdsAckPacket::ACKSIZE;


    assert(_received_bytes <= _src->flowsize());
    if (_src->debug() && _received_bytes == _src->flowsize())
        cout << _nodename << " received " << _received_bytes << " at " << timeAsUs(EventList::getTheEventList().now())<< endl;

    if (pkt.ar()){
        //this triggers an immediate ack; also triggers another ack later when the ooo queue drains (_ack_request tracks this state)
        force_ack = true; 
        _ack_request = true;
    }


    if (_src->debug()) cout << _nodename << " src " << _src->nodename() << " >>    cumulative ack was: " << _expected_epsn << " flow " << _src->flow()->str() << endl;

    if (pkt.epsn() == _expected_epsn) {
        while (_epsn_rx_bitmap[++_expected_epsn]){
            //clean OOO state, this will wrap at some point.
            _epsn_rx_bitmap[_expected_epsn] = 0;
            _out_of_order_count--;
        }
        if (_src->debug()) cout << " EqdsSink "<< _nodename << " src " << _src->nodename() << " >>    cumulative ack now: " << _expected_epsn << " ooo count " << _out_of_order_count << " flow " << _src->flow()->str()  << endl;

        if (_out_of_order_count==0 && _ack_request){
            force_ack = true;
            _ack_request = false;
        }
    }
    else {
        _epsn_rx_bitmap[pkt.epsn()] = 1;
        _out_of_order_count ++;
        _stats.out_of_order++;
    }

    if (ecn || shouldSack() || force_ack){
        EqdsAckPacket* ack_packet = sack(pkt.path_id(),(ecn||pkt.ar()) ? pkt.epsn():sackBitmapBase(pkt.epsn()),ecn);

        if (_src->debug()) cout << " EqdsSink "<< _nodename << " src " << _src->nodename() << " send ack now: " << _expected_epsn << " ooo count " << _out_of_order_count << " flow " << _src->flow()->str()  << endl;

        _accepted_bytes = 0;

        //ack_packet->sendOn();
        _nic.sendControlPacket(ack_packet);
    }
}

void EqdsSink::processTrimmed(const EqdsDataPacket& pkt){
    _stats.trimmed++;

    if (pkt.epsn() < _expected_epsn || _epsn_rx_bitmap[pkt.epsn()]){
        if (_src->debug())
            cout << " EqdsSink processTrimmed got a packet we already have: " << pkt.epsn() << " time " << timeAsNs(getSrc()->eventlist().now()) << " flow" << _src->flow()->str() << endl;

        EqdsAckPacket* ack_packet = sack(pkt.path_id(),pkt.epsn(),false);
        ack_packet->sendOn();
        return;
    }

    if (_src->debug()) 
        cout << " EqdsSink processTrimmed packet " << pkt.epsn() << " time " << timeAsNs(getSrc()->eventlist().now()) << " flow" << _src->flow()->str() << endl;

    handlePullTarget(pkt.pull_target());

    bool was_retransmitting = _retx_backlog > 0;

    //prioritize credits to this sender! Unclear by how much we should increase here. Assume MTU for now.
    _retx_backlog += EqdsBasePacket::quantize_ceil(EqdsSrc::_mtu);

    if (_src->debug()) 
        cout << "RTX_backlog++ trim: " << pkt.epsn() << " from " << getSrc()->nodename() << " rtx_backlog " << rtx_backlog() << " at " << timeAsUs(getSrc()->eventlist().now()) << " flow " << _src->flow()->str() << endl;

    EqdsNackPacket* nack_packet = nack(pkt.path_id(),pkt.epsn());

    //nack_packet->sendOn();
    _nic.sendControlPacket(nack_packet);

    if (!was_retransmitting){
        //source is now retransmitting, must add it to the list.
        if (_src->debug())
            cout << "PullPacer RequestPull: " << _src->flow()->str() << " at " << timeAsUs(getSrc()->eventlist().now()) << endl;        

        _pullPacer->requestRetransmit(this);
    }
}


void EqdsSink::processRts(const EqdsRtsPacket& pkt){
    assert(pkt.ar());

    handlePullTarget(pkt.pull_target());

    //what happens if this is not an actual retransmit, i.e. the host decides with the ACK that it is
    bool was_retransmitting = _retx_backlog > 0;
    _retx_backlog += pkt.retx_backlog();

    if (_src->debug()) 
        cout << "RTX_backlog++ RTS: " << _src->flow()->str() << " rtx_backlog " << rtx_backlog() << " at " << timeAsUs(getSrc()->eventlist().now()) << endl;        

    if (!was_retransmitting){
        if (!_in_pull){
            //got an RTS but didn't even know that the source was backlogged. This means we lost all data packets in current window. Must add to standard Pull list, to ensure that after RTX phase passes,  the remaining packets are pulled normally
            _in_pull = true;
            _pullPacer->requestPull(this);
        }

        if (_src->debug())
            cout << "1PullPacer RequestRetransmit: " << _src->flow()->str() << " at " << timeAsUs(getSrc()->eventlist().now()) << endl;        

        _pullPacer->requestRetransmit(this);
    }

    bool ecn = (bool)(pkt.flags() & ECN_CE);

    if (pkt.epsn() < _expected_epsn || _epsn_rx_bitmap[pkt.epsn()]) {
        if (_src->debug()) cout << _nodename << " src " << _src->nodename() << " duplicate psn " << pkt.epsn() << endl;

        _stats.duplicates++;

        //sender is confused and sending us duplicates: ACK straight away.
        //this code is different from the proposed hardware implementation, as it keeps track of the ACK state of OOO packets.
        EqdsAckPacket* ack_packet = sack(pkt.path_id(),pkt.epsn(),ecn);
        //ack_packet->sendOn();
        _nic.sendControlPacket(ack_packet);

        _accepted_bytes = 0;//careful about this one.
        return;
    }

    //packet is in window, count the bytes we got. 
    //should only count for non RTS and non trimmed packets.
    _received_bytes += pkt.size() - EqdsAckPacket::ACKSIZE;

    if (pkt.epsn() == _expected_epsn) {
        while (_epsn_rx_bitmap[++_expected_epsn]){
            //clean OOO state, this will wrap at some point.
            _epsn_rx_bitmap[_expected_epsn] = 0;
            _out_of_order_count--;
        }
        if (_src->debug())
            cout << " EqdsSink "<< _nodename << " src " << _src->nodename() << " >>    cumulative ack now: " << _expected_epsn << " ooo count " << _out_of_order_count << " flow " << _src->flow()->str()  << endl;

        if (_out_of_order_count==0 && _ack_request){
            _ack_request = false;
        }
    }
    else {
        _epsn_rx_bitmap[pkt.epsn()] = 1;
        _out_of_order_count ++;
        _stats.out_of_order++;
    }

    EqdsAckPacket* ack_packet = sack(pkt.path_id(),(ecn||pkt.ar()) ? pkt.epsn():sackBitmapBase(pkt.epsn()),ecn);

    if (_src->debug()) cout << " EqdsSink "<< _nodename << " src " << _src->nodename() << " send ack now: " << _expected_epsn << " ooo count " << _out_of_order_count << " flow " << _src->flow()->str()  << endl;

    _accepted_bytes = 0;

    //ack_packet->sendOn();
    _nic.sendControlPacket(ack_packet);
}

void EqdsSink::receivePacket(Packet &pkt) {
    _stats.received ++;
    _stats.bytes_received += pkt.size(); // should this include just the payload?

    switch(pkt.type()){
        case EQDSDATA:
            if (pkt.header_only())
                processTrimmed((const EqdsDataPacket&)pkt);
            else 
                processData((const EqdsDataPacket&)pkt);

            pkt.free();
            break;
        case EQDSRTS:
            processRts((const EqdsRtsPacket&)pkt);
            pkt.free();
            break;
        default:
            abort();
    }
}

uint16_t EqdsSink::nextEntropy(){
    int spraymask = (1 << TGT_EV_SIZE) - 1;
    int fixedmask = ~spraymask;
    int idx = _entropy & spraymask;
    int fixed_entropy = _entropy & fixedmask; 
    int ev = idx++ & spraymask;
    
    _entropy = fixed_entropy | ev; //save for next pkt

    return ev;
}

EqdsPullPacket *EqdsSink::pull() {
    //called when pull pacer is ready to give another credit to this connection.
    //TODO: need to credit in multiple of MTU here.

    if (_retx_backlog > 0){
        if (_retx_backlog > EqdsSink::_credit_per_pull)
            _retx_backlog -= EqdsSink::_credit_per_pull;
        else
            _retx_backlog = 0;
    
        if (EqdsSrc::_debug) cout << "RTX_backlog--: " << getSrc()->nodename() << " rtx_backlog " << rtx_backlog() << " at " << timeAsUs(getSrc()->eventlist().now()) << " flow " << _src->flow()->str() << endl;        
    }


    _latest_pull += EqdsSink::_credit_per_pull; 

    EqdsPullPacket* pkt = NULL;
    pkt = EqdsPullPacket::newpkt(_flow, *_route, _latest_pull,nextEntropy(),_srcaddr);

    return pkt;
}

bool EqdsSink::shouldSack(){
    return _accepted_bytes > _bytes_unacked_threshold;
}

EqdsBasePacket::seq_t EqdsSink::sackBitmapBase(EqdsBasePacket::seq_t epsn){
    return  max((int64_t)epsn - 63,(int64_t)(_expected_epsn+1));
}

EqdsBasePacket::seq_t EqdsSink::sackBitmapBaseIdeal(){
    uint8_t lowest_value = UINT8_MAX;
    EqdsBasePacket::seq_t lowest_position;

    //find the lowest non-zero value in the sack bitmap; that is the candidate for the base, since it is the oldest packet that we are yet to sack.
    //on sack bitmap construction that covers a given seqno, the value is incremented. 
    for (EqdsBasePacket::seq_t crt = _expected_epsn; crt <= _high_epsn; crt++)
        if (_epsn_rx_bitmap[crt] && _epsn_rx_bitmap[crt]<lowest_value){
            lowest_value = _epsn_rx_bitmap[crt];
            lowest_position = crt;
        }
    
    if (lowest_position + 64 > _high_epsn)
        lowest_position = _high_epsn - 64;

    if (lowest_position <= _expected_epsn)
        lowest_position = _expected_epsn + 1;
    
    return lowest_position;
}

uint64_t EqdsSink::buildSackBitmap(EqdsBasePacket::seq_t ref_epsn){
    //take the next 64 entries from ref_epsn and create a SACK bitmap with them
    if (_src->debug())
        cout << " EqdsSink: building sack for ref_epsn " << ref_epsn << endl;
    uint64_t bitmap = (uint64_t)(_epsn_rx_bitmap[ref_epsn]!=0) << 63;

    for (int i=1;i<64;i++){
        bitmap = bitmap >> 1 | (uint64_t)(_epsn_rx_bitmap[ref_epsn+i]!=0) << 63;
        if (_src->debug() && (_epsn_rx_bitmap[ref_epsn+i]!=0))
            cout << "     Sack: " <<  ref_epsn+i << endl;

        if (_epsn_rx_bitmap[ref_epsn+i]){
            //remember that we sacked this packet
            if (_epsn_rx_bitmap[ref_epsn+i]<UINT8_MAX)
                _epsn_rx_bitmap[ref_epsn+i]++;
        }
    }
    if (_src->debug())
        cout << "       bitmap is: " << bitmap << endl;
    return bitmap;
}

EqdsAckPacket *EqdsSink::sack(uint16_t path_id, EqdsBasePacket::seq_t seqno, bool ce) {
    uint64_t bitmap = buildSackBitmap(seqno);
    EqdsAckPacket* pkt = EqdsAckPacket::newpkt(_flow, *_route, _expected_epsn,seqno,path_id,ce,_srcaddr);
    pkt->set_bitmap(bitmap);
    return pkt;
}

EqdsNackPacket *EqdsSink::nack(uint16_t path_id, EqdsBasePacket::seq_t seqno) {
    EqdsNackPacket* pkt = EqdsNackPacket::newpkt(_flow, *_route, seqno, path_id, _srcaddr);
    return pkt;
}

void EqdsSink::setEndTrigger(Trigger& end_trigger) {
    _end_trigger = &end_trigger;
};

static unsigned pktByteTimes(unsigned size) {
    // IPG (96 bit times) + preamble + SFD + ether header + FCS = 38B
    return max(size, 46u) + 38;
}

uint32_t EqdsSink::reorder_buffer_size() {
    uint32_t count = 0;
    // it's not very efficient to count each time, but if we only do
    // this occasionally when the sink logger runs, it should be OK.
    for (uint32_t i = 0; i < eqdsMaxInFlightPkts; i++) {
        if (_epsn_rx_bitmap[i]) count++;
    }
    return count;
}

////////////////////////////////////////////////////////////////                                                                   
//  EQDS PACER
////////////////////////////////////////////////////////////////

// pull rate modifier should generally be something like 0.99 so we pull at just less than line rate
EqdsPullPacer::EqdsPullPacer(linkspeed_bps linkSpeed, double pull_rate_modifier, uint16_t mtu, EventList &eventList) :
    EventSource(eventList, "eqdsPull"), _pktTime(pull_rate_modifier * 8 * pktByteTimes(mtu) * 1e12 / linkSpeed) {
    _active = false;
}

void EqdsPullPacer::doNextEvent() {
    if (_rtx_senders.empty() && _active_senders.empty() && _idle_senders.empty()) {
        _active = false;
        return;
    }

    EqdsSink* sink = NULL;
    EqdsPullPacket *pullPkt;

    if (!_rtx_senders.empty()){
        sink = _rtx_senders.front();
        _rtx_senders.pop_front();

        pullPkt = sink->pull();
        if (EqdsSrc::_debug) cout << "PullPacer: RTX: " << sink->getSrc()->nodename() << " rtx_backlog " << sink->rtx_backlog() << " at " << timeAsUs(eventlist().now()) << endl;
        // TODO if more pulls are needed, enqueue again
        if (sink->rtx_backlog()>0)
            _rtx_senders.push_back(sink);
    }
    else if (!_active_senders.empty()){
        sink = _active_senders.front();

        assert(sink->inPullQueue());

        _active_senders.pop_front();
        pullPkt = sink->pull();

        // TODO if more pulls are needed, enqueue again
        if (EqdsSrc::_debug) cout << "PullPacer: Active: " << sink->getSrc()->nodename() << " backlog " << sink->backlog() << " at " << timeAsUs(eventlist().now()) << endl;
        if (sink->backlog()>0)
            _active_senders.push_back(sink);
        else { //this sink has had its demand satisfied, move it to idle senders list.
            _idle_senders.push_back(sink);
            sink->removeFromPullQueue();
            sink->addToSlowPullQueue();
        }
    }
    else { //no active senders, we must have at least one idle sender
        sink = _idle_senders.front();
        _idle_senders.pop_front();
        if(!sink->inSlowPullQueue())
            sink->addToSlowPullQueue();

        if (EqdsSrc::_debug) cout << "PullPacer: Idle: " << sink->getSrc()->nodename() << " at " << timeAsUs(eventlist().now()) << " backlog " << sink->backlog() << " " << sink->slowCredit() << " max " << EqdsBasePacket::quantize_floor(sink->getMaxCwnd()) <<endl;
        pullPkt = sink->pull();
        pullPkt->set_slow_pull(true);

        if (sink->backlog() == 0 && sink->slowCredit() < EqdsBasePacket::quantize_floor(sink->getMaxCwnd())){
            //only send upto 1BDP worth of speculative credit.
            //backlog will be negative once this source starts receiving speculative credit. 
            _idle_senders.push_back(sink);
        }
        else
            sink->removeFromSlowPullQueue();
    }

    pullPkt->flow().logTraffic(*pullPkt, *this, TrafficLogger::PKT_SEND);

    //pullPkt->sendOn();
    sink->getNIC()->sendControlPacket(pullPkt);
    _active = true;

    eventlist().sourceIsPendingRel(*this, _pktTime);
}

bool EqdsPullPacer::isActive(EqdsSink* sink){
    for (auto i = _active_senders.begin(); i != _active_senders.end(); i++) {
        if(*i == sink)
            return true;
    }
    return false;
}

bool EqdsPullPacer::isRetransmitting(EqdsSink* sink){
    for (auto i = _rtx_senders.begin(); i != _rtx_senders.end(); i++) {
        if(*i == sink)
            return true;
    }
    return false;
}

bool EqdsPullPacer::isIdle(EqdsSink* sink){
    for (auto i = _idle_senders.begin(); i != _idle_senders.end(); i++) {
        if(*i == sink)
            return true;
    }
    return false;
}


void EqdsPullPacer::requestPull(EqdsSink *sink) {
    if (isActive(sink)){
        abort(); 
    }
    assert (sink->inPullQueue());

    _active_senders.push_back(sink);
    // TODO ack timer

    if (!_active) {
        eventlist().sourceIsPendingRel(*this, 0);
        _active = true;
    }
}

void EqdsPullPacer::requestRetransmit(EqdsSink *sink) {
    assert (!isRetransmitting(sink));
    
    _rtx_senders.push_back(sink);
    // TODO ack timer

    if (!_active) {
        eventlist().sourceIsPendingRel(*this, 0);
        _active = true;
    }
        
}
