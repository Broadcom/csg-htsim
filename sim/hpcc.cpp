// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#include <math.h>
#include <iostream>
#include <algorithm>
#include "hpcc.h"
#include "queue.h"
#include <stdio.h>
#include "switch.h"
#include "trigger.h"
using namespace std;

////////////////////////////////////////////////////////////////
//  HPCC SOURCE
////////////////////////////////////////////////////////////////

/* When you're debugging, sometimes it's useful to enable debugging on
   a single HPCC receiver, rather than on all of them.  Set this to the
   node ID and recompile if you need this; otherwise leave it
   alone. */
//#define LOGSINK 2332
#define LOGSINK   0 


/* keep track of RTOs.  Generally, we shouldn't see RTOs if
   return-to-sender is enabled.  Otherwise we'll see them with very
   large incasts. */
uint32_t HPCCSrc::_global_node_count = 0;

simtime_picosec HPCCSrc::_T = timeFromUs(12.0);//Known baseline RTT
double HPCCSrc::_eta = 0.95;//Target link utilization
uint32_t HPCCSrc::_max_stages = 5;//Maximum stages for additive increases
uint32_t HPCCSrc::_N = 10;//maximum number of flows.
uint32_t HPCCSrc::_Wai = 0;//Additive increase amount.

HPCCSrc::HPCCSrc(HPCCLogger* logger, TrafficLogger* pktlogger, EventList &eventlist, linkspeed_bps rate)
    : BaseQueue(rate,eventlist,NULL), _logger(logger), _flow(pktlogger)
{
    _mss = Packet::data_packet_size();
    _end_trigger = NULL;

    _stop_time = 0;
    _flow_started = false;
    _pacing_rate = rate;

    _acked_packets = 0;
    _packets_sent = 0;
    _new_packets_sent = 0;
    _rtx_packets_sent = 0;
    _acks_received = 0;
    _nacks_received = 0;

    _highest_sent = 0;
    _last_acked = 0;
    _dstaddr = UINT32_MAX;

    _sink = 0;
    _done = false;

    _drops = 0;
    _flow_size = ((uint64_t)1)<<63;
  
    _node_num = _global_node_count++;
    _nodename = "HPCCsrc " + to_string(_node_num);

    srand(time(NULL));
    _pathid = random()%256;

    _Wai = _mss;

    //cout << _nodename << " path id is " << _pathid << endl;

    // debugging hack
    _log_me = false;

    _state_send = READY;
    _time_last_sent = 0;
    
    _cwnd = _T * rate / pow(10,12) / 8;

    _pacing_rate = _cwnd * 8 * pow(10,12) / _T;
    update_spacing();

    _link_count = 0;
    _last_update_seq = 0;


    cout << "Initial CWND is " << _cwnd << " target RTT " << timeAsUs(_T) << " rate " << rate << endl;
    _flightsize = 0;
    _U = _eta;
}

/*mem_b HPCCSrc::queuesize(){
  return 0;
  }

  mem_b HPCCSrc::maxsize(){
  return 0;
  }*/

void HPCCSrc::set_traffic_logger(TrafficLogger* pktlogger) {
    _flow.set_logger(pktlogger);
}

void HPCCSrc::log_me() {
    // avoid looping
    if (_log_me == true)
        return;

    cout << "Enabling logging on HPCCSrc " << _nodename << endl;
    _log_me = true;
    if (_sink)
        _sink->log_me();
}

void HPCCSrc::startflow(){
    cout << "startflow " << _flow._name << " at " << timeAsUs(eventlist().now()) << endl;
    _flow_started = true;
    _highest_sent = 0;
    _last_acked = 0;
    
    _acked_packets = 0;
    _packets_sent = 0;
    _done = false;
    
    eventlist().sourceIsPendingRel(*this,0);
}

void HPCCSrc::set_end_trigger(Trigger& end_trigger) {
    _end_trigger = &end_trigger;
}

void HPCCSrc::connect(Route* routeout, Route* routeback, HPCCSink& sink, simtime_picosec starttime) {
    assert(routeout);
    _route = routeout;
    
    _sink = &sink;
    _flow.set_id(get_id()); // identify the packet flow with the HPCC source that generated it
    _flow._name = _name;
    _sink->connect(*this, routeback);

    if (starttime != TRIGGER_START) {
        //eventlist().sourceIsPending(*this,starttime);
        startflow();
    }
    else cout << "TRIGGER START " << _nodename << endl; 
}

