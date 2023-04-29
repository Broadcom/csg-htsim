// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#include <math.h>
#include <iostream>
#include "ndptunnel.h"
#include "queue.h"
#include <stdio.h>

////////////////////////////////////////////////////////////////
//  NDP TUNNEL SOURCE
////////////////////////////////////////////////////////////////

/* When you're debugging, sometimes it's useful to enable debugging on
   a single NDP receiver, rather than on all of them.  Set this to the
   node ID and recompile if you need this; otherwise leave it
   alone. */
//#define LOGSINK 2332
#define LOGSINK 0

/* We experimented with adding extra puls to cope with scenarios
   where you've got a bad link and pulls get dropped.  Generally you
   don't want to do this though, so best leave RCV_CWND set to
   zero. Lost pulls are well handled by the cumulative pull number. */
//#define RCV_CWND 15
#define RCV_CWND 0

int NdpTunnelSrc::_global_node_count = 0;

/* keep track of RTOs.  Generally, we shouldn't see RTOs if
   return-to-sender is enabled.  Otherwise we'll see them with very
   large incasts. */
uint32_t NdpTunnelSrc::_global_rto_count = 0;

/* _min_rto can be tuned using SetMinRTO. Don't change it here.  */
simtime_picosec NdpTunnelSrc::_min_rto = timeFromUs((uint32_t)DEFAULT_RTO_MIN);

// You MUST set a route strategy.  The default is to abort without
// running - this is deliberate!
RouteStrategy NdpTunnelSrc::_route_strategy = NOT_SET;
RouteStrategy NdpTunnelSink::_route_strategy = NOT_SET;

NdpTunnelSrc::NdpTunnelSrc(NdpTunnelLogger* logger, TrafficLogger* pktlogger, EventList &eventlist)
    : EventSource(eventlist,"ndp"),  _logger(logger), _flow(pktlogger)
{
    _mss = Packet::data_packet_size();

    _base_rtt = timeInf;
    _acked_packets = 0;
    _packets_sent = 0;
    _new_packets_sent = 0;
    _rtx_packets_sent = 0;
    _acks_received = 0;
    _nacks_received = 0;
    _pulls_received = 0;
    _implicit_pulls = 0;
    _bounces_received = 0;

    _flight_size = 0;

    _qs = 0;
    _maxqs = 100 * _mss;
    
    _highest_sent = 0;
    _last_acked = 0;

    _sink = 0;

    _rtt = 0;
    _rto = timeFromMs(20);
    _cwnd = 15 * Packet::data_packet_size();
    _mdev = 0;
    _drops = 0;
    _flow_size = ((uint64_t)1)<<63;
    _last_pull = 0;
    _pull_window = 0;
  
    _crt_path = 0; // used for SCATTER_PERMUTE route strategy

    _rtx_timeout_pending = false;
    _rtx_timeout = timeInf;
    _node_num = _global_node_count++;
    _nodename = "ndptunnelsrc" + to_string(_node_num);

    // debugging hack
    _log_me = false;
}

void NdpTunnelSrc::set_traffic_logger(TrafficLogger* pktlogger) {
    _flow.set_logger(pktlogger);
}

void NdpTunnelSrc::log_me() {
    // avoid looping
    if (_log_me == true)
        return;
    cout << "Enabling logging on NdpTunnelSrc " << _nodename << endl;
    _log_me = true;
    if (_sink)
        _sink->log_me();
}

void NdpTunnelSrc::set_paths(vector<const Route*>* rt_list){
    size_t no_of_paths = rt_list->size();
    switch(_route_strategy) {
    case NOT_SET:
    case SINGLE_PATH:
        // shouldn't call this with these strategies
        abort();
    case SCATTER_PERMUTE:
    case SCATTER_RANDOM:
    case PULL_BASED:
        _paths.resize(no_of_paths);
        _original_paths.resize(no_of_paths);

        for (size_t i=0; i < no_of_paths; i++){
            Route* tmp = new Route(*(rt_list->at(i)));
            tmp->add_endpoints(this, _sink);
            tmp->set_path_id(i, rt_list->size());
            _paths[i] = tmp;
            _original_paths[i] = tmp;
        }

        _crt_path = 0;
        permute_paths();
        break;
    }
}

void NdpTunnelSrc::startflow(){
    _highest_sent = 0;
    _last_acked = 0;
    
    _acked_packets = 0;
    _packets_sent = 0;
    _rtx_timeout_pending = false;
    _rtx_timeout = timeInf;
    _pull_window = 0;
    
    _flight_size = 0;
    _first_window_count = 0;

    //    while (_flight_size < _cwnd && _flight_size < _flow_size) {
    //        send_packet(0);
    //        _first_window_count++;
    //}
}

void NdpTunnelSrc::connect(Route& routeout, Route& routeback, NdpTunnelSink& sink, simtime_picosec starttime) {
    _route = &routeout;
    assert(_route);
    
    _sink = &sink;
    _flow.set_id(get_id()); // identify the packet flow with the NDP source that generated it
    _flow._name = _name;
    _sink->connect(*this, routeback);
  
    //debugging hacks
    if (sink.get_id()==LOGSINK) {
        cout << "Found source for " << LOGSINK << "\n";
        _log_me = true;
    }
}

