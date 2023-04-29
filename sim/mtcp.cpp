// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#include "mtcp.h"
#include <math.h>
#include <iostream>
////////////////////////////////////////////////////////////////
//  Multipath TCP SOURCE
////////////////////////////////////////////////////////////////

#define ABS(x) ((x)>=0?(x):-(x))
#define A_SCALE 512

MultipathTcpSrc::MultipathTcpSrc(char cc_type,EventList& ev,MultipathTcpLogger* logger, int rwnd):
    EventSource(ev,"MTCP"),_alfa(1),_logger(logger), _e(1)
{
    _cc_type = cc_type;
    a = A_SCALE;
    _sink = NULL;

#ifdef MODEL_RECEIVE_WINDOW
    _highest_sent = 0;
    _last_acked = 0;

    _receive_window = rwnd * 1000;

    for (int i=0;i<100000;i++)
        for (int j=0;j<4;j++)
            _packets_mapped[i][j] = 0;
    for (int j=0;j<4;j++)
        _last_reduce[j] = 0;
#endif
    eventlist().sourceIsPending(*this,timeFromSec(3));
    _nodename = "mtcpsrc";
}

void MultipathTcpSrc::addSubflow(TcpSrc* subflow){
    _subflows.push_back(subflow);
    subflow->_subflow_id = _subflows.size()-1;
    subflow->joinMultipathConnection(this);
}

void
MultipathTcpSrc::receivePacket(Packet& pkt) {
#ifdef MODEL_RECEIVE_WINDOW
    //must perform ack processing if MPTCP RECEIVE_WINDOW is being modelled
    TcpAck *p = (TcpAck*)(&pkt);
    TcpAck::seq_t seqno = p->data_ackno();

    //  cout << "Data SEQ: " << seqno << " at " << timeAsMs(eventlist().now()) <<endl;

    if (seqno<=_last_acked)
        return;

    //there is a chance that we can send packets on all subflows - we should try them out.
    _last_acked = seqno;

    if (_last_acked==_highest_sent){
        //create inactivity timers?
    }
#endif
}

#ifdef MODEL_RECEIVE_WINDOW
int MultipathTcpSrc::getDataSeq(uint64_t* seq, TcpSrc* subflow){
    //if cwnd is small (high chance of RTO) and there is unacked data SEQ, give it that.
    //if there is a possibility of stall (know rwnd) then give it redundant data.

    if (_last_acked+_receive_window > _highest_sent){
        *seq = _highest_sent+1;
        int pos = ((_highest_sent+1)/1000)%100000;

        _highest_sent += 1000;

        for (int j=0;j<4;j++)
            _packets_mapped[pos][j] = 0;

        _packets_mapped[pos][subflow->_subflow_id] = 1;

        return 1;
    } else {
        //receive window blocked
        uint64_t packet = _last_acked+1;
        int pos = (packet/1000)%100000;

        //    cout << "BLK" << endl;

        //shall we stall the subflow at the trailling edge of the window?
#ifdef STALL_SLOW_SUBFLOWS
        int slow_subflow_id = -1;

        for (int j=0;j<4;j++) {
            if (_packets_mapped[pos][j]) {
                if (slow_subflow_id < 0)
                    slow_subflow_id = j;
                else 
                    slow_subflow_id = 4;
            }
        }

        if (slow_subflow_id>=0 && slow_subflow_id<4 && slow_subflow_id!=subflow->_subflow_id){

            TcpSrc* src = NULL;
            list<TcpSrc*>::iterator i = _subflows.begin();
            std::advance(i, slow_subflow_id);
      
            src = *i;
      
            //stall

            if ((src->effective_window()/timeAsMs(src->_rtt))<subflow->effective_window()/timeAsMs(subflow->_rtt) &&
                eventlist().now()-_last_reduce[slow_subflow_id] > src->_rtt){
                src->_ssthresh = deflate_window(src->_cwnd,src->_mss);
                src->_cwnd = src->_ssthresh;
        
                _last_reduce[slow_subflow_id] = eventlist().now();
                //        cout << "PEN"<<endl;
            }
        }
#endif
        //we should retransmit the oldest seqno which hasn't been sent on this subflow
#ifdef REXMIT_ENABLED
        while (packet<_highest_sent) {
            pos = (packet/1000)%100000;
      
            //if (subflow->_highest_data_seq<packet || !subflow->_sent_packets.has_data_seq(packet)){

            /*      TcpSrc* src = NULL;
                    list<TcpSrc*>::iterator i = _subflows.begin();
                    std::advance(i, slow_subflow_id);
    
                    src = *i;*/
      
            if (!_packets_mapped[pos][subflow->_subflow_id]){
                //                    && 4 * subflow->_rtt < src->_rtt){
                *seq = packet;
                _packets_mapped[pos][subflow->_subflow_id] = 1;

                //        cout << "RX"<<endl;
                return 1;
            }
            packet += 1000;
            break;
        }
#endif

        cout << "Fail Data Seq" << endl;
        return 0;
    }
}
#endif

