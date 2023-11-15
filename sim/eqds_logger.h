// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef EQDS_LOGGER_H
#define EQDS_LOGGER_H
#include "config.h"
#include "loggers.h"
#include "eqds.h"

class EqdsSrc;

class EqdsLogger  : public Logger {
 public:
    enum EqdsEvent { EQDS_RCV=0, EQDS_RCV_FR_END=1, EQDS_RCV_FR=2, EQDS_RCV_DUP_FR=3,
                    EQDS_RCV_DUP=4, EQDS_RCV_3DUPNOFR=5,
                    EQDS_RCV_DUP_FASTXMIT=6, EQDS_TIMEOUT=7};
    enum EqdsState { EQDSSTATE_CNTRL=0, EQDSSTATE_SEQ=1 };
    enum EqdsRecord { AVE_CWND=0 };
    enum EqdsSinkRecord { RATE = 0 };
    enum EqdsMemoryRecord  {MEMORY = 0};

    virtual void logEqds(EqdsSrc &src, EqdsEvent ev) =0;
    virtual ~EqdsLogger(){};
};

class EqdsSinkLoggerSampling : public SinkLoggerSampling {
    virtual void doNextEvent();
 public:
    EqdsSinkLoggerSampling(simtime_picosec period, EventList& eventlist);
    static string event_to_str(RawLogEvent& event);
};


#endif
