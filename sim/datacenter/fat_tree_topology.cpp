// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "fat_tree_topology.h"
#include <vector>
#include "string.h"
#include <sstream>

#include <iostream>
#include "main.h"
#include "queue.h"
#include "fat_tree_switch.h"
#include "compositequeue.h"
#include "prioqueue.h"
#include "queue_lossless.h"
#include "queue_lossless_input.h"
#include "queue_lossless_output.h"
#include "swift_scheduler.h"
#include "ecnqueue.h"

string ntoa(double n);
string itoa(uint64_t n);

// default to 3-tier topology.  Change this with set_tiers() before calling the constructor.
uint32_t FatTreeTopology::_tiers = 3;

//extern int N;
FatTreeTopology::FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
                                 QueueLoggerFactory* logger_factory,
                                 EventList* ev,FirstFit * fit,queue_type q, simtime_picosec latency, simtime_picosec switch_latency, 
                                 uint32_t bdp, queue_type snd
                                 ){
    _bdp = bdp;
    _linkspeed = linkspeed;
    _queuesize = queuesize;
    _logger_factory = logger_factory;
    _eventlist = ev;
    ff = fit;
    _qt = q;
    _sender_qt = snd;
    failed_links = 0;
    _hop_latency = latency;
    _switch_latency = switch_latency;

    cout << "Fat Tree topology with " << timeAsUs(_hop_latency) << "us links and " << timeAsUs(_switch_latency) <<"us switching latency." <<endl;
    set_params(no_of_nodes);

    init_network();
}
FatTreeTopology::FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
                                 QueueLoggerFactory* logger_factory,
                                 EventList* ev,FirstFit * fit,queue_type q, simtime_picosec latency, simtime_picosec switch_latency, 
                                 queue_type snd
                                 ){
    _linkspeed = linkspeed;
    _queuesize = queuesize;
    _logger_factory = logger_factory;
    _eventlist = ev;
    ff = fit;
    _qt = q;
    _sender_qt = snd;
    failed_links = 0;
    _hop_latency = latency;
    _switch_latency = switch_latency;


    cout << "Fat Tree topology with " << timeAsUs(_hop_latency) << "us links and " << timeAsUs(_switch_latency) <<"us switching latency." <<endl;
    set_params(no_of_nodes);

    init_network();
}

FatTreeTopology::FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
                                 QueueLoggerFactory* logger_factory,
                                 EventList* ev,FirstFit * fit,queue_type q){
    _linkspeed = linkspeed;
    _queuesize = queuesize;
    _logger_factory = logger_factory;
    _eventlist = ev;
    ff = fit;
    _qt = q;
    _sender_qt = FAIR_PRIO;
    failed_links = 0;
    _hop_latency = timeFromUs((uint32_t)1); 
    _switch_latency = timeFromUs((uint32_t)0); 
 
    cout << "Fat tree topology (1) with " << no_of_nodes << " nodes" << endl;
    set_params(no_of_nodes);

    init_network();
}

FatTreeTopology::FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
                                 QueueLoggerFactory* logger_factory,
                                 EventList* ev,FirstFit * fit, queue_type q, 
                                 uint32_t num_failed){
    _linkspeed = linkspeed;
    _queuesize = queuesize;
    _hop_latency = timeFromUs((uint32_t)1); 
    _switch_latency = timeFromUs((uint32_t)0); 
    _logger_factory = logger_factory;
    _qt = q;
    _sender_qt = FAIR_PRIO;

    _eventlist = ev;
    ff = fit;

    failed_links = num_failed;
  
    cout << "Fat tree topology (2) with " << no_of_nodes << " nodes" << endl;
    set_params(no_of_nodes);

    init_network();
}

FatTreeTopology::FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
                                 QueueLoggerFactory* logger_factory,
                                 EventList* ev,FirstFit * fit, queue_type qtype,
                                 queue_type sender_qtype, uint32_t num_failed){
    _linkspeed = linkspeed;
    _queuesize = queuesize;
    _hop_latency = timeFromUs((uint32_t)1); 
    _switch_latency = timeFromUs((uint32_t)0); 
    _logger_factory = logger_factory;
    _qt = qtype;
    _sender_qt = sender_qtype;

    _eventlist = ev;
    ff = fit;

    failed_links = num_failed;

    cout << "Fat tree topology (3) with " << no_of_nodes << " nodes" << endl;
    set_params(no_of_nodes);

    init_network();
}

FatTreeTopology::FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
                                 QueueLoggerFactory* logger_factory,
                                 EventList* ev,FirstFit * fit, queue_type qtype,
                                 queue_type sender_qtype, uint32_t num_failed, uint32_t bdp){
    _linkspeed = linkspeed;
    _queuesize = queuesize;
    _hop_latency = timeFromUs((uint32_t)1); 
    _switch_latency = timeFromUs((uint32_t)0); 
    _logger_factory = logger_factory;
    _qt = qtype;
    _sender_qt = sender_qtype;

    _bdp = bdp;
    _eventlist = ev;
    ff = fit;

    failed_links = num_failed;

    cout << "Fat tree topology (4) with " << no_of_nodes << " nodes" << endl;
    set_params(no_of_nodes);

    init_network();
}