#define ABS(X) ((X)>0?(X):-(X))

/* Process a return-to-sender packet */
void NdpTunnelSrc::processRTS(NdpTunnelPacket& pkt){
    cout << "Got RTS packet"<<endl;
    assert(0);
}

/* Process a NACK.  Generally this involves queuing the NACKed packet
   for retransmission, but then waiting for a PULL to actually resend
   it.  However, sometimes the NACK has the PULL bit set, and then we
   resend immediately */

void NdpTunnelSrc::processNack(const NdpNack& nack){
    NdpTunnelPacket* p = NULL;
    
    //bool last_packet = (nack.ackno() + _mss - 1) >= _flow_size;
    _sent_times.erase(nack.ackno());

    //_flight_size -= _mss;
    //remove packet from _inflight
    list <NdpTunnelPacket*>::iterator rit;

    bool found = false;
    for (rit = _inflight.begin(); rit!=_inflight.end(); ++rit){
        p = (NdpTunnelPacket*)*rit;        
        if (p->seqno() == nack.ackno()){
            found = true;
            _inflight.erase(rit);
            break;
        }
    };

    assert(found);
    assert(_flight_size>=0);
    
    //p = NdpTunnelPacket::newpkt(_flow, *_route, nack.ackno(), 0, _mss, true,
    //                          _paths.size()>0?_paths.size():1, last_packet);
    
    // need to add packet to rtx queue
    //p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATE);

    p->load_state();
    _rtx_queue.push_back(p);
    if (nack.pull()) {
        _flight_size -= _mss;
        _implicit_pulls++;
        cout << "PULL NACK" << endl;
        pull_packets(nack.pullno(), nack.pacerno());
    }
}

/* Process an ACK.  Mostly just housekeeping, but if the ACK also has
   then PULL bit set, we also send a new packet immediately */
void NdpTunnelSrc::processAck(const NdpAck& ack) {
    NdpAck::seq_t ackno = ack.ackno();
    NdpAck::seq_t pacerno = ack.pacerno();
    NdpAck::seq_t pullno = ack.pullno();
    NdpAck::seq_t cum_ackno = ack.cumulative_ack();
    bool pull = ack.pull();
    if (pull) {
        if (_log_me)
            cout << "PULLACK\n";
        _pull_window--;
        _flight_size -= _mss;
    }
    simtime_picosec ts = ack.ts();
    //int32_t path_id = ack.path_id();

    /*
      if (pull)
      printf("Receive ACK (pull): %s\n", ack.pull_bitmap().to_string().c_str());
      else
      printf("Receive ACK (----): %s\n", ack.pull_bitmap().to_string().c_str());
    */
    log_rtt(_first_sent_times[ackno]);
    _first_sent_times.erase(ackno);
    _sent_times.erase(ackno);

    // Compute rtt.  This comes originally from TCP, and may not be optimal for NDP */
    uint64_t m = eventlist().now()-ts;

    if (m!=0){
        if (_rtt>0){
            uint64_t abs;
            if (m>_rtt)
                abs = m - _rtt;
            else
                abs = _rtt - m;

            _mdev = 3 * _mdev / 4 + abs/4;
            _rtt = 7*_rtt/8 + m/8;

            _rto = _rtt + 4*_mdev;
        } else {
            _rtt = m;
            _mdev = m/2;

            _rto = _rtt + 4*_mdev;
        }
        if (_base_rtt==timeInf || _base_rtt > m)
            _base_rtt = m;
    }

    if (_rto < _min_rto)
        _rto = _min_rto * ((drand() * 0.5) + 0.75);

    if (cum_ackno > _last_acked) { // a brand new ack    
        // we should probably cancel the rtx timer for any acked by
        // the cumulative ack, but we'll get an ACK or NACK anyway in
        // due course.
        _last_acked = cum_ackno;
    }
    if (_logger) _logger->logNdp(*this, NdpTunnelLogger::NDP_RCV);

    //_flight_size -= _mss;

    //remove packet from _inflight
    list<NdpTunnelPacket*>::iterator rit;

    bool found = false;
    NdpTunnelPacket* crt = NULL;
    for (rit = _inflight.begin(); rit!=_inflight.end(); ++rit){
        crt = (NdpTunnelPacket*)*rit;        
        if (crt->seqno() == ackno){
            found = true;
            _inflight.erase(rit);
            break;
        }
    };

    assert(found);
    crt->free();
    
    assert(_flight_size>=0);

    if (cum_ackno >= _flow_size){
        cout << "Flow " << nodename() << " finished at " << timeAsMs(eventlist().now()) << endl;
    }

    update_rtx_time();

    /* if the PULL bit is set, send some new data packets */
    if (pull) {
        _implicit_pulls++;
        //cout << "Recv PULLACK" <<endl;
        pull_packets(pullno, pacerno);
    }
}

