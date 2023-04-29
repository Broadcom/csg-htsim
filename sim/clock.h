#ifndef CLOCK_H
#define CLOCK_H

/*
 * A convenient item to put into an eventlist: it displays a tick mark every so often,
 * to show the simulation is running.
 */

#include "config.h"
#include "eventlist.h"

class Clock : public EventSource {
public:
        Clock(simtime_picosec period, EventList& eventlist); 
        void doNextEvent();
private:
        simtime_picosec _period;
        int _smallticks;
        };

#endif