/* PHPCCss a NACK.  Generally this involves queuing the NACKed packet
   for retransmission, but then waiting for a PULL to actually resend
   it.  However, sometimes the NACK has the PULL bit set, and then we
   resend immediately */
void HPCCSrc::processNack(const HPCCNack& nack){
    _last_acked = nack.ackno();
    _rtx_packets_sent += _highest_sent - _last_acked;

    cout << "HPCC " << _name << " go back n from " <<  _highest_sent << " to " << _last_acked << " at " << timeAsUs(eventlist().now()) << " us" << endl;

    if (_flow_size && _highest_sent>_flow_size && _last_acked < _flow_size){
        //restart the pacing of packets, this has stopped once we've passed the flow size but now a packet in the last window was lost.
        eventlist().sourceIsPendingRel(*this,0);
    }

    _highest_sent = _last_acked;
    _nacks_received ++;

    _cwnd = _mss;
    _flightsize = 0;

    //this packet be sent when it is time to send a new packet!
}

/* PHPCCss an ACK.  Mostly just housekeeping*/
void HPCCSrc::processAck(const HPCCAck& ack) {
    HPCCAck::seq_t ackno = ack.ackno();

    if (ackno > _last_acked) { // a brand new ack    
        assert(ackno - _last_acked <= _flightsize);
        _flightsize -= (ackno - _last_acked);
        _last_acked = ackno;
    }

    //HPCC magic here.
    /*
      21: Procedure NewAck(ack)
      22:    if ack.seq > lastUpdateSeq then
      23:        W = ComputeWind(MeasureInflight(ack), True);
      24:        lastUpdateSeq = snd_nxt;
      25:    else
      26:        W = ComputeWind(MeasureInflight(ack), False);
      27:    R = W/T; L = ack.L; 
    */   
    if (ackno > _last_update_seq){
        _cwnd = computeWind(measureInFlight(ack), true);
        cout << "CWND1 " << _cwnd << " ACKNO " << ackno << " at " << timeAsUs(eventlist().now()) << " src " << _nodename << endl;
        _last_update_seq = _highest_sent;
    }
    else {
        _cwnd = computeWind(measureInFlight(ack), false);
        cout << "CWND2 " << _cwnd << " ACKNO " << ackno << " at " << timeAsUs(eventlist().now()) << " src " << _nodename << endl;
    }

    _pacing_rate = _cwnd * 8 * pow(10,12) / _T;
    update_spacing();

    //cout << "INT Entries " << ack._int_hop << " qs " << ack._int_info[0]._queuesize << " TS " << timeAsUs(ack._int_info[0]._ts) << " TX " << ack._int_info[0]._txbytes << endl; 


    if (_logger) _logger->logHPCC(*this, HPCCLogger::HPCC_RCV);

    if (ackno >= _flow_size){
        cout << "Flow " << _name << " finished at " << timeAsUs(eventlist().now()) << " total bytes " << ackno << endl;
        _done = true;
        if (_end_trigger) {
            _end_trigger->activate();
        }

        return;
    }
}

void HPCCSrc::processPause(const EthPausePacket& p) {
    if (p.sleepTime()>0){
        //remote end is telling us to shut up.
        //cout << "Source " << str() << " PAUSE " << timeAsUs(eventlist().now()) << endl;
        //assert(_state_send != PAUSED);
        _state_send = PAUSED;
    }
    else {
        //we are allowed to send!
        //assert(_state_send != READY);
        _state_send = READY;
        //cout << "Source " << str() << " RESUME " << timeAsUs(eventlist().now()) << endl;
        eventlist().sourceIsPendingRel(*this,0);
    }
}

