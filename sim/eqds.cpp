// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "eqds.h"
#include <math.h>

using namespace std;


// Static stuff

// _path_entropy_size is the number of paths we spray across.  If you don't set it, it will default to all paths.
uint32_t EqdsSrc::_path_entropy_size = 256;
int EqdsSrc::_global_node_count = 0;

/* _min_rto can be tuned using setMinRTO. Don't change it here.  */
simtime_picosec EqdsSrc::_min_rto = timeFromUs((uint32_t)DEFAULT_EQDS_RTO_MIN);

mem_b EqdsSink::_bytes_unacked_threshold = 16384;
mem_b EqdsSink::_credit_per_pull = 4096;

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
}

// srcs call request_sending to see if they can send now.  If the
// answer is no, they'll be called back when it's time to send.
bool EqdsNIC::requestSending(EqdsSrc& src) {
    if (_send_end_time >= eventlist().now()) {
        // we're already sending
        if (_num_queued_srcs == 0) {
            // need to schedule the callback
            eventlist().sourceIsPending(*this,_send_end_time);
            assert(_num_queued_srcs == 0);
        }
        _num_queued_srcs += 1;
        _active_srcs.push_back(&src);
        return false;
    }
    assert(_num_queued_srcs == 0);
    return true;
}

// srcs call startSending when they are allowed to actually send
void EqdsNIC::startSending(EqdsSrc& src, mem_b pkt_size) {
    //if (EqdsSrc::_debug) cout << "startSending at " << timeAsUs(eventlist().now()) << endl;
    // sanity checks - remove later 
    if (_num_queued_srcs > 0) {
        EqdsSrc *queued_src = _active_srcs.front();
        _active_srcs.pop_front();
        _num_queued_srcs--;
        assert(_num_queued_srcs >= 0);
        assert(queued_src == &src);
    }
    assert(eventlist().now() >= _send_end_time);

    _send_end_time = eventlist().now() + (pkt_size * 8 * timeFromSec(1.0))/_linkspeed;
    if (_num_queued_srcs > 0) {
        eventlist().sourceIsPending(*this,_send_end_time);
    }
}

// srcs call cantSend when they previously requested to send, and now its their turn, they can't for some reason.
void EqdsNIC::cantSend(EqdsSrc& src) {
    _num_queued_srcs--;
    assert(_num_queued_srcs >= 0);
    EqdsSrc *queued_src = _active_srcs.front();
    _active_srcs.pop_front();
    assert(queued_src == &src);
    assert(eventlist().now() >= _send_end_time);

    if (_num_queued_srcs > 0) {
        // give the next src a chance.
        queued_src = _active_srcs.front();
        queued_src->timeToSend();
    }
}

void EqdsNIC::doNextEvent() {
    assert(eventlist().now() == _send_end_time);
    assert(_num_queued_srcs > 0);
    // it's time for the next source to send
    EqdsSrc *queued_src = _active_srcs.front();
    queued_src->timeToSend();
}

////////////////////////////////////////////////////////////////                                                                   
//  EQDS SRC
////////////////////////////////////////////////////////////////   

EqdsSrc::EqdsSrc(TrafficLogger *trafficLogger, EventList &eventList, EqdsNIC &nic, bool rts) :
    EventSource(eventList, "eqdsSrc"), _nic(nic), _rts(rts), _flow(trafficLogger)
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
    _highest_pull = 0;
    _received_credit = 0;
    _speculative_credit = _maxwnd;
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
    _path_penalties.resize(_no_of_paths);
    for (uint32_t i = 0; i < _no_of_paths; i++) {
        _path_penalties[i] = 0;
    }
    
    // by default, end silently
    _end_trigger = 0;

    _dstaddr = UINT32_MAX;
    _route = NULL;
    _mtu = Packet::data_packet_size();
    _mss = _mtu - _hdr_size;
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