void FatTreeTopology::set_params(uint32_t no_of_nodes) {
    cout << "Set params " << no_of_nodes << endl;
    cout << "QueueSize " << _queuesize << endl;
    _no_of_nodes = 0;
    K = 0;
    if (_tiers == 3) {
        while (_no_of_nodes < no_of_nodes) {
            K++;
            _no_of_nodes = K * K * K /4;
        }
        if (_no_of_nodes > no_of_nodes) {
            cerr << "Topology Error: can't have a 3-Tier FatTree with " << no_of_nodes
                 << " nodes\n";
            exit(1);
        }
        int NK = (K*K/2);
        NSRV = (K*K*K/4);
        NTOR = NK;
        NAGG = NK;
        NPOD = K;
        NCORE = (K*K/4);
    } else if (_tiers == 2) {
        // We want a leaf-spine topology
        while (_no_of_nodes < no_of_nodes) {
            K++;
            _no_of_nodes = K * K /2;
        }
        if (_no_of_nodes > no_of_nodes) {
            cerr << "Topology Error: can't have a 2-Tier FatTree with " << no_of_nodes
                 << "node, the closest is " << _no_of_nodes
		    << " nodes \n";
            exit(1);
        }
        int NK = K;
        NSRV = K * K /2;
        NTOR = NK;
        NAGG = NK/2;
        NPOD = 1;
        NCORE = 0;
    } else {
        cerr << "Topology Error: " << _tiers << " tier FatTree not supported\n";
        exit(1);
    }
    
    
    cout << "_no_of_nodes " << _no_of_nodes << endl;
    cout << "K " << K << endl;
    cout << "Queue type " << _qt << endl;

    switches_lp.resize(NTOR,NULL);
    switches_up.resize(NAGG,NULL);
    switches_c.resize(NCORE,NULL);


    // These vectors are sparse - we won't use all the entries
    if (_tiers == 3) {
        pipes_nc_nup.resize(NCORE, vector<Pipe*>(NAGG));
        queues_nc_nup.resize(NCORE, vector<BaseQueue*>(NAGG));
    }

    pipes_nup_nlp.resize(NAGG, vector<Pipe*>(NTOR));
    queues_nup_nlp.resize(NAGG, vector<BaseQueue*>(NTOR));

    pipes_nlp_ns.resize(NTOR, vector<Pipe*>(NSRV));
    queues_nlp_ns.resize(NTOR, vector<BaseQueue*>(NSRV));


    if (_tiers == 3) {
        pipes_nup_nc.resize(NAGG, vector<Pipe*>(NCORE));
        queues_nup_nc.resize(NAGG, vector<BaseQueue*>(NCORE));
    }
    
    pipes_nlp_nup.resize(NTOR, vector<Pipe*>(NAGG));
    pipes_ns_nlp.resize(NSRV, vector<Pipe*>(NTOR));
    queues_nlp_nup.resize(NTOR, vector<BaseQueue*>(NAGG));
    queues_ns_nlp.resize(NSRV, vector<BaseQueue*>(NTOR));
}

BaseQueue* FatTreeTopology::alloc_src_queue(QueueLogger* queueLogger){
    switch (_sender_qt) {
    case SWIFT_SCHEDULER:
        return new FairScheduler(_linkspeed, *_eventlist, queueLogger);
    case PRIORITY:
        return new PriorityQueue(_linkspeed,
                                 memFromPkt(FEEDER_BUFFER), *_eventlist, queueLogger);
    case FAIR_PRIO:
        return new FairPriorityQueue(_linkspeed,
                                     memFromPkt(FEEDER_BUFFER), *_eventlist, queueLogger);
    default:
        abort();
    }
}

BaseQueue* FatTreeTopology::alloc_queue(QueueLogger* queueLogger, mem_b queuesize,
                                        link_direction dir, bool tor = false){
    return alloc_queue(queueLogger, _linkspeed, queuesize, dir, tor);
}