void HPCCSrc::receivePacket(Packet& pkt) 
{
    if (!_flow_started){
        assert(pkt.type()==ETH_PAUSE);
        return; 
    }

    if (_stop_time && eventlist().now() >= _stop_time) {
        // stop sending new data, but allow us to finish any retransmissions
        _flow_size = _highest_sent+_mss;
        _stop_time = 0;
    }

    if (_done)
        return;

    switch (pkt.type()) {
    case ETH_PAUSE:    
        processPause((const EthPausePacket&)pkt);
        pkt.free();
        return;
    case HPCCNACK: 
        _nacks_received++;
        processNack((const HPCCNack&)pkt);
        pkt.free();
        return;
    case HPCCACK:
        _acks_received++;
        processAck((const HPCCAck&)pkt);
        pkt.free();
        return;
    default:
        abort();
    }
}

// Note: the data sequence number is the number of Byte1 of the packet, not the last byte.
void HPCCSrc::send_packet() {
    HPCCPacket* p = NULL;
    bool last_packet = false;

    assert(_flow_started);

    if (_flow_size && (_last_acked >= _flow_size || _highest_sent > _flow_size))
        //flow is finished
        return;

    if (_flow_size && _highest_sent + _mss >= _flow_size) {
        last_packet = true;
        //cout << _name << " sending last packet with SEQNO " << _highest_sent+1 << " at " << timeAsUs(eventlist().now()) << endl;
    }
    
    p = HPCCPacket::newpkt(_flow, *_route, _highest_sent+1, _mss, false, last_packet,_dstaddr);
    
    assert(p);
    p->set_pathid(_pathid);

    p->flow().logTraffic(*p,*this,TrafficLogger::PKT_CREATESEND);
    p->set_ts(eventlist().now());
    
    if (_log_me) {
        cout << "Sent " << _highest_sent+1 << " Flow Size: " << _flow_size << endl;
    }
    _highest_sent += _mss;
    _packets_sent++;

    _flightsize += _mss;

    //cout << "Sent " << _highest_sent+1 << " Flow Size: " << _flow_size << " Flow " << _name << " time " << timeAsUs(eventlist().now()) << endl;

    p->sendOn();
}

void HPCCSrc::doNextEvent() {
    /*if (!_flow_started){
      startflow();
      return;
      }*/

    assert(_flow_started);

    if (_state_send==PAUSED)
        return;

    if (_flow_size && _highest_sent >= _flow_size) {
        //cout << _name << " stopping send coz highest_sent is " << _highest_sent << endl;
        return;
    }

    if (_time_last_sent==0 || eventlist().now() - _time_last_sent >= _packet_spacing){
        if (_cwnd >= _flightsize + _mss) 
            send_packet();

        _time_last_sent = eventlist().now();
    }

    simtime_picosec next_send = _time_last_sent + _packet_spacing;
    assert(next_send > eventlist().now());

    eventlist().sourceIsPending(*this, next_send);
}

double HPCCSrc::measureInFlight(const HPCCAck& ack){
    /*    Taken from https://datatracker.ietf.org/doc/draft-miao-tsv-hpcc/01/
          2:  u = 0;
          3:    for each link i on the path do
          4:                  ack.L[i].txBytes-L[i].txBytes
          txRate =  ----------------------------- ;
          ack.L[i].ts-L[i].ts
          5:               min(ack.L[i].qlen,L[i].qlen)      txRate
          u' = ----------------------------- +  ---------- ;
          ack.L[i].B*T                ack.L[i].B
          6:         if u' > u then
          7:             u = u'; tau = ack.L[i].ts -  L[i].ts;
          8:     tau = min(tau, T);
          9:     U = (1 - tau/T)*U + tau/T*u;
          10:    return U;*/

    double u = 0, uprime;
    uint32_t i;
    double txRate;
    simtime_picosec tau = _T;

    if (ack._int_hop == _link_count){
        for (i = 0;i<_link_count;i++){
            txRate = (ack._int_info[i]._txbytes - _link_info[i]._txbytes) * 8 * pow(10,12) / (ack._int_info[i]._ts - _link_info[i]._ts);

            uprime = min(ack._int_info[i]._queuesize, _link_info[i]._queuesize)*8 * pow (10,12) / ( (double)ack._int_info[i]._linkrate * _T ) + txRate / ack._int_info[i]._linkrate; 
            if (uprime > u) {
                u = uprime;
                tau = ack._int_info[i]._ts - _link_info[i]._ts;
            }
        }

        tau = min (tau, _T);

        _U = (1 - tau/_T)*_U + tau/_T*u;
    }
    else {    //reset path state
        _link_count = ack._int_hop;
        for (i = 0;i<_link_count;i++)
            _link_info[i] = ack._int_info[i];
    }

    return _U;
};