simtime_picosec EqdsSrc::computeRTO(simtime_picosec send_time) {
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

    if (_debug) {
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
    auto i = _active_packets.find(ackno);
    if (i == _active_packets.end())
        return;
    //mem_b pkt_size = i->second.pkt_size;
    simtime_picosec send_time = i->second.send_time;

    computeRTO(send_time);

    mem_b pkt_size = i->second.pkt_size;
    _in_flight -= pkt_size;
    assert(_in_flight >= 0);
    if (EqdsSrc::_debug) cout << _nodename << " handleAck " << ackno << " flow " << _flow.str() << endl;
    _active_packets.erase(i);
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

    auto i = _active_packets.begin();
    while (i != _active_packets.end()) {
        auto seqno = i->first;
        // cumulative ack is next expected packet, not yet received
        if (seqno >= cum_ack) {
            // nothing else acked
            break;
        }
        mem_b pkt_size = i->second.pkt_size;
        simtime_picosec send_time = i->second.send_time;

        computeRTO(send_time);

        _in_flight -= pkt_size;
        assert(_in_flight >= 0);
        if (EqdsSrc::_debug) cout << _nodename << " handleCumAck " << seqno << " flow " << _flow.str() << endl;
        _active_packets.erase(i);
        i = _active_packets.begin();
        _send_times.erase(send_time);
        if (send_time == _rto_send_time) {
            recalculateRTO();
        }
    }
}

void EqdsSrc::handlePull(mem_b pullno) {
    if (pullno > _highest_pull) {
        mem_b extra_credit = pullno - _highest_pull;
        _received_credit += extra_credit;
        _highest_pull = pullno;
    }
}

bool EqdsSrc::checkFinished(EqdsDataPacket::seq_t cum_ack) {
    // cum_ack gives the next expected packet
    if (_done_sending) {
        // if (EqdsSrc::_debug) cout << _nodename << " checkFinished done sending " << " cum_acc " << cum_ack << " mss " << _mss << " c*m " << cum_ack * _mss << endl;
        return true;
    }
    if (EqdsSrc::_debug) 
        cout << _nodename << " checkFinished " << " cum_acc " << cum_ack << " mss " << _mss << " RTS sent " << _rts_packets_sent << " total bytes " << (cum_ack - _rts_packets_sent) * _mss << " flow_size " << _flow_size << " done_sending " << _done_sending << endl;

    if ((((mem_b)cum_ack -_rts_packets_sent) * _mss) >= _flow_size) {
        cout << "Flow " << _name << " flowId " << flowId() << " " << _nodename << " finished at " << timeAsUs(eventlist().now()) << " total packets " << cum_ack << " RTS " << _rts_packets_sent << " total bytes " << ((mem_b)cum_ack - _rts_packets_sent) * _mss << endl;
        if (_end_trigger) {
            _end_trigger->activate();
        }
        _done_sending = true;
        return true;
    }
    return false;
}

void EqdsSrc::processAck(const EqdsAckPacket& pkt) {
    auto cum_ack = pkt.cumulative_ack();
    handleCumulativeAck(cum_ack);

    if (EqdsSrc::_debug) 
        cout << _nodename << " processAck cum_ack: " << cum_ack << " flow " << _flow.str() << endl;
 
    auto ackno = pkt.ref_ack();
    uint64_t bitmap = pkt.bitmap();
    while (bitmap > 0) { 
        if (bitmap & 1) {
            if (EqdsSrc::_debug) 
                cout << "Sack " << ackno << " flow " << _flow.str() << endl;

            handleAckno(ackno);
        }
        ackno++;
        bitmap >>= 1;
    }

    auto pullno = pkt.pullno();
    handlePull(pullno);

    // handle ECN echo
    if (pkt.ecn_echo()) {
        penalizePath(pkt.ev(), 1);
    }

    if (checkFinished(cum_ack))
        return;

    clearSpeculativeCredit();
    sendIfPermitted();
}

void EqdsSrc::processNack(const EqdsNackPacket& pkt) {
    auto pullno = pkt.pullno();
    handlePull(pullno);

    auto nacked_seqno = pkt.ref_ack();
    if (EqdsSrc::_debug) 
        cout << _nodename << " processNack nacked: " << nacked_seqno << " flow " <<_flow.str() << endl;

    uint16_t ev = pkt.ev();
    // what should we do when we get a NACK with ECN_ECHO set?  Presumably ECE is superfluous?
    //bool ecn_echo = pkt.ecn_echo();

    // move the packet to the RTX queue
    auto i = _active_packets.find(nacked_seqno);
    if (i == _active_packets.end()) {
        if (EqdsSrc::_debug) 
            cout << "Didn't find NACKed packet in _active_packets flow " << _flow.str() << endl;
        // don't think this can happen in sim, but if it can, change abort to return
        abort();
    }
    mem_b pkt_size = i->second.pkt_size;
    assert(pkt_size > _hdr_size); // check we're not seeing NACKed RTS packets.
    
    auto seqno = i->first;
    simtime_picosec send_time = i->second.send_time;

    computeRTO(send_time);

    if (EqdsSrc::_debug) cout << _nodename << " erasing send record, seqno: " << seqno << " flow " << _flow.str() << endl;
    _active_packets.erase(i);
    assert(_active_packets.find(seqno) == _active_packets.end()); // xxx remove when working
    
    _send_times.erase(send_time);
    queueForRtx(seqno, pkt_size);

    if (send_time == _rto_send_time) {
        recalculateRTO();
    }

    penalizePath(ev, 1);
    clearSpeculativeCredit();
    sendIfPermitted();
}

void EqdsSrc::processPull(const EqdsPullPacket& pkt) {
    auto cum_ack = pkt.cumulative_ack();
    handleCumulativeAck(cum_ack);

    auto pullno = pkt.pullno();
    if (EqdsSrc::_debug) cout << _nodename << " processPull " << pullno << " cum_ack " << cum_ack << " flow " << _flow.str() << endl;

    handlePull(pullno);

    if (checkFinished(cum_ack))
        return;

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
        if (EqdsSrc::_debug) cout << "Starting flow " << _name << endl;                                                                                           
        startFlow();
    }
}

