// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "dctcp.h"
#include "ecn.h"
#include "mtcp.h"
#include "config.h"

string ntoa(double n);


////////////////////////////////////////////////////////////////
//  DCTCP SOURCE
////////////////////////////////////////////////////////////////

DCTCPSrc::DCTCPSrc(TcpLogger* logger, TrafficLogger* pktlogger, 
                   EventList &eventlist) : TcpSrc(logger,pktlogger,eventlist)
{
    _pkts_seen = 0;
    _pkts_marked = 0;
    _alfa = 0;
    _past_cwnd = 2*Packet::data_packet_size();
    _rto = timeFromMs(10);    
}

//drop detected
void
DCTCPSrc::deflate_window(){
    _pkts_seen = 0;
    _pkts_marked = 0;
    if (_mSrc==NULL){
        _ssthresh = max(_cwnd/2, (uint32_t)(2 * _mss));
    }
    else
        _ssthresh = _mSrc->deflate_window(_cwnd,_mss);

    _past_cwnd = _cwnd;
}


void
DCTCPSrc::receivePacket(Packet& pkt) 
{
    _pkts_seen++;

    if (pkt.flags() & ECN_ECHO){
        _pkts_marked += 1;

        //exit slow start since we're causing congestion
        if (_ssthresh>_cwnd)
            _ssthresh = _cwnd;
    }

    if (_pkts_seen * _mss >= _past_cwnd){
        //update window, once per RTT
        
        double f = (double)_pkts_marked/_pkts_seen;
        //        cout << ntoa(timeAsMs(eventlist().now())) << " ID " << str() << " PKTS MARKED " << _pkts_marks;

        _alfa = 15.0/16.0 * _alfa + 1.0/16.0 * f;
        _pkts_seen = 0;
        _pkts_marked = 0;

        if (_alfa>0){
            _cwnd = _cwnd * (1-_alfa/2);

            if (_cwnd<_mss)
                _cwnd = _mss;

            _ssthresh = _cwnd;
        }
        _past_cwnd = _cwnd;

        //cout << ntoa(timeAsMs(eventlist().now())) << " UPDATE " << str() << " CWND " << _cwnd << " alfa " << ntoa(_alfa)<< " marked " << ntoa(f) << endl;
    }

    TcpSrc::receivePacket(pkt);
    //cout << ntoa(timeAsMs(eventlist().now())) << " ATCPID " << str() << " CWND " << _cwnd << " alfa " << ntoa(_alfa)<< endl;
}

void 
DCTCPSrc::rtx_timer_hook(simtime_picosec now,simtime_picosec period){
    TcpSrc::rtx_timer_hook(now,period);
};