void NdpTunnelSrc::receivePacket(Packet& pkt) 
{
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);

    switch (pkt.type()) {
    case NDP:
        {
            _bounces_received++;
            _first_window_count--;
            processRTS((NdpTunnelPacket&)pkt);
            return;
        }
    case NDPNACK: 
        {
            _nacks_received++;
            _pull_window++;
            _first_window_count--;
            /*if (_log_me) {
                printf("NACK, pw=%d\n", _pull_window);
            } else {
                printf("NACK\n");
            }*/
            processNack((const NdpNack&)pkt);
            pkt.free();
            return;
        } 
    case NDPPULL: 
        {
            _pulls_received++;
            _pull_window--;
            if (_log_me) {
                printf("PULL, pw=%d\n", _pull_window);
            }
            _flight_size -= _mss;
            assert(_flight_size >= 0);

            NdpPull *p = (NdpPull*)(&pkt);
            NdpPull::seq_t cum_ackno = p->cumulative_ack();
            if (cum_ackno > _last_acked) { // a brand new ack    
                // we should probably cancel the rtx timer for any acked by
                // the cumulative ack, but we'll get an ACK or NACK anyway in
                // due course.
                _last_acked = cum_ackno;
            }
            //cout << "Recv PULL" << endl;
            pull_packets(p->pullno(), p->pacerno());
            return;
        }
    case NDPACK:
        {
            _acks_received++;
            _pull_window++;
            _first_window_count--;
            //            if (_log_me) {
            //        printf("ACK, pw=%d\n", _pull_window);
            //}
            processAck((const NdpAck&)pkt);
            pkt.free();
            return;
        }
    case TCP:
    case TCPACK:
        {
            //must buffer packet in Queue
            bool empty = _queue.empty();
        
            if (pkt.size()+_qs > _maxqs){
                cout << "Dropping packet in EQDS queue" <<endl;
                pkt.free();
                return;
            }
            _queue.push_front(&pkt);
            _qs += pkt.size();

            //cout << "EQDS qs " << _qs << endl;

            //check to see if we can send any packets out
            if (empty)
                pull_packets(_last_pull + (_cwnd-_flight_size)/_mss,0);

            return;
        }
    default:
        assert(0);
    }
}

/* Choose a route for a particular packet */
const Route* NdpTunnelSrc::choose_route() {
    const Route * rt;
    switch(_route_strategy) {
    case SCATTER_RANDOM:
        //ECMP
        assert(_paths.size() > 0);
        _crt_path = random()%_paths.size();
        break;
    case SCATTER_PERMUTE:
        //Cycle through a permutation.  Generally gets better load balancing than SCATTER_RANDOM.
        _crt_path++;
        assert(_paths.size() > 0);
        if (_crt_path == _paths.size()) {
            permute_paths();
            _crt_path = 0;
        }
        break;
    case SINGLE_PATH:
        abort();  //not sure if this can ever happen - if it can, remove this line
        return _route;
    case PULL_BASED: // added this to shut to compiler warnings - add real code if we ever hit this abort
        abort();  
    case NOT_SET:
        abort();  // shouldn't be here at all
    }        

    rt = _paths.at(_crt_path);
    return rt;
}

void NdpTunnelSrc::pull_packets(NdpPull::seq_t pull_no, NdpPull::seq_t pacer_no) {
    // Pull number is cumulative both to allow for lost pulls and to
    // reduce reverse-path RTT - if one pull is delayed on one path, a
    // pull that gets there faster on another path can supercede it
  //    while (_last_pull < pull_no) {
  while(_flight_size<_cwnd){
    if (!send_packet(pacer_no))
      break;
    _last_pull++;
  }
}

// Note: the data sequence number is the number of Byte1 of the packet, not the last byte.
int NdpTunnelSrc::send_packet(NdpPull::seq_t pacer_no) {
    NdpTunnelPacket* p = NULL;
    if (!_rtx_queue.empty()) {
        // There are packets in the RTX queue for us to send

        p = _rtx_queue.front();
        _rtx_queue.pop_front();
        p->flow().logTraffic(*p,*this,TrafficLogger::PKT_SEND);
        p->set_ts(eventlist().now());
        p->set_pacerno(pacer_no);

        if (_route_strategy == SINGLE_PATH){
            p->set_route(*_route);
        } else {
            const Route *rt = choose_route();
            p->set_route(*rt);
        }

        _inflight.push_back(p);
        _flight_size += _mss;
        
        p->sendOn();
        
        _packets_sent ++;
        _rtx_packets_sent++;
        update_rtx_time();
        if (_rtx_timeout == timeInf) {
            _rtx_timeout = eventlist().now() + _rto;
        }
        return 1;
    }
    else if (!_queue.empty()){
        // there are no packets in the RTX queue, so we'll send a new one from the EQDS queue.
        Packet * pkt = _queue.back();
        _queue.pop_back();


        _qs -= pkt->size();
        //cout << "Sending packet into tunnel, qs " << _qs << " packet size " << pkt->size() << " flightsize" << _flight_size << endl;

      
        switch (_route_strategy) {
        case SCATTER_PERMUTE:
        case SCATTER_RANDOM:
        case PULL_BASED:
            {
                assert(_paths.size() > 0);
                const Route *rt = choose_route();
                p = NdpTunnelPacket::newpkt(_flow, *rt, _highest_sent+1, pacer_no, _mss, false,
                                            _paths.size()>0?_paths.size():1, false,pkt);
                break;
            }
        case SINGLE_PATH:
            p = NdpTunnelPacket::newpkt(_flow, *_route, _highest_sent+1, pacer_no,
                                        _mss, false, 1,
                                        false,pkt);
            break;
        case NOT_SET:
            abort();
        }
        assert(p);
        p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);
        p->set_ts(eventlist().now());
      
        _flight_size += _mss;
        _inflight.push_back(p);

        //refcount to remember we saved this locally.
        p->save_state();
        p->inc_ref_count();
        p->inner_packet()->inc_ref_count();
      
        _highest_sent += _mss;  //XX beware wrapping
        _packets_sent++;
        _new_packets_sent++;
      
        p->sendOn();
      
        if (_rtx_timeout == timeInf) {
            _rtx_timeout = eventlist().now() + _rto;
        }
        return 1;
    }
    else return 0;
}