uint32_t
MultipathTcpSrc::inflate_window(uint32_t cwnd, int newly_acked,uint32_t mss) {
    int tcp_inc = (newly_acked * mss)/cwnd;
    int tt = (newly_acked * mss) % cwnd;
    int64_t tmp, total_cwnd, tmp2,subs;
    double tmp_float;

    if (tcp_inc==0)
        return cwnd;

    switch(_cc_type) {
    case UNCOUPLED:
        if ((cwnd + tcp_inc)/mss != cwnd/mss){
            if (_logger){
                _logger->logMultipathTcp(*this,MultipathTcpLogger::WINDOW_UPDATE);
                _logger->logMultipathTcp(*this,MultipathTcpLogger::RTT_UPDATE);
            }
        }
        return cwnd + tcp_inc;

    case COUPLED_SCALABLE_TCP:
        return cwnd + newly_acked*0.01;

    case FULLY_COUPLED:
        total_cwnd = compute_total_window();
        tt = (int)(newly_acked * mss * A);
        tmp = tt/total_cwnd;
        if (tmp>tcp_inc)
            tmp = tcp_inc;

        //if ((rand()%total_cwnd) < tt % total_cwnd)
        //tmp ++;

        if ((cwnd + tmp)/mss != cwnd/mss){
            if (_logger){
                _logger->logMultipathTcp(*this,MultipathTcpLogger::WINDOW_UPDATE);
                _logger->logMultipathTcp(*this,MultipathTcpLogger::RTT_UPDATE);
            }
        }

        return cwnd + tmp;

    case COUPLED_INC:
        //use double!
        total_cwnd = compute_total_window();
        tmp_float =((double)newly_acked * mss * a) / total_cwnd / A_SCALE; 

        tmp2 = (newly_acked * mss * a) / total_cwnd;
        tmp = tmp2 / A_SCALE;

        if (tmp < 0){
            printf("Negative increase!");
            tmp = 0;
        }

        if (rand() % A_SCALE < tmp2 % A_SCALE){
            tmp++;
            tmp_float++;
        }
    
        if (tmp>tcp_inc)//capping
            tmp = tcp_inc;

        if (tmp_float>tcp_inc)//capping
            tmp_float = tcp_inc;
    
        //    cout << timeAsMs(eventlist().now()) << " ID " << id << " Tmp "<< tmp << " tmp2 " << tmp2 << " tt " << tmp_float << " tcp inc " << tcp_inc << " alfa " << a << endl;


        if ((cwnd + tmp_float)/mss != cwnd/mss){
            a = compute_a_scaled();
            if (_logger){
                _logger->logMultipathTcp(*this,MultipathTcpLogger::WINDOW_UPDATE);
                _logger->logMultipathTcp(*this,MultipathTcpLogger::RTT_UPDATE);
                _logger->logMultipathTcp(*this,MultipathTcpLogger::CHANGE_A);
            }
        }

        return cwnd + tmp_float;

    case COUPLED_TCP:
        //need to update this to the number of subflows (wtf man!)
        subs = _subflows.size();
        subs *= subs;
        tmp = tcp_inc/subs;
        if (tcp_inc%subs>=subs/2)
            tmp++;

        if ((cwnd + tmp)/mss != cwnd/mss){
            //a = compute_a_tcp();
            if (_logger){
                _logger->logMultipathTcp(*this,MultipathTcpLogger::WINDOW_UPDATE);
                _logger->logMultipathTcp(*this,MultipathTcpLogger::RTT_UPDATE);
                //_logger->logMultipathTcp(*this,MultipathTcpLogger::CHANGE_A);
            }
        }
        return cwnd + tmp;

    case COUPLED_EPSILON:
        total_cwnd = compute_total_window();
        tmp_float = ((double)newly_acked * mss * _alfa * pow(_alfa*cwnd,1-_e))/pow(total_cwnd,2-_e);

        tmp = (int)floor(tmp_float);
        if (drand()< tmp_float-tmp)
            tmp++;

        if (tmp>tcp_inc)//capping
            tmp = tcp_inc;

        if ((cwnd + tmp)/mss != cwnd/mss){
            if (_e>0&&_e<2)
                _alfa = compute_alfa();

            if (_logger){
                _logger->logMultipathTcp(*this,MultipathTcpLogger::WINDOW_UPDATE);
                _logger->logMultipathTcp(*this,MultipathTcpLogger::RTT_UPDATE);
                _logger->logMultipathTcp(*this,MultipathTcpLogger::CHANGE_A);
            }
        }
        return cwnd + tmp;
    

    default:
        printf("Unknown cc type %d\n",_cc_type);
        exit(1);
    }
}

