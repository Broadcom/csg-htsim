// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        

#include "queue.h"
#include "switch.h"
#include "eth_pause_packet.h"
#include "queue_lossless.h"
#include "queue_lossless_input.h"
#include "loggers.h"

uint32_t Switch::id = 0;

int Switch::addPort(BaseQueue* q){
    _ports.push_back(q);
    q->setSwitch(this);
    return _ports.size()-1;
    
}

void Switch::sendPause(LosslessQueue* problem, unsigned int wait){
    cout << "Switch " << _name << " link " << problem->_name << " blocked, sending pause " << wait << endl;

    for (size_t i = 0;i < _ports.size();i++){
        LosslessQueue* q = (LosslessQueue*)_ports.at(i);
    
        if (q==problem || !(q->getRemoteEndpoint()))
            continue;

        cout << "Informing " << q->_name << endl;
        EthPausePacket* pkt = EthPausePacket::newpkt(wait,_id);
        q->getRemoteEndpoint()->receivePacket(*pkt);
    }
};

void Switch::configureLossless(){
    for (size_t i = 0;i < _ports.size();i++){
        LosslessQueue* q = (LosslessQueue*)_ports.at(i);    
        q->setSwitch(this);
        q->initThresholds();
    }
};
/*Switch::configureLosslessInput(){
  for (list<Queue*>::iterator it=_ports.begin(); it != _ports.end(); ++it){
  LosslessInputQueue* q = (LosslessInputQueue*)*it;
  q->setSwitch(this);
  q->initThresholds();
  }
  };*/

void Switch::add_logger(Logfile& log, simtime_picosec sample_period) {
    // we want to log the sum of all queues on the switch, so we have
    // one logger that is shared by all ports
    assert(_ports.size() > 0);
    MultiQueueLoggerSampling* queue_logger = new MultiQueueLoggerSampling(get_id(), sample_period,_ports.at(0)->eventlist());
    log.addLogger(*queue_logger);
    for (size_t i = 0; i < _ports.size(); i++) {
        //cout << "adding logger to switch " << nodename() << " id " << get_id() << " queue " << _ports.at(i)->nodename() << " id " << _ports.at(i)->get_id() << endl;
        _ports.at(i)->setLogger(queue_logger);
    }
}