void EqdsSrc::setFlowsize(uint64_t flow_size_in_bytes) {
    _flow_size = flow_size_in_bytes;
}

void EqdsSrc::startFlow() {
    _cwnd = _maxwnd;
    _speculative_credit = _maxwnd;
    if (EqdsSrc::_debug) cout << "startflow " <<  _flow._name <<  " CWND " << _cwnd << " at " << timeAsUs(eventlist().now()) << " flow " << _flow.str() << endl;
    clearRTO();
    _in_flight = 0;
    _pull_target = 0;
    _highest_pull = 0;
    _unsent = _flow_size;
    _last_rts = 0;
    // backlog is total amount of data we expect to send, including headers
    _backlog = ceil(((double)_flow_size)/_mss) * _hdr_size + _flow_size;
    _send_blocked_on_nic = false;
    while (_send_blocked_on_nic == false && credit() > _mtu && _unsent > 0) {
        if (EqdsSrc::_debug) cout << "requestSending 0 "<< " flow " << _flow.str() << endl;

        bool can_i_send = _nic.requestSending(*this);
        if (can_i_send) {
            mem_b sent_bytes = sendNewPacket();
            if (sent_bytes > 0) {
                _nic.startSending(*this, sent_bytes);
            }
        } else {
            _send_blocked_on_nic = true;
            return;
        }
    }
}

mem_b EqdsSrc::credit() const {
    return _received_credit + _speculative_credit;
}

void EqdsSrc::clearSpeculativeCredit() {
    // we just got an ack or nack.  Any speculative credit we used at
    // startup should now be discarded because we've used enough of it
    // to bootstrap a window.
    //
    // Note we may need to do something different here to handle bursty
    // sources and speculative credit that isn't initial startup
    // credit.  
    _speculative_credit = 0;
}

bool EqdsSrc::spendCredit(mem_b pktsize) {
    assert(credit() >= pktsize);
    bool speculative = false;
    _received_credit -= pktsize;
    if (_received_credit < 0) {
        // need to spend speculative credit
        _speculative_credit += _received_credit;
        _received_credit = 0;
        speculative = true;
    }
    return speculative;
}

mem_b EqdsSrc::computePullTarget() {
    mem_b pull_target = _backlog;
    if (pull_target > _cwnd + _mtu) {
        pull_target = _cwnd + _mtu;
    }
    if (pull_target > _maxwnd) {
        pull_target = _maxwnd;
    }
    pull_target = pull_target + _highest_pull - _received_credit - _speculative_credit;
    return pull_target;
}