void MultipathTcpSrc::window_changed(){

    switch(_cc_type){
    case COUPLED_EPSILON:
        if (_e>0&&_e<2)
            _alfa = compute_alfa();
    
        if (_logger){
            _logger->logMultipathTcp(*this,MultipathTcpLogger::WINDOW_UPDATE);
            _logger->logMultipathTcp(*this,MultipathTcpLogger::RTT_UPDATE);
            _logger->logMultipathTcp(*this,MultipathTcpLogger::CHANGE_A);
        }
        return;
    case COUPLED_INC:
        a = compute_a_scaled();
        if (_logger){
            _logger->logMultipathTcp(*this,MultipathTcpLogger::WINDOW_UPDATE);
            _logger->logMultipathTcp(*this,MultipathTcpLogger::RTT_UPDATE);
            _logger->logMultipathTcp(*this,MultipathTcpLogger::CHANGE_A);
        }
        return;
    }
}

uint32_t 
MultipathTcpSrc::deflate_window(uint32_t cwnd, uint32_t mss){
    int d;
    uint32_t decrease_tcp = max(cwnd/2,(uint32_t)(mss));
    switch(_cc_type){
    case UNCOUPLED:
    case COUPLED_INC:
    case COUPLED_TCP:
    case COUPLED_EPSILON:
        return decrease_tcp;

    case COUPLED_SCALABLE_TCP:
        d = (int)cwnd - (compute_total_window()>>3);
        if (d<0) d = 0;

        //cout << "Dropping to " << max(mss,(uint32_t)d) << endl;
        return max(mss, (uint32_t)d);

    case FULLY_COUPLED:
        d = (int)cwnd - compute_total_window()/B;
        if (d<0) d = 0;

        //cout << "Dropping to " << max(mss,(uint32_t)d) << endl;
        return max(mss, (uint32_t)d);

    default:
        printf("Unknown cc type %d\n",_cc_type);
        exit(1);    
    }
}

uint32_t
MultipathTcpSrc::compute_total_window(){
    list<TcpSrc*>::iterator it;
    uint32_t crt_wnd = 0;

    //  printf ("Tot wnd ");
    for (it = _subflows.begin();it!=_subflows.end();it++){
        TcpSrc& crt = *(*it);
        crt_wnd += crt._in_fast_recovery?crt._ssthresh:crt._cwnd;
    }

    return crt_wnd;
}

uint64_t
MultipathTcpSrc::compute_total_bytes(){
    list<TcpSrc*>::iterator it;
    uint64_t b = 0;

    for (it = _subflows.begin();it!=_subflows.end();it++){
        TcpSrc& crt = *(*it);
        b += crt._last_acked;
    }

    return b;
}


uint32_t
MultipathTcpSrc::compute_a_tcp(){
    if (_cc_type!=COUPLED_TCP)
        return 0;

    if (_subflows.size()!=2){
        cout << "Expecting 2 subflows, found" << _subflows.size() << endl;
        exit(1);
    }

    TcpSrc* flow0 = _subflows.front();
    TcpSrc* flow1 = _subflows.back();

    uint32_t cwnd0 = flow0->_in_fast_recovery?flow0->_ssthresh:flow0->_cwnd;
    uint32_t cwnd1 = flow1->_in_fast_recovery?flow1->_ssthresh:flow1->_cwnd;

    double pdelta = (double)cwnd1/cwnd0;
    double rdelta;

#if USE_AVG_RTT
    if (flow0->_rtt_avg>timeFromMs(0)&&flow1->_rtt_avg>timeFromMs(1))
        rdelta = (double)flow0->_rtt_avg / flow1->_rtt_avg;
    else
        rdelta = 1;

#else
    rdelta = (double)flow0->_rtt / flow1->_rtt;

#endif


    if (1 < pdelta * rdelta * rdelta){
#if USE_AVG_RTT
        if (flow0->_rtt_avg>timeFromMs(0)&&flow1->_rtt_avg>timeFromMs(1))
            rdelta = (double)flow1->_rtt_avg / flow0->_rtt_avg;
        else
            rdelta = 1;
#else
        rdelta = (double)flow1->_rtt / flow0->_rtt;
#endif

        pdelta = (double)cwnd0/cwnd1;
    }

    double t = 1.0+rdelta*sqrt(pdelta); 

    return (uint32_t)(A_SCALE/t/t);
}