void NdpTunnelSrc::permute_paths() {
    int len = _paths.size();
    for (int i = 0; i < len; i++) {
        int ix = random() % (len - i);
        const Route* tmppath = _paths[ix];
        _paths[ix] = _paths[len-1-i];
        _paths[len-1-i] = tmppath;
    }
}

void 
NdpTunnelSrc::update_rtx_time() {
    //simtime_picosec now = eventlist().now();
    if (_sent_times.empty()) {
        _rtx_timeout = timeInf;
        return;
    }
    map<NdpTunnelPacket::seq_t, simtime_picosec>::iterator i;
    simtime_picosec first_senttime = timeInf;
    int c = 0;
    for (i = _sent_times.begin(); i != _sent_times.end(); i++) {
        simtime_picosec sent = i->second;
        if (sent < first_senttime || first_senttime == timeInf) {
            first_senttime = sent;
        }
        c++;
    }
    _rtx_timeout = first_senttime + _rto;
}
 
void 
NdpTunnelSrc::process_cumulative_ack(NdpTunnelPacket::seq_t cum_ackno) {
    map<NdpTunnelPacket::seq_t, simtime_picosec>::iterator i, i_next;
    i = _sent_times.begin();
    while (i != _sent_times.end()) {
        if (i->first <= cum_ackno) {
            i_next = i; //juggling to keep i valid
            i_next++;
            _sent_times.erase(i);
            i = i_next;
        } else {
            return;
        }
    }
    //need to call update_rtx_time right after this!
}

void 
NdpTunnelSrc::retransmit_packet() {
  cout << "implement retransmit_packet\n" << endl;
  assert(0);
  
  /*    NdpTunnelPacket* p;
    map<NdpTunnelPacket::seq_t, simtime_picosec>::iterator i, i_next;
    i = _sent_times.begin();
    list <NdpTunnelPacket::seq_t> rtx_list;
    // we build a list first because otherwise we're adding to and
    // removing from _sent_times and the iterator gets confused
    while (i != _sent_times.end()) {
        if (i->second + _rto <= eventlist().now()) {
            //cout << "_sent_time: " << timeAsUs(i->second) << "us rto " << timeAsUs(_rto) << "us now " << timeAsUs(eventlist().now()) << "us\n";
            //this one is due for retransmission
            rtx_list.push_back(i->first);
            i_next = i; //we're about to invalidate i when we call erase
            i_next++;
            _sent_times.erase(i);
            i = i_next;
        } else {
            i++;
        }
    }
    list <NdpTunnelPacket::seq_t>::iterator j;
    for (j = rtx_list.begin(); j != rtx_list.end(); j++) {
        NdpTunnelPacket::seq_t seqno = *j;
        bool last_packet = (seqno + _mss - 1) >= _flow_size;
        switch (_route_strategy) {
        case SCATTER_PERMUTE:
        case SCATTER_RANDOM:
        case PULL_BASED:
        {
            assert(_paths.size() > 0);
            const Route* rt = _paths.at(_crt_path);
            p = NdpTunnelPacket::newpkt(_flow, *rt, seqno, 0, _mss, true,
                                  _paths.size(), last_packet);
            if (_route_strategy == SCATTER_RANDOM) {
                _crt_path = random() % _paths.size();
            } else {
                _crt_path++;
                if (_crt_path==_paths.size()){ 
                    permute_paths();
                    _crt_path = 0;
                }
            }
            break;
        }
        case SINGLE_PATH:
            p = NdpTunnelPacket::newpkt(_flow, *_route, seqno, 0, _mss, true,
                                  _paths.size(), last_packet);
            break;
        case NOT_SET:
            abort();
        }
        
        p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);
        p->set_ts(eventlist().now());
        _sent_times[seqno] = eventlist().now();
        //         if (_log_me) {
        //cout << "Sent " << seqno << " RTx" << " flow id " << p->flow().id << endl;
        //         }
        _global_rto_count++;
        cout << "Total RTOs: " << _global_rto_count << endl;
        p->sendOn();
        _packets_sent++;
        _rtx_packets_sent++;
    }
    update_rtx_time();*/
}

