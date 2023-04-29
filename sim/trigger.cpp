// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-                                                     
#include <algorithm>
#include "trigger.h"
using namespace std;

Trigger::Trigger(EventList& eventlist, triggerid_t id) : _eventlist(eventlist), _id(id) {
}

void
Trigger::add_target(TriggerTarget& target) {
    _targets.push_back(&target);
};


SingleShotTrigger::SingleShotTrigger(EventList& eventlist, triggerid_t id)
    : Trigger(eventlist, id)
{
    _done = false;
}

void
SingleShotTrigger::activate() {
    assert(!_done);
    assert(_targets.size() > 0);
    vector <TriggerTarget*>::iterator i;
    cout << "Trigger " << _id << " fired, " << _targets.size() << " targets\n";
    for (i = _targets.begin(); i != _targets.end(); i++) {
        _eventlist.triggerIsPending(**i);
    }
    _done = true;
}

MultiShotTrigger::MultiShotTrigger(EventList& eventlist, triggerid_t id)
    : Trigger(eventlist, id)
{
    _next = 0;
}

void
MultiShotTrigger::activate() {
    if (_next>=_targets.size()){
        //I have activated everyone!
        cout << "Noone left to activate" << endl;
        return;
    }
    vector <TriggerTarget*>::iterator i;
    cout << "Multishot Trigger " << _id << " fired, " << _targets.size() << " targets\n";
    _eventlist.triggerIsPending(*(_targets[_next]));
    _next++;
}

BarrierTrigger::BarrierTrigger(EventList& eventlist, triggerid_t id, size_t activations_needed)
    : Trigger(eventlist, id)
{
    _activations_remaining = activations_needed;
}

void
BarrierTrigger::activate() {
    assert(_activations_remaining > 0);
    assert(_targets.size() > 0);
    _activations_remaining--;
    if (_activations_remaining > 0) {
        cout << "Trigger " << _id << " activated, activations remaining: "
             << _activations_remaining << endl;
        return;
    }
    vector <TriggerTarget*>::iterator i;
    cout << "Trigger " << _id << " fired, " << _targets.size() << " targets\n";
    for (i = _targets.begin(); i != _targets.end(); i++) {
        _eventlist.triggerIsPending(**i);
    }
}