BaseQueue*
FatTreeTopology::alloc_queue(QueueLogger* queueLogger, linkspeed_bps speed, mem_b queuesize,
                             link_direction dir, bool tor){
    switch (_qt) {
    case RANDOM:
        return new RandomQueue(speed, queuesize, *_eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
    case COMPOSITE:
        return new CompositeQueue(speed, queuesize, *_eventlist, queueLogger);
    case CTRL_PRIO:
        return new CtrlPrioQueue(speed, queuesize, *_eventlist, queueLogger);
    case ECN:
        return new ECNQueue(speed, queuesize, *_eventlist, queueLogger, memFromPkt(15));
    case LOSSLESS:
        return new LosslessQueue(speed, queuesize, *_eventlist, queueLogger, NULL);
    case LOSSLESS_INPUT:
        return new LosslessOutputQueue(speed, queuesize, *_eventlist, queueLogger);
    case LOSSLESS_INPUT_ECN: 
        return new LosslessOutputQueue(speed, memFromPkt(10000), *_eventlist, queueLogger,1,memFromPkt(16));
    case COMPOSITE_ECN:
        if (tor) 
            return new CompositeQueue(speed, queuesize, *_eventlist, queueLogger);
        else
            return new ECNQueue(speed, memFromPkt(2*SWITCH_BUFFER), *_eventlist, queueLogger, memFromPkt(15));
    case COMPOSITE_ECN_LB:
        {
            // if (_bdp == 0 || _bdp > queuesize ){
            //     cerr << "We need BDP to set the ecn mark threshold, now BDP value is " << _bdp << " \n";
            //     exit(1);                
            // }
            CompositeQueue* q = new CompositeQueue(speed, queuesize, *_eventlist, queueLogger);
            if (!tor || dir == UPLINK) {
                // don't use ECN on ToR downlinks
                if (_bdp == 0){
                    q->set_ecn_threshold(FatTreeSwitch::_ecn_threshold_fraction * queuesize);
                }else{
                    // q->set_ecn_threshold(FatTreeSwitch::_ecn_threshold_fraction * _bdp);
                    q->set_ecn_thresholds(37.5*1000, 100*1000);
                }

            }
            return q;
        }
    default:
        abort();
    }
}

void FatTreeTopology::init_network(){
    QueueLogger* queueLogger;

    if (_tiers == 3) {
        for (uint32_t j=0;j<NCORE;j++) {
            for (uint32_t k=0;k<NAGG;k++) {
                queues_nc_nup[j][k] = NULL;
                pipes_nc_nup[j][k] = NULL;
                queues_nup_nc[k][j] = NULL;
                pipes_nup_nc[k][j] = NULL;
            }
        }
    }
    
    for (uint32_t j=0;j<NAGG;j++) {
        for (uint32_t k=0;k<NTOR;k++) {
            queues_nup_nlp[j][k] = NULL;
            pipes_nup_nlp[j][k] = NULL;
            queues_nlp_nup[k][j] = NULL;
            pipes_nlp_nup[k][j] = NULL;
        }
    }
    
    for (uint32_t j=0;j<NTOR;j++) {
        for (uint32_t k=0;k<NSRV;k++) {
            queues_nlp_ns[j][k] = NULL;
            pipes_nlp_ns[j][k] = NULL;
            queues_ns_nlp[k][j] = NULL;
            pipes_ns_nlp[k][j] = NULL;
        }
    }

    //create switches if we have lossless operation
    //if (_qt==LOSSLESS)
    // changed to always create switches
    cout << "total switches: ToR " << NTOR << " NAGG " << NAGG << " NCORE " << NCORE << " srv_per_tor " << K/2 << endl;
    for (uint32_t j=0;j<NTOR;j++){
        switches_lp[j] = new FatTreeSwitch(*_eventlist, "Switch_LowerPod_"+ntoa(j),FatTreeSwitch::TOR,j,_switch_latency,this);
    }
    for (uint32_t j=0;j<NAGG;j++){
        switches_up[j] = new FatTreeSwitch(*_eventlist, "Switch_UpperPod_"+ntoa(j), FatTreeSwitch::AGG,j,_switch_latency,this);
    }
    for (uint32_t j=0;j<NCORE;j++){
        switches_c[j] = new FatTreeSwitch(*_eventlist, "Switch_Core_"+ntoa(j), FatTreeSwitch::CORE,j,_switch_latency,this);
    }
      
    // links from lower layer pod switch to server
    for (uint32_t tor = 0; tor < NTOR; tor++) {  
        for (uint32_t l = 0; l < K/2; l++) {
            uint32_t srv = tor * K/2 + l; 
            // Downlink
            if (_logger_factory) {
                queueLogger = _logger_factory->createQueueLogger();
            } else {
                queueLogger = NULL;
            }
            
            queues_nlp_ns[tor][srv] = alloc_queue(queueLogger, _queuesize, DOWNLINK, true);
            queues_nlp_ns[tor][srv]->setName("LS" + ntoa(tor) + "->DST" +ntoa(srv));
            //if (logfile) logfile->writeName(*(queues_nlp_ns[tor][srv]));

            pipes_nlp_ns[tor][srv] = new Pipe(_hop_latency, *_eventlist);
            pipes_nlp_ns[tor][srv]->setName("Pipe-LS" + ntoa(tor)  + "->DST" + ntoa(srv));
            //if (logfile) logfile->writeName(*(pipes_nlp_ns[tor][srv]));
            
            // Uplink
            if (_logger_factory) {
                queueLogger = _logger_factory->createQueueLogger();
            } else {
                queueLogger = NULL;
            }
            queues_ns_nlp[srv][tor] = alloc_src_queue(queueLogger);   
            queues_ns_nlp[srv][tor]->setName("SRC" + ntoa(srv) + "->LS" +ntoa(tor));
            //if (logfile) logfile->writeName(*(queues_ns_nlp[srv][tor]));

            queues_ns_nlp[srv][tor]->setRemoteEndpoint(switches_lp[tor]);

            switches_lp[tor]->addPort(queues_nlp_ns[tor][srv]);

            /*if (_qt==LOSSLESS){
              ((LosslessQueue*)queues_nlp_ns[tor][srv])->setRemoteEndpoint(queues_ns_nlp[srv][tor]);
              }else */
            if (_qt==LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN){
                //no virtual queue needed at server
                new LosslessInputQueue(*_eventlist,queues_ns_nlp[srv][tor],switches_lp[tor]);
            }
        
            pipes_ns_nlp[srv][tor] = new Pipe(_hop_latency, *_eventlist);
            pipes_ns_nlp[srv][tor]->setName("Pipe-SRC" + ntoa(srv) + "->LS" + ntoa(tor));
            //if (logfile) logfile->writeName(*(pipes_ns_nlp[srv][tor]));
            
            if (ff){
                ff->add_queue(queues_nlp_ns[tor][srv]);
                ff->add_queue(queues_ns_nlp[srv][tor]);
            }
        }
    }

    /*    for (uint32_t i = 0;i<NSRV;i++){
          for (uint32_t j = 0;j<NK;j++){
          printf("%p/%p ",queues_ns_nlp[i][j], queues_nlp_ns[j][i]);
          }
          printf("\n");
          }*/
    
    //Lower layer in pod to upper layer in pod!
    for (uint32_t tor = 0; tor < NTOR; tor++) {
        uint32_t podid = 2*tor/K;
        uint32_t agg_min, agg_max;
        if (_tiers == 3) {
            //Connect the lower layer switch to the upper layer switches in the same pod
            agg_min = MIN_POD_ID(podid);
            agg_max = MAX_POD_ID(podid);
        } else {
            //Connect the lower layer switch to all upper layer switches
            assert(_tiers == 2);
            agg_min = 0;
            agg_max = NAGG-1;
        }
        for (uint32_t agg=agg_min; agg<=agg_max; agg++){
            // Downlink
            if (_logger_factory) {
                queueLogger = _logger_factory->createQueueLogger();
            } else {
                queueLogger = NULL;
            }
            queues_nup_nlp[agg][tor] = alloc_queue(queueLogger, _queuesize, DOWNLINK);
            queues_nup_nlp[agg][tor]->setName("US" + ntoa(agg) + "->LS_" + ntoa(tor));
            //if (logfile) logfile->writeName(*(queues_nup_nlp[agg][tor]));
            
            pipes_nup_nlp[agg][tor] = new Pipe(_hop_latency, *_eventlist);
            pipes_nup_nlp[agg][tor]->setName("Pipe-US" + ntoa(agg) + "->LS" + ntoa(tor));
            //if (logfile) logfile->writeName(*(pipes_nup_nlp[agg][tor]));
            
            // Uplink
            if (_logger_factory) {
                queueLogger = _logger_factory->createQueueLogger();
            } else {
                queueLogger = NULL;
            }
            queues_nlp_nup[tor][agg] = alloc_queue(queueLogger, _queuesize, UPLINK, true);
            queues_nlp_nup[tor][agg]->setName("LS" + ntoa(tor) + "->US" + ntoa(agg));
            //if (logfile) logfile->writeName(*(queues_nlp_nup[tor][agg]));

            switches_lp[tor]->addPort(queues_nlp_nup[tor][agg]);
            switches_up[agg]->addPort(queues_nup_nlp[agg][tor]);
            queues_nlp_nup[tor][agg]->setRemoteEndpoint(switches_up[agg]);
            queues_nup_nlp[agg][tor]->setRemoteEndpoint(switches_lp[tor]);

            /*if (_qt==LOSSLESS){
              ((LosslessQueue*)queues_nlp_nup[tor][agg])->setRemoteEndpoint(queues_nup_nlp[agg][tor]);
              ((LosslessQueue*)queues_nup_nlp[agg][tor])->setRemoteEndpoint(queues_nlp_nup[tor][agg]);
              }else */
            if (_qt==LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN){            
                new LosslessInputQueue(*_eventlist, queues_nlp_nup[tor][agg],switches_up[agg]);
                new LosslessInputQueue(*_eventlist, queues_nup_nlp[agg][tor],switches_lp[tor]);
            }
        
            pipes_nlp_nup[tor][agg] = new Pipe(_hop_latency, *_eventlist);
            pipes_nlp_nup[tor][agg]->setName("Pipe-LS" + ntoa(tor) + "->US" + ntoa(agg));
            //if (logfile) logfile->writeName(*(pipes_nlp_nup[tor][agg]));
        
            if (ff){
                ff->add_queue(queues_nlp_nup[tor][agg]);
                ff->add_queue(queues_nup_nlp[agg][tor]);
            }
        }
    }

    /*for (int32_t i = 0;i<NK;i++){
      for (uint32_t j = 0;j<NK;j++){
      printf("%p/%p ",queues_nlp_nup[i][j], queues_nup_nlp[j][i]);
      }
      printf("\n");
      }*/
    
    // Upper layer in pod to core!
    if (_tiers == 3) {
        for (uint32_t agg = 0; agg < NAGG; agg++) {
            uint32_t podpos = agg%(K/2);
            for (uint32_t l = 0; l < K/2; l++) {
                uint32_t core = podpos * K/2 + l;
                // Downlink
                if (_logger_factory) {
                    queueLogger = _logger_factory->createQueueLogger();
                } else {
                    queueLogger = NULL;
                }

                queues_nup_nc[agg][core] = alloc_queue(queueLogger, _queuesize, UPLINK);
                queues_nup_nc[agg][core]->setName("US" + ntoa(agg) + "->CS" + ntoa(core));
                //if (logfile) logfile->writeName(*(queues_nup_nc[agg][core]));
        
                pipes_nup_nc[agg][core] = new Pipe(_hop_latency, *_eventlist);
                pipes_nup_nc[agg][core]->setName("Pipe-US" + ntoa(agg) + "->CS" + ntoa(core));
                //if (logfile) logfile->writeName(*(pipes_nup_nc[agg][core]));
        
                // Uplink
                if (_logger_factory) {
                    queueLogger = _logger_factory->createQueueLogger();
                } else {
                    queueLogger = NULL;
                }
        
                if ((l+agg*K/2)<failed_links){
                    queues_nc_nup[core][agg] = alloc_queue(queueLogger,_linkspeed/10, _queuesize,
                                                           DOWNLINK, false);
                    cout << "Adding link failure for agg_sw " << ntoa(agg) << " l " << ntoa(l) << endl;
                } else {
                    queues_nc_nup[core][agg] = alloc_queue(queueLogger, _queuesize, DOWNLINK);
                }
        
                queues_nc_nup[core][agg]->setName("CS" + ntoa(core) + "->US" + ntoa(agg));

                switches_up[agg]->addPort(queues_nup_nc[agg][core]);
                switches_c[core]->addPort(queues_nc_nup[core][agg]);
                queues_nup_nc[agg][core]->setRemoteEndpoint(switches_c[core]);
                queues_nc_nup[core][agg]->setRemoteEndpoint(switches_up[agg]);

                /*if (_qt==LOSSLESS){
                  ((LosslessQueue*)queues_nup_nc[agg][core])->setRemoteEndpoint(queues_nc_nup[core][agg]);
                  ((LosslessQueue*)queues_nc_nup[core][agg])->setRemoteEndpoint(queues_nup_nc[agg][core]);
                  }
                  else*/
                if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN){
                    new LosslessInputQueue(*_eventlist, queues_nup_nc[agg][core],switches_c[core]);
                    new LosslessInputQueue(*_eventlist, queues_nc_nup[core][agg],switches_up[agg]);
                }
                //if (logfile) logfile->writeName(*(queues_nc_nup[core][agg]));
            
                pipes_nc_nup[core][agg] = new Pipe(_hop_latency, *_eventlist);
                pipes_nc_nup[core][agg]->setName("Pipe-CS" + ntoa(core) + "->US" + ntoa(agg));
                //if (logfile) logfile->writeName(*(pipes_nc_nup[core][agg]));
            
                if (ff){
                    ff->add_queue(queues_nup_nc[agg][core]);
                    ff->add_queue(queues_nc_nup[core][agg]);
                }
            }
        }
    }

    /*    for (uint32_t i = 0;i<NK;i++){
          for (uint32_t j = 0;j<NC;j++){
          printf("%p/%p ",queues_nup_nc[i][agg], queues_nc_nup[agg][i]);
          }
          printf("\n");
          }*/
    
    //init thresholds for lossless operation
    if (_qt==LOSSLESS) {
        for (uint32_t j=0;j<NTOR;j++){
            switches_lp[j]->configureLossless();
        }
        for (uint32_t j=0;j<NAGG;j++){
            switches_up[j]->configureLossless();
        }
        for (uint32_t j=0;j<NCORE;j++){
            switches_c[j]->configureLossless();
        }
    }
    // report_stats();
}