void NdpTunnelSrc::rtx_timer_hook(simtime_picosec now, simtime_picosec period) {
#ifndef RESEND_ON_TIMEOUT
    return;  // if we're using RTS, we shouldn't need to also use
             // timeouts, at least in simulation where we don't see
             // corrupted packets
#endif

    if (_highest_sent == 0) return;
    if (_rtx_timeout==timeInf || now + period < _rtx_timeout) return;

    cout <<"At " << timeAsUs(now) << "us RTO " << timeAsUs(_rto) << "us MDEV " << timeAsUs(_mdev) << "us RTT "<< timeAsUs(_rtt) << "us SEQ " << _last_acked / _mss << " CWND "<< _cwnd/_mss << " Flow ID " << str()  << endl;
    /*
    if (_log_me) {
        cout << "Flow " << LOGSINK << "scheduled for RTX\n";
    }
    */

    // here we can run into phase effects because the timer is checked
    // only periodically for ALL flows but if we keep the difference
    // between scanning time and real timeout time when restarting the
    // flows we should minimize them !
    if(!_rtx_timeout_pending) {
        _rtx_timeout_pending = true;

        
        // check the timer difference between the event and the real value
        simtime_picosec too_early = _rtx_timeout - now;
        if (now > _rtx_timeout) {
            // this shouldn't happen
            cout << "late_rtx_timeout: " << _rtx_timeout << " now: " << now << " now+rto: " << now + _rto << " rto: " << _rto << endl;
            too_early = 0;
        }
        eventlist().sourceIsPendingRel(*this, too_early);
    }
}

void NdpTunnelSrc::log_rtt(simtime_picosec sent_time) {
}

void NdpTunnelSrc::doNextEvent() {
    if (_rtx_timeout_pending) {
        _rtx_timeout_pending = false;
    
        if (_logger) _logger->logNdp(*this, NdpTunnelLogger::NDP_TIMEOUT);

        retransmit_packet();
    } else {
        cout << "Starting flow" << endl;
        startflow();
    }
}

////////////////////////////////////////////////////////////////
//  NDP SINK
////////////////////////////////////////////////////////////////

/* Only use this constructor when there is only one for to this receiver */
NdpTunnelSink::NdpTunnelSink(EventList& event, linkspeed_bps linkspeed, double pull_rate_modifier)
    : DataReceiver("ndp_sink"),_cumulative_ack(0) , _total_received(0) 
{
    _src = 0;
    _pacer = new NdpTunnelPullPacer(event, linkspeed, pull_rate_modifier);
    //_pacer = new NdpTunnelPullPacer(event, "/Users/localadmin/poli/new-datacenter-protocol/data/1500.recv.cdf.pretty");
    
    _nodename = "ndpsink";
    _pull_no = 0;
    _last_packet_seqno = 0;
    _log_me = false;
    _total_received = 0;
}

/* Use this constructor when there are multiple flows to one receiver
   - all the flows to one receiver need to share the same
   NdpTunnelPullPacer */
NdpTunnelSink::NdpTunnelSink(NdpTunnelPullPacer* pacer) : DataReceiver("ndp_sink"),_cumulative_ack(0) , _total_received(0) 
{
    _src = 0;
    _pacer = pacer;
    _nodename = "ndpsink";
    _pull_no = 0;
    _last_packet_seqno = 0;
    _log_me = false;
    _total_received = 0;
}

void NdpTunnelSink::log_me() {
    // avoid looping
    if (_log_me == true)
        return;

    _log_me = true;
    if (_src)
        _src->log_me();
    _pacer->log_me();
    
}

/* Connect a src to this sink.  We normally won't use this route if
   we're sending across multiple paths - call set_paths() after
   connect to configure the set of paths to be used. */
void NdpTunnelSink::connect(NdpTunnelSrc& src, Route& route)
{
    _src = &src;
    switch (_route_strategy) {
    case SINGLE_PATH:
        _route = &route;
        break;
    default:
        // do nothing we shouldn't be using this route - call
        // set_paths() to set routing information
        _route = NULL;
        break;
    }
        
    _cumulative_ack = 0;
    _drops = 0;

    // debugging hack
    if (get_id() == LOGSINK) {
        cout << "Found sink for " << LOGSINK << "\n";
        _log_me = true;
        _pacer->log_me();
    }
}

/* sets the set of paths to be used when sending from this NdpTunnelSink back to the NdpTunnelSrc */
void NdpTunnelSink::set_paths(vector<const Route*>* rt_list){
    switch (_route_strategy) {
    case SCATTER_PERMUTE:
    case SCATTER_RANDOM:
    case PULL_BASED:
        assert(_paths.size() == 0);
        _paths.resize(rt_list->size());
        for (unsigned int i=0;i<rt_list->size();i++){
            Route* t = new Route(*(rt_list->at(i)));
            t->add_endpoints(this, _src);
            _paths[i]=t;
        }
        _crt_path = 0;
        permute_paths();
        break;
    case SINGLE_PATH:
    case NOT_SET:
        abort();
    }
}

