// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#ifndef EXOQUEUE_H
#define EXOQUEUE_H

/*
 * A simple exogenous queue
 */

#include <list>
#include "config.h"
#include "eventlist.h"
#include "network.h"
#include "loggertypes.h"

class ExoQueue : public PacketSink {
public:
    ExoQueue(double loss_rate);
    void receivePacket(Packet& pkt);

    void setLossRate(double l);
    // should really be private, but loggers want to see

    // Housekeeping
    double _loss_rate;
    string _nodename;

    const string& nodename() {return _nodename;};
};
#endif