uint32_t MultipathTcpSrc::compute_a_scaled(){
    if (_cc_type!=COUPLED_INC && _cc_type!=COUPLED_TCP)
        return 0;

    uint32_t sum_denominator=0;
    uint64_t t = 0;
    uint64_t cwndSum = 0;
  
    list<TcpSrc*>::iterator it;
    for(it=_subflows.begin();it!=_subflows.end();++it) {
        TcpSrc& flow = *(*it);
        uint32_t cwnd = flow._in_fast_recovery?flow._ssthresh:flow._cwnd;          
        uint32_t rtt = timeAsUs(flow._rtt)/10;
        if(rtt==0) rtt=1;
    
        t = max(t,(uint64_t)cwnd * flow._mss * flow._mss / rtt / rtt);
        sum_denominator += cwnd * flow._mss / rtt;
        cwndSum += cwnd;
    }

    uint32_t alpha = (uint32_t)( A_SCALE * (uint64_t)cwndSum * t / sum_denominator / sum_denominator);
  
    if (alpha==0){
        cout << "alpha is 0 and t is "<<t <<" cwndSum "<<cwndSum<<endl;
        alpha = A_SCALE;
    }

    return alpha;
}


double MultipathTcpSrc::compute_alfa(){
    if (_cc_type!=COUPLED_EPSILON)
        return 0;

    if (_subflows.size()==1){
        return 1;
    }
    else {
        double maxt = 0,sum_denominator = 0;

        list<TcpSrc*>::iterator it;
        for(it=_subflows.begin();it!=_subflows.end();++it) {
            TcpSrc& flow = *(*it);
            uint32_t cwnd = flow._in_fast_recovery?flow._ssthresh:flow._cwnd;
            uint32_t rtt = timeAsMs(flow._rtt);
      
            if (rtt==0)
                rtt = 1;

            double t = pow(cwnd,_e/2)/rtt;
            if (t>maxt)
                maxt = t;

            sum_denominator += ((double)cwnd/rtt);
        }

        return (double)compute_total_window() * pow(maxt, 1/(1-_e/2)) / pow(sum_denominator, 1/(1-_e/2));
    }
}


double
MultipathTcpSrc::compute_a(){
    if (_cc_type!=COUPLED_INC && _cc_type!=COUPLED_TCP)
        return -1;

    if (_subflows.size()!=2){
        cout << "Expecting 2 subflows, found" << _subflows.size() << endl;
        exit(1);
    }

    double a;
    
    TcpSrc* flow0 = _subflows.front();
    TcpSrc* flow1 = _subflows.back();

    uint32_t cwnd0 = flow0->_in_fast_recovery?flow0->_ssthresh:flow0->_cwnd;
    uint32_t cwnd1 = flow1->_in_fast_recovery?flow1->_ssthresh:flow1->_cwnd;

    double pdelta = (double)cwnd1/cwnd0;
    double rdelta;

#if USE_AVG_RTT
    if (flow0->_rtt_avg>timeFromMs(0)&&flow1->_rtt_avg>timeFromMs(1))
        rdelta = (double)flow0->_rtt_avg / flow1->_rtt_avg;
    else
        rdelta = 1;

    //cout << "R1 " << flow0->_rtt_avg/1000000000 << " R2 " << flow1->_rtt_avg/1000000000 << endl;
#else
    rdelta = (double)flow0->_rtt / flow1->_rtt;

    //cout << "R1 " << flow0->_rtt/1000000000 << " R2 " << flow1->_rtt/1000000000 << endl;
#endif


    //cout << "R1 " << flow0->_rtt_avg/1000000000 << " R2 " << flow1->_rtt_avg/1000000000 << endl;

    //second_better
    if (1 < pdelta * rdelta * rdelta){
#if USE_AVG_RTT
        if (flow0->_rtt_avg>timeFromMs(0)&&flow1->_rtt_avg>timeFromMs(1))
            rdelta = (double)flow1->_rtt_avg / flow0->_rtt_avg;
        else
            rdelta = 1;
#else
        rdelta = (double)flow1->_rtt / flow0->_rtt;
#endif

        pdelta = (double)cwnd0/cwnd1;
    }

    if (_cc_type==COUPLED_INC){
        a = (1+pdelta)/(1+pdelta*rdelta)/(1+pdelta*rdelta);
        //cout << "A\t" << a << endl;


        if (a<0.5){
            cout << " a comp error " << a << ";resetting to 0.5" <<endl;
            a = 0.5;
        }

        //a = compute_a2();
        ///if (ABS(a-compute_a2())>0.01)
        //cout << "Compute a error! " <<a << " vs " << compute_a2()<<endl;
    }
    else{
        double t = 1.0+rdelta*sqrt(pdelta); 
        a = 1.0/t/t;
    }

    return a;
}