// Receive a packet.
// Note: _cumulative_ack is the last byte we've ACKed.
// seqno is the first byte of the new packet.
void NdpTunnelSink::receivePacket(Packet& pkt) {
    NdpTunnelPacket *p = (NdpTunnelPacket*)(&pkt);
    NdpTunnelPacket::seq_t seqno = p->seqno();
    NdpTunnelPacket::seq_t pacer_no = p->pacerno();
    simtime_picosec ts = p->ts();
    bool last_packet = ((NdpTunnelPacket*)&pkt)->last_packet();
    switch (pkt.type()) {
    case NDP:
        /*
          if (_log_me) {
          cout << "Recv " << seqno;
          if (pkt.header_only()) 
          cout << " (hdr)";
          else 
          cout << " (full)";
          cout << endl;
          }
        */
        break;
    case NDPACK:
    case NDPNACK:
    case NDPPULL:
        // Is there anything we should do here?  Generally won't happen unless the topolgy is very asymmetric.
        assert(pkt.bounced());
        cout << "Got bounced feedback packet!\n";
        p->free();
        return;
    default:
        abort();
    }
        
    if (pkt.header_only()){
        send_nack(ts,((NdpTunnelPacket*)&pkt)->seqno(), pacer_no);          
        pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);

        //not releasing this packet since it will be released by the sender once succesfully rexmitted.
        //p->free();
        return;
    }

    int size = p->size()-ACKSIZE; // TODO: the following code assumes all packets are the same size

    if (last_packet) {
        // we've seen the last packet of this flow, but may not have
        // seen all the preceding packets
        _last_packet_seqno = p->seqno() + size - 1;
    }

    _total_received+=size;

    if (seqno == _cumulative_ack+1) { // it's the next expected seq no
        //cout << "Normal Delivery new PKT " << seqno << " ACK " << _cumulative_ack << " flow " << p->flow().flow_id() << endl;

        _cumulative_ack = seqno + size - 1;

        Packet * inner = p->inner_packet();

        inner->sendOn();

        //not releasing this packet since it will be released by the sender once succesfully rexmitted.    
        //p->free();
  
        // are there any additional received packets we can now ack?
        while (!_received.empty() && (_received.front()->seqno() == _cumulative_ack+1) ) {
            NdpTunnelPacket * f = _received.front();

            //cout << "Out of order delivery new PKT " << seqno << " ACK " << _cumulative_ack << " flow " << p->flow().flow_id() << endl;          
            inner = f->inner_packet();
            f->free();

            _received.pop_front();

            inner->sendOn();
            //send this packet ON!
            _cumulative_ack+= size;
        }
    } else if (seqno < _cumulative_ack+1) {

        assert(0);

    } else { // it's not the next expected sequence number
        p->inc_ref_count();
        p->inner_packet()->inc_ref_count();
      
        if (_received.empty()) {
            _received.push_front(p);
        } else if (seqno > _received.back()->seqno()) { // likely case
            _received.push_back(p);
        } 
        else { // uncommon case - it fills a hole
            list<NdpTunnelPacket*>::iterator i;
            for (i = _received.begin(); i != _received.end(); i++) {
                if (seqno == (*i)->seqno()) break; // it's a bad retransmit
                if (seqno < (*i)->seqno()) {
                    _received.insert(i, p);
                    break;
                }
            }
        }
    }
    send_ack(ts, seqno, pacer_no);
    // have we seen everything yet?
    if (_last_packet_seqno > 0 && _cumulative_ack == _last_packet_seqno) {
        _pacer->release_pulls(flow_id());
    }
}

void NdpTunnelSink::send_ack(simtime_picosec ts, NdpTunnelPacket::seq_t ackno, NdpTunnelPacket::seq_t pacer_no) {
    NdpAck *ack = NULL;
    _pull_no++;
    
    switch (_route_strategy) {
    case SCATTER_PERMUTE:
    case SCATTER_RANDOM:
    case PULL_BASED:
        assert(_paths.size() > 0);
        ack = NdpAck::newpkt(_src->_flow, *(_paths.at(_crt_path)), 0, ackno, 
                             _cumulative_ack, _pull_no, 
                             0);
        if (_route_strategy == SCATTER_RANDOM) {
            _crt_path = random()%_paths.size();
        } else {
            _crt_path++;
            if (_crt_path == _paths.size()) {
                permute_paths();
                _crt_path = 0;
            }
        }
        break;
    case SINGLE_PATH:        
        ack = NdpAck::newpkt(_src->_flow, *_route, 0, ackno, _cumulative_ack,
                             _pull_no, 0);
        break;
    case NOT_SET:
        abort();
    }

    ack->flow().logTraffic(*ack,*this,TrafficLogger::PKT_CREATE);
    ack->set_ts(ts);

    _pacer->sendPacket(ack, pacer_no, this);
}

