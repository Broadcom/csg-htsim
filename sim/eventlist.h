// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef EVENTLIST_H
#define EVENTLIST_H

#include <map>
#include <sys/time.h>
#include "config.h"
#include "loggertypes.h"

class EventList;
class TriggerTarget;

class EventSource : public Logged {
public:
    EventSource(EventList& eventlist, const string& name) : Logged(name), _eventlist(eventlist) {};
    virtual ~EventSource() {};
    virtual void doNextEvent() = 0;
    inline EventList& eventlist() const {return _eventlist;}
protected:
    EventList& _eventlist;
};

class EventList {
public:
    EventList();
    static void setEndtime(simtime_picosec endtime); // end simulation at endtime (rather than forever)
    static bool doNextEvent(); // returns true if it did anything, false if there's nothing to do
    static void sourceIsPending(EventSource &src, simtime_picosec when);
    static void sourceIsPendingRel(EventSource &src, simtime_picosec timefromnow)
    { sourceIsPending(src, EventList::now()+timefromnow); }
    static void cancelPendingSource(EventSource &src);
    static void reschedulePendingSource(EventSource &src, simtime_picosec when);
    static void triggerIsPending(TriggerTarget &target);
    static inline simtime_picosec now() {return EventList::_lasteventtime;}

    static EventList& getTheEventList();
    EventList(const EventList&)      = delete;  // disable Copy Constructor
    void operator=(const EventList&) = delete;  // disable Assign Constructor

private:
    static simtime_picosec _endtime;
    static simtime_picosec _lasteventtime;
    typedef multimap <simtime_picosec, EventSource*> pendingsources_t;
    static pendingsources_t _pendingsources;
    static vector <TriggerTarget*> _pending_triggers;

    static int _instanceCount;
    static EventList* _theEventList;
};

#endif
