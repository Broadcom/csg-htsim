// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#include "qcn.h"

const int QcnPacket::PKTSIZE=1500;
const int QcnAck::ACK_SIZE=40;
const double QcnReactor::GAIN = 0.5; // assume Fb has been normalized to [0,1]
const int QcnReactor::CYCLESOFRECOVERY = 5;
const int QcnReactor::PACKETSPERRECOVERYCYCLE=100;
const linkspeed_bps QcnReactor::RATEINCREASE=500000; //0.5Mb/s
const linkspeed_bps QcnReactor::MINRATE=1000000; //1Mb/s
const double QcnQueue::GAMMA = 2;

PacketDB<QcnPacket> QcnPacket::_packetdb;
PacketDB<QcnAck> QcnAck::_packetdb;


QcnReactor::QcnReactor(QcnLogger* logger, TrafficLogger* pktlogger, EventList &eventlist)
    : EventSource(eventlist,"qcn"), _logger(logger), _flow(pktlogger) {
}

void QcnReactor::connect(route_t& route, routes_t& routesback, simtime_picosec startTime, linkspeed_bps linkspeed) {
    _route = &route;
    _routesback = &routesback;
    _flow.set_id(get_id()); // identify the packet flow with the QCN source that generated it
    //
    _packetCycles = 0;
    _timerCycles = 0;
    _packetsSentInCurrentCycle = 0;
    _linkspeed = linkspeed;
    _currentRate = (double)linkspeed;
    _targetRate = (double)linkspeed;
    _seqno = 0;
    //
    eventlist().sourceIsPending(*this,startTime);
}

void QcnReactor::doNextEvent() {
    // Ignore timer cycles for now, i.e. scheduling is only to send packets
    // Send this packet
    int pktsize = QcnPacket::PKTSIZE;
    QcnPacket* p = QcnPacket::newpkt(_flow,*_route,*_routesback,*this,pktsize,_seqno);
    _logger->logQcn(*this,QcnLogger::QCN_SEND,0);
    p->sendOn();
    // Schedule the next packet
    _seqno++;          
    simtime_picosec nextPacketDue = timeFromSec(8*pktsize/(double)_currentRate);
    eventlist().sourceIsPendingRel(*this,nextPacketDue);
    // Act on the packet-counter
    onPacketSent();
}

void QcnReactor::receivePacket(Packet& pkt) { // i.e. react to feedback
    QcnAck *p = (QcnAck*)(&pkt);
    double fb = p->_fb;
    p->free();
    onFeedback(fb);
}

void QcnEndpoint::receivePacket(Packet& pkt)
{ 
    QcnPacket *p = (QcnPacket*)(&pkt);
    //pkt.flow().logTraffic(pkt,*this,TrafficLogger::PKT_RCVDESTROY);
    p->free();
};

QcnQueue::QcnQueue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, QueueLogger* logger, QcnLogger* qcnlogger)
    : Queue(bitrate,maxsize,eventlist,logger), _qcnlogger(qcnlogger)
{
    _packetsTillNextFeedback = 0;
    _lastSampledQueuesize = 0;
    _targetQueuesize = (mem_b)(0.2*maxsize);
};

void QcnQueue::receivePacket(Packet& pkt) {
    QcnPacket* qpkt = (QcnPacket*)(&pkt);
    onPacketReceived(*qpkt);
    Queue::receivePacket(pkt);
};


//------------------------------------------------------------------
// THE SPECIFICATION OF THE ALGORITHM

void QcnReactor::onPacketSent() {
    _packetsSentInCurrentCycle++;
    if (_packetsSentInCurrentCycle>=QcnReactor::PACKETSPERRECOVERYCYCLE) {
        _logger->logQcn(*this,QcnLogger::QCN_INC,_targetRate-_currentRate);
        _packetCycles++;
        _packetsSentInCurrentCycle = 0;
        _targetRate += (double)QcnReactor::RATEINCREASE;
        _currentRate = (_currentRate+_targetRate)/2;
        _logger->logQcn(*this,QcnLogger::QCN_INCD,0);
    }
}

void QcnReactor::onFeedback(double fb) {
    // Really it'd be more elegant to update the next time-to-send-pkt. But I won't bother.
    _logger->logQcn(*this,QcnLogger::QCN_DEC,fb);
    if (_packetCycles>0) _targetRate = _currentRate;
    _packetCycles = 0;
    _packetsSentInCurrentCycle = 0;
    _currentRate = qmax(_currentRate*(1+QcnReactor::GAIN*fb),QcnReactor::MINRATE);
    _logger->logQcn(*this,QcnLogger::QCN_DECD,0);
}

void QcnQueue::onPacketReceived(QcnPacket &qpkt) {
    _packetsTillNextFeedback--;
    if (_packetsTillNextFeedback<=0) {
        double fbrange = _targetQueuesize * (1+2*QcnQueue::GAMMA);
        double fb1 = - (_queuesize-_targetQueuesize) / fbrange;
        double fb2 = - QcnQueue::GAMMA*(_queuesize-_lastSampledQueuesize) / fbrange;
        double fb = qmax(-1,fb1 + fb2);
        if (fb<0) {
            _qcnlogger->logQcnQueue(*this,QcnLogger::QCN_FB,fb,fb1,fb2);
            QcnAck* ack = QcnAck::newpkt(qpkt, (QcnAck::fb_t)fb);
            ack->sendOn();
        }
        else
            _qcnlogger->logQcnQueue(*this,QcnLogger::QCN_NOFB,fb,fb1,fb2);
        _lastSampledQueuesize = _queuesize;
        _packetsTillNextFeedback = 100;
    }
}