void FatTreeTopology::add_failed_link(uint32_t type, uint32_t switch_id, uint32_t link_id){
    assert(type == FatTreeSwitch::AGG);
    assert(link_id < getK()/2);
    assert(switch_id < NAGG);
    
    uint32_t podpos = switch_id%(getK()/2);
    uint32_t k = podpos * getK()/2 + link_id;
    
    assert(queues_nup_nc[switch_id][k]!=NULL && queues_nc_nup[k][switch_id]!=NULL );
    queues_nup_nc[switch_id][k] = NULL;
    queues_nc_nup[k][switch_id] = NULL;

    assert(pipes_nup_nc[switch_id][k]!=NULL && pipes_nc_nup[k][switch_id]);
    pipes_nup_nc[switch_id][k] = NULL;
    pipes_nc_nup[k][switch_id] = NULL;
}


vector<const Route*>* FatTreeTopology::get_bidir_paths(uint32_t src, uint32_t dest, bool reverse){
    vector<const Route*>* paths = new vector<const Route*>();

    route_t *routeout, *routeback;
  
    //QueueLoggerSimple *simplequeuelogger = new QueueLoggerSimple();
    //QueueLoggerSimple *simplequeuelogger = 0;
    //logfile->addLogger(*simplequeuelogger);
    //Queue* pqueue = new Queue(_linkspeed, memFromPkt(FEEDER_BUFFER), *_eventlist, simplequeuelogger);
    //pqueue->setName("PQueue_" + ntoa(src) + "_" + ntoa(dest));
    //logfile->writeName(*pqueue);
    if (HOST_POD_SWITCH(src)==HOST_POD_SWITCH(dest)){
  
        // forward path
        routeout = new Route();
        //routeout->push_back(pqueue);
        routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);
        routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);

        if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
            routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]->getRemoteEndpoint());

        routeout->push_back(queues_nlp_ns[HOST_POD_SWITCH(dest)][dest]);
        routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);

        if (reverse) {
            // reverse path for RTS packets
            routeback = new Route();
            routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)]);
            routeback->push_back(pipes_ns_nlp[dest][HOST_POD_SWITCH(dest)]);

            if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)]->getRemoteEndpoint());

            routeback->push_back(queues_nlp_ns[HOST_POD_SWITCH(src)][src]);
            routeback->push_back(pipes_nlp_ns[HOST_POD_SWITCH(src)][src]);

            routeout->set_reverse(routeback);
            routeback->set_reverse(routeout);
        }

        //print_route(*routeout);
        paths->push_back(routeout);

        check_non_null(routeout);
        return paths;
    }
    else if (HOST_POD(src)==HOST_POD(dest)){
        //don't go up the hierarchy, stay in the pod only.

        uint32_t pod = HOST_POD(src);
        //there are K/2 paths between the source and the destination
        if (_tiers == 2) {
            // xxx sanity check for debugging, remove later.
            assert(MIN_POD_ID(pod) == 0);
            assert(MAX_POD_ID(pod) == NAGG - 1);
        }
        for (uint32_t upper = MIN_POD_ID(pod);upper <= MAX_POD_ID(pod); upper++){
            //upper is nup
      
            routeout = new Route();
            //routeout->push_back(pqueue);
      
            routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);
            routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);

            if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]->getRemoteEndpoint());

            routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][upper]);
            routeout->push_back(pipes_nlp_nup[HOST_POD_SWITCH(src)][upper]);

            if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][upper]->getRemoteEndpoint());

            routeout->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(dest)]);
            routeout->push_back(pipes_nup_nlp[upper][HOST_POD_SWITCH(dest)]);

            if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                routeout->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(dest)]->getRemoteEndpoint());

            routeout->push_back(queues_nlp_ns[HOST_POD_SWITCH(dest)][dest]);
            routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);

            if (reverse) {
                // reverse path for RTS packets
                routeback = new Route();
      
                routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)]);
                routeback->push_back(pipes_ns_nlp[dest][HOST_POD_SWITCH(dest)]);

                if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                    routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)]->getRemoteEndpoint());

                routeback->push_back(queues_nlp_nup[HOST_POD_SWITCH(dest)][upper]);
                routeback->push_back(pipes_nlp_nup[HOST_POD_SWITCH(dest)][upper]);

                if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                    routeback->push_back(queues_nlp_nup[HOST_POD_SWITCH(dest)][upper]->getRemoteEndpoint());

                routeback->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(src)]);
                routeback->push_back(pipes_nup_nlp[upper][HOST_POD_SWITCH(src)]);

                if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                    routeback->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(src)]->getRemoteEndpoint());
      
                routeback->push_back(queues_nlp_ns[HOST_POD_SWITCH(src)][src]);
                routeback->push_back(pipes_nlp_ns[HOST_POD_SWITCH(src)][src]);

                routeout->set_reverse(routeback);
                routeback->set_reverse(routeout);
            }
      
            // print_route(*routeout); 
            paths->push_back(routeout);
            check_non_null(routeout);
        }
        return paths;
    } else {
        assert(_tiers == 3);
        uint32_t pod = HOST_POD(src);

        for (uint32_t upper = MIN_POD_ID(pod);upper <= MAX_POD_ID(pod); upper++)
            for (uint32_t core = (upper%(K/2)) * K / 2; core < ((upper % (K/2)) + 1)*K/2; core++){
                //upper is nup
        
                routeout = new Route();
                //routeout->push_back(pqueue);
        
                routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);
                routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);

                if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                    routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]->getRemoteEndpoint());
        
                routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][upper]);
                routeout->push_back(pipes_nlp_nup[HOST_POD_SWITCH(src)][upper]);

                if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                    routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][upper]->getRemoteEndpoint());
        
                routeout->push_back(queues_nup_nc[upper][core]);
                routeout->push_back(pipes_nup_nc[upper][core]);

                if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                    routeout->push_back(queues_nup_nc[upper][core]->getRemoteEndpoint());
        
                //now take the only link down to the destination server!
        
                uint32_t upper2 = HOST_POD(dest)*K/2 + 2 * core / K;
                //printf("K %d HOST_POD(%d) %d core %d upper2 %d\n",K,dest,HOST_POD(dest),core, upper2);
        
                routeout->push_back(queues_nc_nup[core][upper2]);
                routeout->push_back(pipes_nc_nup[core][upper2]);

                if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                    routeout->push_back(queues_nc_nup[core][upper2]->getRemoteEndpoint());        

                routeout->push_back(queues_nup_nlp[upper2][HOST_POD_SWITCH(dest)]);
                routeout->push_back(pipes_nup_nlp[upper2][HOST_POD_SWITCH(dest)]);

                if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                    routeout->push_back(queues_nup_nlp[upper2][HOST_POD_SWITCH(dest)]->getRemoteEndpoint());
        
                routeout->push_back(queues_nlp_ns[HOST_POD_SWITCH(dest)][dest]);
                routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);

                if (reverse) {
                    // reverse path for RTS packets
                    routeback = new Route();
        
                    routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)]);
                    routeback->push_back(pipes_ns_nlp[dest][HOST_POD_SWITCH(dest)]);

                    if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                        routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)]->getRemoteEndpoint());
        
                    routeback->push_back(queues_nlp_nup[HOST_POD_SWITCH(dest)][upper2]);
                    routeback->push_back(pipes_nlp_nup[HOST_POD_SWITCH(dest)][upper2]);

                    if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                        routeback->push_back(queues_nlp_nup[HOST_POD_SWITCH(dest)][upper2]->getRemoteEndpoint());
        
                    routeback->push_back(queues_nup_nc[upper2][core]);
                    routeback->push_back(pipes_nup_nc[upper2][core]);

                    if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                        routeback->push_back(queues_nup_nc[upper2][core]->getRemoteEndpoint());
        
                    //now take the only link back down to the src server!
        
                    routeback->push_back(queues_nc_nup[core][upper]);
                    routeback->push_back(pipes_nc_nup[core][upper]);

                    if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                        routeback->push_back(queues_nc_nup[core][upper]->getRemoteEndpoint());
        
                    routeback->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(src)]);
                    routeback->push_back(pipes_nup_nlp[upper][HOST_POD_SWITCH(src)]);

                    if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                        routeback->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(src)]->getRemoteEndpoint());
        
                    routeback->push_back(queues_nlp_ns[HOST_POD_SWITCH(src)][src]);
                    routeback->push_back(pipes_nlp_ns[HOST_POD_SWITCH(src)][src]);


                    routeout->set_reverse(routeback);
                    routeback->set_reverse(routeout);
                }
        
                //print_route(*routeout);
                paths->push_back(routeout);
                check_non_null(routeout);
            }
        return paths;
    }
}

