// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#include "priopullqueue.h"
#include "ndppacket.h"

template<class PullPkt>
PrioPullQueue<PullPkt>::PrioPullQueue() {
    this->_pull_count = 0;
}


template<class PullPkt>
void
PrioPullQueue<PullPkt>::enqueue(PullPkt& pkt, int priority) {
    CircularBuffer<PullPkt*>* pull_queue;
    if (queue_exists(pkt, priority)) {
        pull_queue = find_queue(pkt, priority);
    }  else {
        self_check();
        pull_queue = create_queue(pkt, priority);
        self_check();
    }
    //we add packets to the front,remove them from the back
    PullPkt* pkt_p = &pkt;
    pull_queue->push(pkt_p);
    this->_pull_count++;
    _prio_counts[priority]++;
    self_check();
}

template<class PullPkt>
void PrioPullQueue<PullPkt>::self_check() {
    int count = 0;
    typename PrioMap::iterator pmi;
    for (pmi = _prio_queue_map.begin(); pmi != _prio_queue_map.end(); pmi++) {
        int priority = pmi->first;
        count += _prio_counts[priority];
    }
    assert(count == this->_pull_count);
}

template<class PullPkt>
PullPkt* 
PrioPullQueue<PullPkt>::dequeue() {
    if (this->_pull_count == 0)
        return 0;
    typename PrioMap::iterator pmi;
    //cout << "prio_map size: " << _prio_queue_map.size() << endl;
    for (pmi = _prio_queue_map.begin(); pmi != _prio_queue_map.end(); pmi++) {
        QueueMap *queue_map = pmi->second;
        int priority = pmi->first;
        if (!queue_map->empty() && _prio_counts[priority] > 0) {
            // we keep one fair queue iterator for each priority level
            typename CurrentQueueMap::iterator cqmi = _current_queue_map.find(priority);
            typename QueueMap::iterator current_queue = cqmi->second;
            //cout << "queue_map size: " << queue_map->size() << endl;
            while (1) {
                if (current_queue == queue_map->end())
                    current_queue = queue_map->begin();
                CircularBuffer <PullPkt*>* pull_queue = current_queue->second;
                current_queue++;
                if (!pull_queue->empty()) {
                    //we add packets to the front,remove them from the back
                    PullPkt* packet = pull_queue->pop();
                    this->_pull_count--;
                    _prio_counts[priority]--;

                    //remember where we were for next time
                    _current_queue_map[priority] = current_queue;
                    self_check();
                    return packet;
                } 
                // there are packets queued, so we'll eventually find a queue
                // that lets this terminate
            }
        }
    }
    // shouldn't get here if something is queued
    cout << "debug trace\n";
    cout << "pull count: " << this->_pull_count << endl;
    for (pmi = _prio_queue_map.begin(); pmi != _prio_queue_map.end(); pmi++) {
        int priority = pmi->first;
        cout << "Prio: " << priority << " count " << _prio_counts[priority] << endl;
    }
    abort();
}

template<class PullPkt>
void
PrioPullQueue<PullPkt>::flush_flow(flowid_t flow_id, int priority) {
    typename PrioMap::iterator pmi = _prio_queue_map.find(priority);
    if (pmi == _prio_queue_map.end()) {
        return;
    }
    QueueMap *queue_map = pmi->second;
    typename QueueMap::iterator i;
    i = queue_map->find(flow_id);
    if (i == queue_map->end())
        return;
    CircularBuffer<PullPkt*>* pull_queue = i->second;
    while (!pull_queue->empty()) {
            PullPkt* packet = pull_queue->pop();
            packet->free();
            this->_pull_count--;
            _prio_counts[priority]--;
    }

    // move the iterator on if it points to the queue we're about to erase
    typename CurrentQueueMap::iterator cqmi = _current_queue_map.find(priority);
    typename QueueMap::iterator current_queue = cqmi->second;
    if (current_queue == i) {
        current_queue++;
        _current_queue_map[priority] = current_queue;
    }
    
    queue_map->erase(i);
    self_check();
}

template<class PullPkt>
bool 
PrioPullQueue<PullPkt>::queue_exists(const PullPkt& pkt, int priority) {
    typename PrioMap::iterator pmi = _prio_queue_map.find(priority);
    if (pmi == _prio_queue_map.end()) {
        return false;
    }
    QueueMap *queue_map = pmi->second;
    typename QueueMap::iterator i;
    i = queue_map->find(pkt.flow_id());
    if (i == queue_map->end())
        return false;
    return true;
}

template<class PullPkt>
CircularBuffer<PullPkt*>* 
PrioPullQueue<PullPkt>::find_queue(const PullPkt& pkt, int priority) {
    typename PrioMap::iterator pmi = _prio_queue_map.find(priority);
    if (pmi == _prio_queue_map.end()) {
        return 0;
    }
    QueueMap *queue_map = pmi->second;
    typename QueueMap::iterator i;
    i = queue_map->find(pkt.flow_id());
    if (i == queue_map->end())
        return 0;
    return i->second;
}

template<class PullPkt>
CircularBuffer<PullPkt*>* 
PrioPullQueue<PullPkt>::create_queue(const PullPkt& pkt, int priority) {
    //cout << "create_queue, prio " << priority << endl;
    typename PrioMap::iterator pmi;
    /*
    for (pmi = _prio_queue_map.begin(); pmi != _prio_queue_map.end(); pmi++) {
        cout << "prio " << pmi->first << endl;
    }
    */
    pmi = _prio_queue_map.find(priority);
    QueueMap *queue_map = 0;
    if (pmi == _prio_queue_map.end()) {
        queue_map = new QueueMap();
        _prio_queue_map[priority] = queue_map;
        _current_queue_map[priority] = queue_map->end();
        _prio_counts[priority] = 0;
    } else {
        queue_map = pmi->second;
    }
    
    CircularBuffer<PullPkt*>* new_queue = new CircularBuffer<PullPkt*>;
    queue_map->insert(pair<flowid_t, CircularBuffer<PullPkt*>*>(pkt.flow_id(), new_queue));

    self_check();
    return new_queue;
}

template class PrioPullQueue<NdpPull>;
template class PrioPullQueue<Packet>;

