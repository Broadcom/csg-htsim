// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef incast
#define incast

#include "main.h"
#include "tcp_transfer.h"
#include "randomqueue.h"
#include "eventlist.h"
#include <list>
#include <map>
#include <vector>

class Incast : public EventSource {
public:
    Incast(uint64_t bytes, EventList& eventlist);
    void addFlow(TcpSrcTransfer* src);
    void doNextEvent();

private:
    vector<TcpSrcTransfer*> _flows;
    uint32_t _finished;
    uint64_t _bytes;
};

#endif