void EqdsSrc::sendIfPermitted() {
    // send if the NIC, credit and window allow.

    // how large will the packet be?
    mem_b pkt_size = 0;
    if (_rtx_queue.empty()) {
        if (_backlog == 0) {
            // nothing to retransmit, and no backlog.  Nothing to do here.
            if (_received_credit > 0) {
                if (EqdsSrc::_debug) cout << "we have " << _received_credit << " bytes of credit, but nothing to use it on"<< " flow " << _flow.str() << endl;
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

    if (pkt_size > credit()) {
        return;
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
    if (EqdsSrc::_debug) cout << "requestSending 1\n";
    bool can_i_send = _nic.requestSending(*this);
    if (can_i_send) {
        mem_b sent_bytes = sendPacket();
        if (sent_bytes > 0) {
            _nic.startSending(*this, sent_bytes);
        }
    } else {
        // we can't send yet, but NIC will call us back when we can
        _send_blocked_on_nic = true;
        return;
    }    
}

// if sendPacket got called, we have already asked the NIC for
// permission, and we've already got both credit and cwnd to send, so
// we will be sending something...
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
    _path_penalties[path_id] += penalty;
    if (_path_penalties[path_id] > _max_penalty) {
        _path_penalties[path_id] = _max_penalty;
    }
}

uint16_t EqdsSrc::nextEntropy() {
    // _no_of_paths must be a power of 2
    uint16_t mask = _no_of_paths - 1;  
    uint16_t entropy = (_current_ev_index ^ _path_xor) & mask;
    while (_path_penalties[entropy] > 0) {
        _path_penalties[entropy]--;
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
    if (EqdsSrc::_debug) cout << _nodename << " sendNewPacket highest_sent " << _highest_sent << " h*m " << _highest_sent * _mss << " backlog " << _backlog << " unsent " << _unsent << " flow " << _flow.str() << endl;
    assert(_unsent > 0);
    assert(((mem_b)_highest_sent - _rts_packets_sent) * _mss < _flow_size);
    mem_b payload_size = _mss;
    if (_unsent < payload_size) {
        payload_size = _unsent;
    }
    assert(payload_size > 0);
    mem_b full_pkt_size = payload_size + _hdr_size;
    
    _backlog -= full_pkt_size;
    assert(_backlog >= 0);
    _unsent -= payload_size;
    assert(_backlog >= _unsent);
    _in_flight += full_pkt_size;
    bool speculative = spendCredit(full_pkt_size);
    auto ptype = EqdsDataPacket::DATA;
    if (speculative) {
        ptype = EqdsDataPacket::SPECULATIVE;
    }
    _pull_target = computePullTarget();

    auto *p = EqdsDataPacket::newpkt(_flow, *_route, _highest_sent, full_pkt_size, ptype, _pull_target, _dstaddr);
    uint16_t ev = nextEntropy();
    p->set_pathid(ev);
    p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);

    createSendRecord(_highest_sent, full_pkt_size);
    if (EqdsSrc::_debug) cout << _flow.str() << " sending pkt " << _highest_sent << " size " << full_pkt_size << " pull target " << _pull_target << " at " << timeAsUs(eventlist().now())<<endl;
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
    _rtx_queue.erase(_rtx_queue.begin());
    _in_flight += full_pkt_size;
    spendCredit(full_pkt_size);
    auto *p = EqdsDataPacket::newpkt(_flow, *_route, seq_no, full_pkt_size,
                                     EqdsDataPacket::DATA, _pull_target, _dstaddr);
    uint16_t ev = nextEntropy();
    p->set_pathid(ev);
    p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);
    _backlog -= full_pkt_size;
    assert(_backlog >= 0);

    createSendRecord(seq_no, full_pkt_size);

    if (EqdsSrc::_debug) cout << _nodename << " sending rtx pkt " << seq_no << " size " << full_pkt_size << " flow " << _flow.str() << " at " << timeAsUs(eventlist().now())<< endl;
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
    if (EqdsSrc::_debug) cout << _nodename << " sendRTS, route: " << _route << " flow " << _flow.str() << " at " << timeAsUs(eventlist().now()) << " last RTS " << timeAsUs(_last_rts) << endl;
    createSendRecord(_highest_sent, _hdr_size);
    auto *p = EqdsRtsPacket::newpkt(_flow, *_route, _highest_sent, _hdr_size,
                                    _pull_target, _dstaddr);
    p->set_dst(_dstaddr);
    uint16_t ev = nextEntropy();
    p->set_pathid(ev);
    p->sendOn();
    _highest_sent++;
    _rts_packets_sent++;
    _last_rts = eventlist().now();
    startRTO(eventlist().now());
}

void EqdsSrc::createSendRecord(EqdsBasePacket::seq_t seqno, mem_b full_pkt_size) {
    //assert(full_pkt_size > 64);
    if (EqdsSrc::_debug) cout << _nodename << " createSendRecord seqno: " << seqno << " size " << full_pkt_size << endl;
    assert(_active_packets.find(seqno) == _active_packets.end());
    _active_packets.emplace(seqno, sendRecord(full_pkt_size, eventlist().now()));
    _send_times.emplace(eventlist().now(), seqno);
}

void EqdsSrc::queueForRtx(EqdsBasePacket::seq_t seqno, mem_b pkt_size) {
    // need to increase the credit we'll ask for - as far as credit
    // requesting, this acts like new data we need to send
    _backlog += pkt_size;
    
    assert(_rtx_queue.find(seqno) == _rtx_queue.end());
    _rtx_queue.emplace(seqno, pkt_size);
    sendIfPermitted();
}

void EqdsSrc::timeToSend() {
    if (EqdsSrc::_debug) cout << "timeToSend" << " flow " << _flow.str() << endl;;
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
        if (EqdsSrc::_debug) cout << "cantSend\n";
        _nic.cantSend(*this);
        return;
    }
#endif

    // do we have enough credit?
    if (credit() < full_pkt_size) {
        if (EqdsSrc::_debug) cout << "cantSend"<< " flow " << _flow.str() << endl;;
        _nic.cantSend(*this);
        return;
    }

    // OK, we're good to send.
    if (_rtx_queue.empty()) {
        sendNewPacket();
    } else {
        sendRtxPacket();
    }

    // let the NIC know we sent, so it can calculate next send time.
    _nic.startSending(*this, full_pkt_size);

#ifdef USE_CWND
    if (_cwnd < full_pkt_size) {
        return;
    }
#endif
    // do we have enough credit to send again?
    if (credit() < full_pkt_size) {
        return;
    }

    if (_unsent == 0 && _rtx_queue.empty()) {
        // we're done - nothing more to send.
        assert(_backlog == 0);
        return;
    }

    // we're ready to send again.  Let the NIC know.
    assert(!_send_blocked_on_nic);
    if (EqdsSrc::_debug) cout << "requestSending2"<< " flow " << _flow.str() << endl;;
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

    auto send_record = _active_packets.find(seqno);
    assert(send_record != _active_packets.end());
    mem_b pkt_size = send_record->second.pkt_size;

    //update flightsize?

    _send_times.erase(first_entry);
    if (EqdsSrc::_debug) cout << _nodename << " rtx timer expired for " << seqno << " flow " << _flow.str() << endl;
    _active_packets.erase(send_record);
    recalculateRTO();

    if (!_rtx_queue.empty()) {
        // there's already a queue, so clearly we shouldn't just
        // resend right now.  But send an RTS (no more than once per
        // RTT) to cover the case where the receiver doesn't know
        // we're waiting.
        queueForRtx(seqno, pkt_size);
        sendRTS();
        
        if (EqdsSrc::_debug) 
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

    if (credit() < pkt_size ) {
        // we don't have any credit to send.  Send an RTS (no more
        // than once per RTT) to cover the case where the receiver
        // doesn't know to send us credit
        if (EqdsSrc::_debug) 
            cout << "sendRTS 2"<< " flow " << _flow.str() << endl;

        sendRTS();
        return;
    }

    // we've got enough credit already to send this, so see if the NIC
    // is ready right now
    if (EqdsSrc::_debug) cout << "requestSending 4\n"<< " flow " << _flow.str() << endl;;

    bool can_i_send = _nic.requestSending(*this);
    if (can_i_send) {
        sendRtxPacket();
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

EqdsSink::EqdsSink(TrafficLogger *trafficLogger, EqdsPullPacer* pullPacer) :
    DataReceiver("eqdsSink"),
    _flow(trafficLogger), 
    _pullPacer(pullPacer), 
    _cumulative_ack(0),
    _highest_received(0),
    _rtx_backlog(0),
    _pull_no(0),
    _highest_pull_target(0),
    _bytes_unacked(0),
    _received_bytes(0),
    _end_trigger(NULL),
    _out_of_order(0)
{
    _nodename = "eqdsSink";  // TBD: would be nice at add nodenum to nodename
    _stats.bytes_received = 0;
}

EqdsSink::EqdsSink(TrafficLogger* trafficLogger, linkspeed_bps linkSpeed, double rate_modifier, uint16_t mtu, EventList &eventList) :
    DataReceiver("eqdsSink"), _flow(trafficLogger), 
    _cumulative_ack(0),
    _highest_received(0),
    _rtx_backlog(0),    
    _pull_no(0),
    _highest_pull_target(0),
    _bytes_unacked(0),
    _received_bytes(0),
    _end_trigger(NULL),
    _out_of_order(0)
{
    _pullPacer = new EqdsPullPacer(linkSpeed, rate_modifier, mtu, eventList);
    _stats.bytes_received = 0;
} 

void EqdsSink::connect(EqdsSrc* src, Route* route){
    _src = src;
    _route = route;
}

void EqdsSink::receivePacket(Packet &pkt) {
    _stats.received ++;
    _stats.bytes_received += pkt.size(); // should this include just the payload?

    if (pkt.type() != EQDSDATA && pkt.type() != EQDSRTS ) {
        assert(pkt.bounced());
        cerr << "Got bounced packet - discarded.\n";
        pkt.free();
        return;
    }

    bool was_retransmitting = rtx_backlog()>0; // source currently in RTX pull pacer list
    bool was_backlogged = (_highest_pull_target > _pull_no); //source currently in standard pacer list. 
    bool is_rts = false;

    if (pkt.type() == EQDSRTS) {
        //handle RTS here;

        EqdsRtsPacket* p = dynamic_cast<EqdsRtsPacket*>(&pkt);
        assert(p->ar());

        //what happens if this is not an actual retransmit, i.e. the host decides with the ACK that it is
        _rtx_backlog += p->retx_backlog();
        is_rts = true; 

        if (EqdsSrc::_debug) 
            cout << "RTX_backlog++ RTS: " << _src->flow()->str() << " rtx_backlog " << rtx_backlog() << " at " << timeAsUs(getSrc()->eventlist().now()) << endl;        

        //EQDSRTS packets are also data packets, keep going.
        //return;

        if (!was_retransmitting){
            if (!was_backlogged){
                //got an RTS but didn't even know that the source was backlogged. This means we lost all data packets in current window. Must add to standard Pull list, to ensure that after RTX phase passes,  the remaining packets are pulled normally
                _pullPacer->requestPull(this);
            }   

            if (EqdsSrc::_debug)
                cout << "PullPacer RequestRetransmit: " << _src->flow()->str() << " at " << timeAsUs(getSrc()->eventlist().now()) << endl;        

            _pullPacer->requestRetransmit(this);
            was_retransmitting = true;
        }
    }
    else if (pkt.type() != EQDSDATA) {
        cerr << "Got unknown packet type!\n";
        abort();
    }

    EqdsDataPacket* p = dynamic_cast<EqdsDataPacket*>(&pkt);

    if (EqdsSrc::_debug)
        cout << "eqdsSnk: " << _src->flow()->str() << " received packet " << pkt.str() << " epsn " << p->epsn() << " cumulative ack " << _cumulative_ack << " at " << timeAsUs(getSrc()->eventlist().now()) << endl;        

    if (p->pull_target() > _highest_pull_target){
        _highest_pull_target = p->pull_target();

        /*if (!was_retransmitting && rtx_backlog()>0){
            //add this to the retransmitting senders list only if it was not already there.
            _pullPacer->requestRetransmit(this);
        }
        else*/          
        if (!was_backlogged && !was_retransmitting){
            //add this to the active senders list only if it was not already there and source is not already retransmitting, which means it is already in the high priority class.
            _pullPacer->requestPull(this);
        }
    }
   
    if (p->header_only() && !is_rts) { //got a trimmed packet, send NACK.
        if (EqdsSrc::_debug) cout << " >>    received trimmed packet " << p->epsn() << " flow" << _src->flow()->str() << endl;
        _stats.trimmed++;

        //prioritize credits to this sender! Unclear by how much we should increase here. Assume MTU for now.
        _rtx_backlog += EqdsSrc::_mtu;

        if (EqdsSrc::_debug) cout << "RTX_backlog++ trim: " << p->epsn() << " from " << getSrc()->nodename() << " rtx_backlog " << rtx_backlog() << " at " << timeAsUs(getSrc()->eventlist().now()) << " flow " << _src->flow()->str() << endl;

        EqdsNackPacket* nack_packet = nack(p->pathid(),p->epsn());
        pkt.free();

        nack_packet->sendOn();

        if (!was_retransmitting){
            //source is now retransmitting, must add it to the list.
            if (EqdsSrc::_debug)
                cout << "PullPacer RequestPull: " << _src->flow()->str() << " at " << timeAsUs(getSrc()->eventlist().now()) << endl;        

            _pullPacer->requestRetransmit(this);
        }

        return;
    }
   // if (EqdsSrc::_debug) cout << _nodename << " src " << _src->nodename() << " >>    received packet " << p->epsn() << " size " << p->size() << " flow " << _src->flow()->str() << endl;

    //need to quantize here the packet size, as suggested in the eEQDS spec. Using actual size for now.
    _bytes_unacked += p->size();

    if (p->epsn() > _highest_received){
        //highest_received is used to bound the sack bitmap. This is a 64 bit number in simulation, never wraps. 
        //In practice need to handle sequence number wrapping.
        _highest_received = p->epsn();
    }

    //should send an ACK; if incoming packet is ECN marked, the ACK will be sent straight away; 
    //otherwise ack will be delayed and sent when the pull pacer triggers, or when the ACK timer triggers. 
    bool ecn = (bool)(p->flags() & ECN_CE);

    if (p->epsn() < _cumulative_ack || _out_of_order[p->epsn()]) {
        if (EqdsSrc::_debug) cout << _nodename << " src " << _src->nodename() << " duplicate psn " << p->epsn() << endl;
        _stats.duplicates++;

        //sender is confused and sending us duplicates: ACK straight away.
        //this code is different from the proposed hardware implementation, as it keeps track of the ACK state of OOO packets.
        EqdsAckPacket* ack_packet = sack(p->path_id(),ecn?p->epsn():sackBitmapBase(p->epsn()),ecn);
        ack_packet->sendOn();
        _bytes_unacked = 0;//careful about this one.
        p->free();
        return;
    }

    //packet is in window, count the bytes we got. 
    _received_bytes += p->size() - ACKSIZE;

    bool force_ack = false;
    if (EqdsSrc::_debug) cout << _nodename << " src " << _src->nodename() << " >>    cumulative ack was: " << _cumulative_ack << " flow " << _src->flow()->str() << endl;
    if (p->epsn() == _cumulative_ack) {
        force_ack = true; // force an ack every time cumulative_ack increases - ensures we ack the last packets of a flow
        while (_out_of_order[++_cumulative_ack]){
            //clean OOO state, this will wrap at some point.
            _out_of_order[_cumulative_ack] = 0;
        }
        if (EqdsSrc::_debug) cout << _nodename << " src " << _src->nodename() << " >>    cumulative ack now: " << _cumulative_ack << " flow " << _src->flow()->str()  << endl;

        //the code below will trigger earlier when getting out-of-order packets at the tail of the flow. 
        if (_received_bytes >= (mem_b)_src->flowsize()) {
            //if (EqdsSrc::_debug) cout << "Flow " << _name << " flowId " << _src->flowId() << " dstfinished at total bytes " << _cumulative_ack * _src->_mss << endl;
            //end triggers are unreliable at this point!
            /*if (_end_trigger) {
                _end_trigger->activate();
            }*/
        }
    }
    else {
        _out_of_order[p->epsn()] = 1;
        _stats.out_of_order++;
    }

    if (ecn || shouldSack() || force_ack || p->ar()){
        EqdsAckPacket* ack_packet = sack(p->path_id(),(ecn||p->ar()) ? p->epsn():sackBitmapBase(p->epsn()),ecn);
        _bytes_unacked = 0;

        ack_packet->sendOn();
    }
    p->free();
}

EqdsBasePacket *EqdsSink::pull() {
    //called when pull pacer is ready to give another credit to this connection.
    //TODO: need to credit in multiple of MTU here.

    if (_rtx_backlog > 0)
        _rtx_backlog = max ((mem_b)0, _rtx_backlog - EqdsSink::_credit_per_pull);

    if (EqdsSrc::_debug) cout << "RTX_backlog--: " << getSrc()->nodename() << " rtx_backlog " << rtx_backlog() << " at " << timeAsUs(getSrc()->eventlist().now()) << " flow " << _src->flow()->str() << endl;        

    _pull_no += EqdsSink::_credit_per_pull; 

    EqdsBasePacket* pkt = NULL;
    pkt = EqdsPullPacket::newpkt(_flow, *_route, _cumulative_ack, _pull_no,0,_srcaddr);
    return pkt;
}

bool EqdsSink::shouldSack(){
    return _bytes_unacked > _bytes_unacked_threshold;
}

EqdsBasePacket::seq_t EqdsSink::sackBitmapBase(EqdsBasePacket::seq_t epsn){
    return  max(epsn - 63,_cumulative_ack+1);
}

EqdsBasePacket::seq_t EqdsSink::sackBitmapBaseIdeal(){
    uint8_t lowest_value = UINT8_MAX;
    EqdsBasePacket::seq_t lowest_position;

    //find the lowest non-zero value in the sack bitmap; that is the candidate for the base, since it is the oldest packet that we are yet to sack.
    //on sack bitmap construction that covers a given seqno, the value is incremented. 
    for (EqdsBasePacket::seq_t crt = _cumulative_ack; crt <= _highest_received; crt++)
        if (_out_of_order[crt] && _out_of_order[crt]<lowest_value){
            lowest_value = _out_of_order[crt];
            lowest_position = crt;
        }
    
    if (lowest_position + 64 > _highest_received)
        lowest_position = _highest_received - 64;

    if (lowest_position <= _cumulative_ack)
        lowest_position = _cumulative_ack + 1;
    
    return lowest_position;
}

uint64_t EqdsSink::buildSackBitmap(EqdsBasePacket::seq_t ref_epsn){
    //take the next 64 entries from ref_epsn and create a SACK bitmap with them
    uint64_t bitmap = (_out_of_order[ref_epsn]!=0);

    for (int i=1;i<64;i++){
        bitmap = (bitmap << 1) & (_out_of_order[ref_epsn+i]!=0);

        if (_out_of_order[ref_epsn+i]){
            //remember that we sacked this packet
            if (_out_of_order[ref_epsn+i]<UINT8_MAX)
                _out_of_order[ref_epsn+i]++;
        }
    }
    return bitmap;
}

EqdsAckPacket *EqdsSink::sack(uint16_t path_id, EqdsBasePacket::seq_t seqno, bool ce) {
    uint64_t bitmap = buildSackBitmap(seqno);
    EqdsAckPacket* pkt = EqdsAckPacket::newpkt(_flow, *_route, _cumulative_ack,seqno,_pull_no,path_id,ce,_srcaddr);
    pkt->set_bitmap(bitmap);
    return pkt;
}

EqdsNackPacket *EqdsSink::nack(uint16_t path_id, EqdsBasePacket::seq_t seqno) {
    EqdsNackPacket* pkt = EqdsNackPacket::newpkt(_flow, *_route, seqno, _pull_no,path_id,_srcaddr);
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
    for (uint i = 0; i < eqdsMaxInFlightPkts; i++) {
        if (_out_of_order[i]) count++;
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
    EqdsBasePacket *pullPkt;

    if (!_rtx_senders.empty()){
        sink = _rtx_senders.front();
        _rtx_senders.pop_front();

        pullPkt = sink->pull();
        if (EqdsSrc::_debug) cout << "PullPacer: RTX: " << sink->getSrc()->nodename() << " rtx_backlog " << sink->rtx_backlog() << " at " << timeAsUs(eventlist().now()) << endl;
        // TODO if more pulls are needed, enqueue again
        if (sink->rtx_backlog()>0)
            _rtx_senders.push_back(sink);
        else if (sink->backlog() <= 0 && sink->backlog()+sink->getMaxCwnd() > 0) 
            //this sink has had its demand satisfied, move it to idle senders list.
            _idle_senders.push_back(sink);
    }
    else if (!_active_senders.empty()){
        sink = _active_senders.front();
        _active_senders.pop_front();
        pullPkt = sink->pull();

        // TODO if more pulls are needed, enqueue again
        if (EqdsSrc::_debug) cout << "PullPacer: Active: " << sink->getSrc()->nodename() << " backlog " << sink->backlog() << " at " << timeAsUs(eventlist().now()) << endl;
        if (sink->backlog()>0)
            _active_senders.push_back(sink);
        else //this sink has had its demand satisfied, move it to idle senders list.
            _idle_senders.push_back(sink);
    }
    else { //no active senders, we must have at least one idle sender
        sink = _idle_senders.front();
        _idle_senders.pop_front();
        if (EqdsSrc::_debug) cout << "PullPacer: Idle: " << sink->getSrc()->nodename() << " at " << timeAsUs(eventlist().now()) << endl;
        pullPkt = sink->pull();

        if (sink->backlog() + sink->getMaxCwnd() > 0)
            //only send upto 1BDP worth of speculative credit.
            //backlog will be negative once this source starts receiving speculative credit. 
            _idle_senders.push_back(sink);
    }

    pullPkt->flow().logTraffic(*pullPkt, *this, TrafficLogger::PKT_SEND);
    pullPkt->sendOn();

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
        if (EqdsSrc::_debug) cout << "BLA" << endl;
        return; 
    }

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