void NdpTunnelSink::send_nack(simtime_picosec ts, NdpTunnelPacket::seq_t ackno, NdpTunnelPacket::seq_t pacer_no) {
    NdpNack *nack = NULL;
    _pull_no++;
    switch (_route_strategy) {
    case SCATTER_PERMUTE:
    case SCATTER_RANDOM:
    case PULL_BASED:
        assert(_paths.size() > 0);
        nack = NdpNack::newpkt(_src->_flow, *(_paths.at(_crt_path)), 0, ackno, 
                               _cumulative_ack, _pull_no,
                               0);
        if (_route_strategy == SCATTER_RANDOM) {
            _crt_path = random()%_paths.size();
        } else {
            _crt_path++;
            if (_crt_path == _paths.size()) {
                permute_paths();
                _crt_path = 0;
            }
        }
        break;
    case SINGLE_PATH:
      nack = NdpNack::newpkt(_src->_flow, *_route, 0, ackno, _cumulative_ack,  _pull_no, 0 );
      break;
    case NOT_SET:
      abort();
    }
    nack->flow().logTraffic(*nack,*this,TrafficLogger::PKT_CREATE);
    nack->set_ts(ts);
    _pacer->sendPacket(nack, pacer_no, this);
}


void NdpTunnelSink::permute_paths() {
    int len = _paths.size();
    for (int i = 0; i < len; i++) {
        int ix = random() % (len - i);
        const Route* tmppath = _paths[ix];
        _paths[ix] = _paths[len-1-i];
        _paths[len-1-i] = tmppath;
    }
}


double* NdpTunnelPullPacer::_pull_spacing_cdf = NULL;
int NdpTunnelPullPacer::_pull_spacing_cdf_count = 0;

/* Every NdpTunnelSink needs an NdpTunnelPullPacer to pace out it's PULL packets.
   Multiple incoming flows at the same receiving node much share a
   single pacer */
NdpTunnelPullPacer::NdpTunnelPullPacer(EventList& event, linkspeed_bps linkspeed, double pull_rate_modifier)  : 
    EventSource(event, "ndp_pacer"), _last_pull(0)
{
    _packet_drain_time = (simtime_picosec)((Packet::data_packet_size()+ACKSIZE) * (pow(10.0,12.0) * 8) / linkspeed) / pull_rate_modifier;
    cout << "Packet drain time " << timeAsUs(_packet_drain_time) << endl;
    _log_me = false;
    _pacer_no = 0;
}

NdpTunnelPullPacer::NdpTunnelPullPacer(EventList& event, char* filename)  : 
    EventSource(event, "ndp_pacer"), _last_pull(0)
{
    int t;
    _packet_drain_time = 0;

    if (!_pull_spacing_cdf){
        FILE* f = fopen(filename,"r");
        if (fscanf(f,"%d\n",&_pull_spacing_cdf_count) != 1) {
            cerr << "Parse error, failed to read CDF count\n";
            exit(1);
        }
        cout << "Generating pull spacing from CDF; reading " << _pull_spacing_cdf_count << " entries from CDF file " << filename << endl;
        _pull_spacing_cdf = new double[_pull_spacing_cdf_count];

        for (int i=0;i<_pull_spacing_cdf_count;i++){
            if (fscanf(f,"%d %lf\n",&t,&_pull_spacing_cdf[i]) != 2) {
                cerr << "Parse error reading CDF value\n";
                exit(1);
            }
            //assert(t==i);
            //cout << " Pos " << i << " " << _pull_spacing_cdf[i]<<endl;
        }
    }
    
    _log_me = false;
    _pacer_no = 0;
}

void NdpTunnelPullPacer::log_me() {
    // avoid looping
    if (_log_me == true)
        return;

    _log_me = true;
    _total_excess = 0;
    _excess_count = 0;
}

void NdpTunnelPullPacer::set_pacerno(Packet *pkt, NdpPull::seq_t pacer_no) {
    if (pkt->type() == NDPACK) {
        ((NdpAck*)pkt)->set_pacerno(pacer_no);
    } else if (pkt->type() == NDPNACK) {
        ((NdpNack*)pkt)->set_pacerno(pacer_no);
    } else if (pkt->type() == NDPPULL) {
        ((NdpPull*)pkt)->set_pacerno(pacer_no);
    } else {
        abort();
    }
}