HPCCPacket::seq_t HPCCSrc::computeWind(double U, bool updateWc){
    /*
      11: Function ComputeWind(U, updateWc)
      12:    if U >= eta or incStage >= maxStagee then
      13:             Wc
      W = ----- + W_ai;
      U/eta
      14:        if updateWc then
      15:            incStagee = 0; Wc = W ;
      16:    else
      17:        W = Wc + W_ai ;
      18:        if updateWc then
      19:            incStage++; Wc = W ;
      20:    return W
    */
    HPCCPacket::seq_t W;
    
    if (_U >= _eta || _incStage >= _max_stages){
        W = _cwnd / (_U / _eta) + _Wai;

        if (updateWc){
            _incStage = 0;
            _cwnd = W;
        }
    }   
    else {
        W = _cwnd + _Wai;
        if (updateWc){
            _incStage++;
            _cwnd = W;
        }
    } 
    return W; 
};

////////////////////////////////////////////////////////////////
//  HPCC SINK
////////////////////////////////////////////////////////////////

/* Only use this constructor when there is only one for to this receiver */
HPCCSink::HPCCSink()
    : DataReceiver("HPCC_sink"),_cumulative_ack(0) , _total_received(0) 
{
    _src = 0;
    
    _nodename = "HPCCsink";
    _highest_seqno = 0;
    _log_me = false;
    _total_received = 0;
}

void HPCCSink::log_me() {
    // avoid looping
    if (_log_me == true)
        return;

    _log_me = true;

    if (_src)
        _src->log_me();  
}

/* Connect a src to this sink. */ 
void HPCCSink::connect(HPCCSrc& src, Route* route)
{
    _src = &src;
    _route = route;
    
    _cumulative_ack = 0;
    _drops = 0;
}


// Receive a packet.
// Note: _cumulative_ack is the last byte we've ACKed.
// seqno is the first byte of the new packet.
void HPCCSink::receivePacket(Packet& pkt) {
    /*
      if (random()%10==0){
      pkt.free();
      return;
      }*/

    switch (pkt.type()) {
    case HPCC:
        break;
    default:
        abort();
    }

    HPCCPacket *p = (HPCCPacket*)(&pkt);
    HPCCPacket::seq_t seqno = p->seqno();

    simtime_picosec ts = p->ts();
    //bool last_packet = ((HPCCPacket*)&pkt)->last_packet();

    if (seqno > _cumulative_ack+1){
        send_nack(ts,_cumulative_ack);      
    
        pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);

        p->free();

        //cout << "Wrong seqno received at HPCC SINK " << seqno << " expecting " << _cumulative_ack << endl;
        return;
    }

    int size = p->size()-HPCCPacket::ACKSIZE; 

    if (seqno == _cumulative_ack+1) { // it's the next expected seq no
        _cumulative_ack = seqno + size - 1;
    } else if (seqno < _cumulative_ack+1) {
        //must have been a bad retransmit
    }
    send_ack(ts,p->_int_info, p->_int_hop);
    // have we seen everything yet?
    pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);
    pkt.free();
}

void HPCCSink::send_ack(simtime_picosec ts, IntEntry* intinfo, uint32_t hops) {
    HPCCAck *ack = 0;
    ack = HPCCAck::newpkt(_src->_flow, *_route, _cumulative_ack,_srcaddr);
    ack->set_pathid(0);

    if (hops>0)
        ack->copy_int_info(intinfo,hops);

    ack->sendOn();
}

void HPCCSink::send_nack(simtime_picosec ts, HPCCPacket::seq_t ackno) {
    HPCCNack *nack = NULL;
    nack = HPCCNack::newpkt(_src->_flow, *_route, ackno,_srcaddr);

    nack->set_pathid(0);
    assert(nack);
    nack->flow().logTraffic(*nack,*this,TrafficLogger::PKT_CREATE);
    nack->set_ts(ts);
    nack->sendOn();
}




