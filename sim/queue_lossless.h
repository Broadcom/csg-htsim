// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef _LOSSLESS_QUEUE_H
#define _LOSSLESS_QUEUE_H
#include "queue.h"
/*
 * A FIFO queue that supports PAUSE frames and lossless operation
 */

#include <list>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"
#include "eth_pause_packet.h"

class Switch;

class LosslessQueue : public Queue {
public:
    LosslessQueue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, QueueLogger* logger, 
                  Switch* sw);

    void receivePacket(Packet& pkt);
    void beginService();
    void completeService();
    void initThresholds();

    //    void setSwitch(Switch *s) {_switch = s;};
    //Switch* getSwitch() {return _switch;};

    //void enqueuePauseFrame(EthPausePacket*);

    enum {PAUSED,READY,PAUSE_RECEIVED};

private:
    //    Switch* _switch;
    int _state_send;
    int _state_recv;

    int _sending;

    int _low_threshold;
    int _high_threshold;
};

#endif