void NdpTunnelPullPacer::sendPacket(Packet* ack, NdpTunnelPacket::seq_t rcvd_pacer_no, NdpTunnelSink* receiver) {
  /*
    if (_log_me) {
    cout << "pacerno diff: " << _pacer_no - rcvd_pacer_no << endl;
    }
  */
  
  /*if (rcvd_pacer_no != 0 && _pacer_no - rcvd_pacer_no < RCV_CWND) {
  // we need to increase the number of packets in flight from this flow
  if (_log_me)
  cout << "increase_window\n";
  receiver->increase_window();
  }*/

    simtime_picosec drain_time;
    
    if (_packet_drain_time>0)
        drain_time = _packet_drain_time;
    else {
        int t = (int)(drand()*_pull_spacing_cdf_count);
        drain_time = 10*timeFromNs(_pull_spacing_cdf[t])/20;
        //cout << "Drain time is " << timeAsUs(drain_time);
    }
            

    if (_pull_queue.empty()){
        simtime_picosec delta = eventlist().now()-_last_pull;
    
        if (delta >= drain_time){
            //send out as long as last NACK/ACK was sent more than packetDrain time ago.
            ack->flow().logTraffic(*ack,*this,TrafficLogger::PKT_SEND);
            set_pacerno(ack, _pacer_no++);
            ack->sendOn();
            _last_pull = eventlist().now();
            return;
        } else {
            eventlist().sourceIsPendingRel(*this,drain_time - delta);
        }
    }

    if (_log_me) {
        _excess_count++;
        cout << "Mean excess: " << _total_excess / _excess_count << endl;
        if (ack->type() == NDPACK) {
            cout << "Ack " <<  (((NdpAck*)ack)->ackno()-1)/9000 << " (queue)\n";
        } else if (ack->type() == NDPNACK) {
            cout << "Nack " << (((NdpNack*)ack)->ackno()-1)/9000 << " (queue)\n";
        } else {
            cout << "WTF\n";
        }
    }

    //Create a pull packet and stick it in the queue.
    //Send on the ack/nack, but with pull cleared.
    NdpPull *pull_pkt = NULL;
    if (ack->type() == NDPACK) {
        pull_pkt = NdpPull::newpkt((NdpAck*)ack);
        ((NdpAck*)ack)->dont_pull();
    } else if (ack->type() == NDPNACK) {
        pull_pkt = NdpPull::newpkt((NdpNack*)ack);
        ((NdpNack*)ack)->dont_pull();
    }
    pull_pkt->flow().logTraffic(*pull_pkt,*this,TrafficLogger::PKT_CREATE);

    _pull_queue.enqueue(*pull_pkt, receiver->priority());

    ack->flow().logTraffic(*ack,*this,TrafficLogger::PKT_SEND);
    ack->sendOn();
    
    //   if (_log_me) {
    //       list <Packet*>::iterator i = _waiting_pulls.begin();
    //       cout << "Queue: ";
    //       while (i != _waiting_pulls.end()) {
    //           Packet* p = *i;
    //           if (p->type() == NDPNACK) {
    //               cout << "Nack(" << ((NdpNack*)p)->ackno() << ") ";
    //           } else if (p->type() == NDPACK) {
    //               cout << "Ack(" << ((NdpAck*)p)->ackno() << ") ";
    //           } 
    //           i++;
    //       }
    //       cout << endl;
    //   }
    //cout << "Qsize = " << _waiting_pulls.size() << endl;
}


// when we're reached the last packet of a connection, we can release
// all the queued acks for that connection because we know they won't
// generate any more data packets.  This will move the nacks up the
// queue too, causing any retransmitted packets from the tail of the
// file to be received earlier
void NdpTunnelPullPacer::release_pulls(uint32_t flow_id) {
    _pull_queue.flush_flow(flow_id);
}

void NdpTunnelPullPacer::doNextEvent(){
    if (_pull_queue.empty()) {
        // this can happen if we released all the acks at the end of
        // the connection.  we didn't cancel the timer, so we end up
        // here.
        return;
    }
    Packet *pkt = _pull_queue.dequeue();
    //cout << "Sending PULL for flow " << pkt->flow().flow_id() << endl;
    

    //   cout << "Sending NACK for packet " << nack->ackno() << endl;
    pkt->flow().logTraffic(*pkt,*this,TrafficLogger::PKT_SEND);
    if (pkt->flow().log_me()) {
        if (pkt->type() == NDPACK) {
            abort(); //we now only pace pulls
        } else if (pkt->type() == NDPNACK) {
            abort(); //we now only pace pulls
        } if (pkt->type() == NDPPULL) {
            //cout << "Pull (queued) " << ((NdpNack*)pkt)->ackno() << "\n";
        } else {
            abort(); //we now only pace pulls
        }
    }
    set_pacerno(pkt, _pacer_no++);
    pkt->sendOn();   

    _last_pull = eventlist().now();
   
    simtime_picosec drain_time;

    if (_packet_drain_time>0)
        drain_time = _packet_drain_time;
    else {
        int t = (int)(drand()*_pull_spacing_cdf_count);
        drain_time = 10*timeFromNs(_pull_spacing_cdf[t])/20;
        //cout << "Drain time is " << timeAsUs(drain_time);
    }

    if (!_pull_queue.empty()){
      eventlist().sourceIsPendingRel(*this,drain_time);//*(0.5+drand()));
    }
    else {
        //    cout << "Empty pacer queue at " << timeAsMs(eventlist().now()) << endl; 
    }
}


////////////////////////////////////////////////////////////////
//  NDP TUNNEL RETRANSMISSION TIMER
////////////////////////////////////////////////////////////////

NdpTunnelRtxTimerScanner::NdpTunnelRtxTimerScanner(simtime_picosec scanPeriod, EventList& eventlist)
  : EventSource(eventlist,"RtxScanner"), 
    _scanPeriod(scanPeriod)
{
    eventlist.sourceIsPendingRel(*this, 0);
}

void 
NdpTunnelRtxTimerScanner::registerNdp(NdpTunnelSrc &tcpsrc)
{
    _tcps.push_back(&tcpsrc);
}

void
NdpTunnelRtxTimerScanner::doNextEvent() 
{
    simtime_picosec now = eventlist().now();
    tcps_t::iterator i;
    for (i = _tcps.begin(); i!=_tcps.end(); i++) {
        (*i)->rtx_timer_hook(now,_scanPeriod);
    }
    eventlist().sourceIsPendingRel(*this, _scanPeriod);
}