void FatTreeTopology::count_queue(Queue* queue){
    if (_link_usage.find(queue)==_link_usage.end()){
        _link_usage[queue] = 0;
    }

    _link_usage[queue] = _link_usage[queue] + 1;
}

int64_t FatTreeTopology::find_lp_switch(Queue* queue){
    //first check ns_nlp
    for (uint32_t srv=0;srv<NSRV;srv++)
        for (uint32_t tor = 0; tor < NTOR; tor++)
            if (queues_ns_nlp[srv][tor] == queue)
                return tor;

    //only count nup to nlp
    count_queue(queue);

    for (uint32_t agg=0;agg<NAGG;agg++)
        for (uint32_t tor = 0; tor < NTOR; tor++)
            if (queues_nup_nlp[agg][tor] == queue)
                return tor;

    return -1;
}

int64_t FatTreeTopology::find_up_switch(Queue* queue){
    count_queue(queue);
    //first check nc_nup
    for (uint32_t core=0; core < NCORE; core++)
        for (uint32_t agg = 0; agg < NAGG; agg++)
            if (queues_nc_nup[core][agg] == queue)
                return agg;

    //check nlp_nup
    for (uint32_t tor=0; tor < NTOR; tor++)
        for (uint32_t agg = 0; agg < NAGG; agg++)
            if (queues_nlp_nup[tor][agg] == queue)
                return agg;

    return -1;
}

