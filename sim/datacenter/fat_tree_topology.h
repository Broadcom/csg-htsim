// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef FAT_TREE
#define FAT_TREE
#include "main.h"
#include "randomqueue.h"
#include "pipe.h"
#include "config.h"
#include "loggers.h"
#include "network.h"
#include "firstfit.h"
#include "topology.h"
#include "logfile.h"
#include "eventlist.h"
#include "switch.h"
#include <ostream>

//#define N K*K*K/4

#ifndef QT
#define QT
typedef enum {UNDEFINED, RANDOM, ECN, COMPOSITE, PRIORITY,
              CTRL_PRIO, FAIR_PRIO, LOSSLESS, LOSSLESS_INPUT, LOSSLESS_INPUT_ECN,
              COMPOSITE_ECN, COMPOSITE_ECN_LB, SWIFT_SCHEDULER} queue_type;
typedef enum {UPLINK, DOWNLINK} link_direction;
#endif

class FatTreeTopology: public Topology{
public:
    vector <Switch*> switches_lp;
    vector <Switch*> switches_up;
    vector <Switch*> switches_c;

    vector< vector<Pipe*> > pipes_nc_nup;
    vector< vector<Pipe*> > pipes_nup_nlp;
    vector< vector<Pipe*> > pipes_nlp_ns;
    vector< vector<BaseQueue*> > queues_nc_nup;
    vector< vector<BaseQueue*> > queues_nup_nlp;
    vector< vector<BaseQueue*> > queues_nlp_ns;

    vector< vector<Pipe*> > pipes_nup_nc;
    vector< vector<Pipe*> > pipes_nlp_nup;
    vector< vector<Pipe*> > pipes_ns_nlp;
    vector< vector<BaseQueue*> > queues_nup_nc;
    vector< vector<BaseQueue*> > queues_nlp_nup;
    vector< vector<BaseQueue*> > queues_ns_nlp;
  
    FirstFit* ff;
    QueueLoggerFactory* _logger_factory;
    EventList* _eventlist;
    uint32_t failed_links;
    queue_type _qt;
    queue_type _sender_qt;
    uint32_t _bdp;

    FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize, QueueLoggerFactory* logger_factory,
                    EventList* ev,FirstFit* f, queue_type qt, simtime_picosec latency, simtime_picosec switch_latency, uint32_t bdp, queue_type snd = FAIR_PRIO);
    FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize, QueueLoggerFactory* logger_factory,
                    EventList* ev,FirstFit* f, queue_type qt, simtime_picosec latency, simtime_picosec switch_latency, queue_type snd = FAIR_PRIO);
    FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize, QueueLoggerFactory* logger_factory,
                    EventList* ev,FirstFit* f, queue_type qt);      
    FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize, QueueLoggerFactory* logger_factory,
                    EventList* ev,FirstFit* f, queue_type qt, uint32_t fail);
    FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize, QueueLoggerFactory* logger_factory,
                    EventList* ev,FirstFit* f, queue_type qt, queue_type sender_qt, uint32_t fail);
    FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize, QueueLoggerFactory* logger_factory,
                    EventList* ev,FirstFit* f, queue_type qt, queue_type sender_qt, uint32_t fail, uint32_t bdp);

    void init_network();
    virtual vector<const Route*>* get_bidir_paths(uint32_t src, uint32_t dest, bool reverse);

    BaseQueue* alloc_src_queue(QueueLogger* q);
    BaseQueue* alloc_queue(QueueLogger* q, mem_b queuesize, link_direction dir, bool tor);
    BaseQueue* alloc_queue(QueueLogger* q, uint64_t speed, mem_b queuesize,
                           link_direction dir, bool tor);
    static void set_tiers(uint32_t tiers) {_tiers = tiers;}
    static uint32_t get_tiers() {return _tiers;}

    void count_queue(Queue*);
    void print_path(std::ofstream& paths,uint32_t src,const Route* route);
    void report_stats();
    vector<uint32_t>* get_neighbours(uint32_t src) { return NULL;};
    uint32_t no_of_nodes() const {return _no_of_nodes;}
    uint32_t no_of_cores() const {return NCORE;}
    uint32_t no_of_servers() const {return NSRV;}
    uint32_t no_of_pods() const {return NPOD;}
    uint32_t no_of_switches_per_pod() const {return K;}

    void add_failed_link(uint32_t type, uint32_t switch_id, uint32_t link_id);

    // add loggers to record total queue size at switches
    virtual void add_switch_loggers(Logfile& log, simtime_picosec sample_period); 

    uint32_t HOST_POD_SWITCH(uint32_t src){
        return 2*src/K;
    }
    uint32_t HOST_POD_ID(uint32_t src){
        if (_tiers == 3)
            return src%NCORE;
        else
            // only one pod in leaf-spine
            return src;
    }
    uint32_t HOST_POD(uint32_t src){
        if (_tiers == 3) 
            return src/NCORE;
        else
            // only one pod in leaf-spine
            return 0;
    }
    uint32_t MIN_POD_ID(uint32_t pod_id){
        if (_tiers == 2) assert(pod_id == 0);
        return pod_id*K/2;
    }
    uint32_t MAX_POD_ID(uint32_t pod_id){
        if (_tiers == 2) assert(pod_id == 0);
        return (pod_id+1)*K/2-1;
    }
    
    uint32_t getK() const {return K;}
    uint32_t getNAGG() const {return NAGG;}
private:
    map<Queue*,int> _link_usage;
    int64_t find_lp_switch(Queue* queue);
    int64_t find_up_switch(Queue* queue);
    int64_t find_core_switch(Queue* queue);
    int64_t find_destination(Queue* queue);
    void set_params(uint32_t no_of_nodes);
    uint32_t K, NCORE, NAGG, NTOR, NSRV, NPOD;
    static uint32_t _tiers;
    uint32_t _no_of_nodes;
    mem_b _queuesize;
    linkspeed_bps _linkspeed;
    simtime_picosec _hop_latency,_switch_latency;
};

#endif