void MultipathTcpSrc::doNextEvent(){
    list<TcpSrc*>::iterator it;
    for(it=_subflows.begin();it!=_subflows.end();++it) {
        TcpSrc& flow = *(*it);
        flow.send_packets();
    }  

    eventlist().sourceIsPendingRel(*this, timeFromSec(1));
}


////////////////////////////////////////////////////////////////
//  MTCP SINK
////////////////////////////////////////////////////////////////

MultipathTcpSink::MultipathTcpSink(EventList& ev)  : EventSource(ev,"MTCPSink")
{
    _cumulative_ack = 0;

#ifdef DYNAMIC_RIGHT_SIZING
    _last_seq = 0;
    _last_min_rtt = timeFromMs(100);

    eventlist().sourceIsPendingRel(*this, timeFromMs(100));
#endif
    _nodename = "mtcpsink";
}

void MultipathTcpSink::addSubflow(TcpSink* sink){
    _subflows.push_back(sink);
    sink->joinMultipathConnection(this);
}

void MultipathTcpSink::doNextEvent(){
#ifdef DYNAMIC_RIGHT_SIZING
    list<TcpSink*>::iterator it;
    simtime_picosec min_rtt = timeFromSec(100);
    simtime_picosec max_rtt = 0;
    uint64_t total = 0;
    MultipathTcpSrc* m;

    for(it=_subflows.begin();it!=_subflows.end();++it) {
        TcpSink& flow = *(*it);
        total += flow._packets;
        m = flow._src->_mSrc;

        if (min_rtt > flow._src->_rtt){
            min_rtt = flow._src->_rtt;
        }
        if (max_rtt < flow._src->_rtt){
            max_rtt = flow._src->_rtt;
        }
    }

    if (_last_min_rtt<timeFromMs(1))
        cout << "Problem with last min rtt "<<endl;
    else{
        uint64_t new_window = 2 * (total - _last_seq) * max_rtt / _last_min_rtt;


        //    if (m->_receive_window<new_window){
        //  m->_receive_window = new_window;
    }
    //cout << "Receive buffer should be " << new_window << " crt " << m->_receive_window << endl;
    //}


    if (_last_seq!=total)
        _last_min_rtt = min_rtt;
    else
        _last_min_rtt += min_rtt;

    _last_seq = total;

    if (min_rtt<timeFromMs(1))
        min_rtt = timeFromMs(1);

    eventlist().sourceIsPendingRel(*this, min_rtt);
#endif
}

void
MultipathTcpSink::receivePacket(Packet& pkt) {
#ifdef MODEL_RECEIVE_WINDOW
    TcpPacket *p = (TcpPacket*)(&pkt);
    TcpPacket::seq_t seqno = p->data_seqno();
    int size = p->size();
  
    // cout << "at " << timeAsMs(eventlist().now()) << " Received on " << p->flow().id << " " << p->seqno() << " " << p->data_seqno() << " waiting for " << _cumulative_ack+1 << endl;

    if (seqno == _cumulative_ack+1) { // it's the next expected seq no
        _cumulative_ack = seqno + size - 1;
        // are there any additional received packets we can now ack?
        while (!_received.empty() && (_received.front() == _cumulative_ack+1) ) {
            _received.pop_front();
            _cumulative_ack+= size;
        }
    } else if (seqno < _cumulative_ack+1) { //must have been a bad retransmit
    } else { // it's not the next expected sequence number
        if (_received.empty()) {
            _received.push_front(seqno);
        } else if (seqno > _received.back()) { // likely case
            _received.push_back(seqno);
        } else { // uncommon case - it fills a hole
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
#endif
}        