int64_t FatTreeTopology::find_core_switch(Queue* queue){
    count_queue(queue);
    //first check nup_nc
    for (uint32_t agg=0;agg<NAGG;agg++)
        for (uint32_t core = 0;core<NCORE;core++)
            if (queues_nup_nc[agg][core]==queue)
                return core;

    return -1;
}

int64_t FatTreeTopology::find_destination(Queue* queue){
    //first check nlp_ns
    for (uint32_t tor=0; tor<NTOR; tor++)
        for (uint32_t srv = 0; srv<NSRV; srv++)
            if (queues_nlp_ns[tor][srv]==queue)
                return srv;

    return -1;
}

void FatTreeTopology::print_path(std::ofstream &paths,uint32_t src,const Route* route){
    paths << "SRC_" << src << " ";
  
    if (route->size()/2==2){
        paths << "LS_" << find_lp_switch((Queue*)route->at(1)) << " ";
        paths << "DST_" << find_destination((Queue*)route->at(3)) << " ";
    } else if (route->size()/2==4){
        paths << "LS_" << find_lp_switch((Queue*)route->at(1)) << " ";
        paths << "US_" << find_up_switch((Queue*)route->at(3)) << " ";
        paths << "LS_" << find_lp_switch((Queue*)route->at(5)) << " ";
        paths << "DST_" << find_destination((Queue*)route->at(7)) << " ";
    } else if (route->size()/2==6){
        paths << "LS_" << find_lp_switch((Queue*)route->at(1)) << " ";
        paths << "US_" << find_up_switch((Queue*)route->at(3)) << " ";
        paths << "CS_" << find_core_switch((Queue*)route->at(5)) << " ";
        paths << "US_" << find_up_switch((Queue*)route->at(7)) << " ";
        paths << "LS_" << find_lp_switch((Queue*)route->at(9)) << " ";
        paths << "DST_" << find_destination((Queue*)route->at(11)) << " ";
    } else {
        paths << "Wrong hop count " << ntoa(route->size()/2);
    }
  
    paths << endl;
}

void FatTreeTopology::add_switch_loggers(Logfile& log, simtime_picosec sample_period) {
    for (uint32_t i = 0; i < NTOR; i++) {
        switches_lp[i]->add_logger(log, sample_period);
    }
    for (uint32_t i = 0; i < NAGG; i++) {
        switches_up[i]->add_logger(log, sample_period);
    }
    for (uint32_t i = 0; i < NCORE
             ; i++) {
        switches_c[i]->add_logger(log, sample_period);
    }
}


void FatTreeTopology::report_stats(){
    cout << "queues_nup_nlp " << queues_nup_nlp.size() <<endl;
    cout << "queues_nlp_nup " << queues_nlp_nup.size() << endl;
    cout << "queues_nc_nup " << queues_nc_nup.size() << endl;
    cout << "queues_nup_nc " << queues_nup_nc.size() << endl;

    int _max_queue = 0;
    CompositeQueue *max_qdepth_q = NULL;
    int index_j = 0;
    int queue_type = -1;
    //Lower layer in pod to upper layer in pod!
    for (uint32_t tor = 0; tor < NTOR; tor++) {
        uint32_t podid = 2*tor/K;
        uint32_t agg_min, agg_max;
        if (_tiers == 3) {
            //Connect the lower layer switch to the upper layer switches in the same pod
            agg_min = MIN_POD_ID(podid);
            agg_max = MAX_POD_ID(podid);
        } else {
            //Connect the lower layer switch to all upper layer switches
            assert(_tiers == 2);
            agg_min = 0;
            agg_max = NAGG-1;
        }
        for (uint32_t agg=agg_min; agg<=agg_max; agg++){
            CompositeQueue *q = dynamic_cast<CompositeQueue*>(queues_nup_nlp[agg][tor]);
            if(q->_hightest_qdepth > _max_queue){
                max_qdepth_q = q;
                _max_queue = q->_hightest_qdepth;
            }
            cout<< q->nodename() << " _queue_id " << q->_queue_id << " max_queue "<< q->_hightest_qdepth << endl;
            q = dynamic_cast<CompositeQueue*>(queues_nlp_nup[tor][agg]);

            if(q->_hightest_qdepth > _max_queue){
                max_qdepth_q = q;
                _max_queue = q->_hightest_qdepth;
            }
            cout<< q->nodename() << " _queue_id " << q->_queue_id << " max_queue "<< q->_hightest_qdepth << endl;
            
        }
    }
    // Upper layer in pod to core!
    if (_tiers == 3) {
        for (uint32_t agg = 0; agg < NAGG; agg++) {
            uint32_t podpos = agg%(K/2);
            for (uint32_t l = 0; l < K/2; l++) {
                uint32_t core = podpos * K/2 + l;
                CompositeQueue *q = dynamic_cast<CompositeQueue*>(queues_nup_nc[agg][core]);
                if (q->_hightest_qdepth > _max_queue)
                {
                    max_qdepth_q = q;
                    _max_queue = q->_hightest_qdepth;
                }
                cout<< q->nodename()<< " _queue_id " << q->_queue_id << " max_queue "<< q->_hightest_qdepth << endl;
                q = dynamic_cast<CompositeQueue*>(queues_nc_nup[core][agg]);
                if (q->_hightest_qdepth > _max_queue)
                {
                    max_qdepth_q = q;
                    _max_queue = q->_hightest_qdepth;
                }
                cout<< q->nodename()<< " _queue_id " << q->_queue_id << " max_queue "<< q->_hightest_qdepth << endl;
            }
        }
    }
    for (uint32_t tor = 0; tor < NTOR; tor++) {  
        for (uint32_t l = 0; l < K/2; l++) {
            uint32_t srv = tor * K/2 + l; 
            CompositeQueue *q = dynamic_cast<CompositeQueue*>(queues_nlp_ns[tor][srv]);
            simtime_picosec duration = q->_last_packet_time - q->_first_packet_time;
            if(duration > 0){
                float pps = q->_num_pkts / (duration / 1000000.0);
                cout << "rx " << q->nodename() << " pps(M) " << pps << endl;
            }
            FairPriorityQueue *srv_q = dynamic_cast<FairPriorityQueue*>(queues_ns_nlp[srv][tor]);
            duration = srv_q->_last_packet_time - srv_q->_first_packet_time;
            if (duration > 0){
                float pps = srv_q->_num_pkts / (duration / 1000000.0);
                cout << "tx " << srv_q->nodename() << " pps(M) " << pps << endl;
            }
        }
    }

    for (auto i : max_qdepth_q->_flow_counts)    // auto keyword 
		cout <<"worst_queue "<< max_qdepth_q->nodename() <<" flow_id " <<  i.first << " pkts " << i.second << endl;
}
