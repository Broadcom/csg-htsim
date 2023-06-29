// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "config.h"
#include <sstream>

#include <iostream>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "network.h"
#include "randomqueue.h"
#include "queue_lossless_input.h"
#include "shortflows.h"
#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "clock.h"
#include "roce.h"
#include "compositequeue.h"
#include "firstfit.h"
#include "topology.h"
#include "connection_matrix.h"

#include "fat_tree_topology.h"
#include "fat_tree_switch.h"

#include <list>

// Simulation params

#define PRINT_PATHS 1

#define PERIODIC 0
#include "main.h"

uint32_t RTT = 1; // this is per link delay in us; identical RTT microseconds = 0.02 ms
int DEFAULT_NODES = 432;
#define DEFAULT_QUEUE_SIZE 15

//#define SWITCH_BUFFER (SERVICE * RTT / 1000)
#define USE_FIRST_FIT 0
#define FIRST_FIT_INTERVAL 100

EventList eventlist;

void exit_error(char* progr) {
    cout << "Usage " << progr << " [-nodes N]\n\t[-conns C]\n\t[-q queue_size]\n\t[-queue_type composite|random|lossless|lossless_input|]\n\t[-tm traffic_matrix_file]\n\t[-strat route_strategy (single,\n\tecmp_host,ecmp_ar,\n\tecmp_host_ar ar_thresh)]\n\t[-log log_level]\n\t[-seed random_seed]\n\t[-end end_time_in_usec]\n\t[-mtu MTU]\n\t[-hop_latency x] per hop wire latency in us,default 1\n\t[-switch_latency x] switching latency in us, default 0\n\t[-start_delta] time in us to randomly delay the start of connections\n\t[-pfc_thresholds low high]" << endl;
    exit(1);
}

int main(int argc, char **argv) {
    Clock c(timeFromSec(5 / 100.), eventlist);
    mem_b queuesize = DEFAULT_QUEUE_SIZE;
    linkspeed_bps linkspeed = speedFromMbps((double)HOST_NIC);
    int packet_size = 9000;
    uint32_t path_entropy_size = 10000000;
    uint32_t no_of_conns = 0, no_of_nodes = DEFAULT_NODES;
    uint32_t tiers = 3; // we support 2 and 3 tier fattrees     
    double logtime = 0.25; // ms;
    stringstream filename(ios_base::out);
    simtime_picosec hop_latency = timeFromUs((uint32_t)1);
    simtime_picosec switch_latency = timeFromUs((uint32_t)0);
    simtime_picosec start_delta = 0;
    queue_type qt = LOSSLESS_INPUT;
    float ar_sticky_delta = 10;

    queue_type snd_type = FAIR_PRIO;

    uint64_t high_pfc = 15, low_pfc = 12;

    bool log_sink = false;
    bool log_tor_downqueue = false;
    bool log_tor_upqueue = false;
    bool log_traffic = false;
    bool log_switches = false;
    bool log_queue_usage = false;
    RouteStrategy route_strategy = NOT_SET;
    int seed = 13;
    int i = 1;
    filename << "logout.dat";
    int end_time = 1000;//in microseconds

    char* tm_file = NULL;
    char* topo_file = NULL;

    while (i<argc) {
        if (!strcmp(argv[i],"-o")) {
            filename.str(std::string());
            filename << argv[i+1];
            i++;
        } else if (!strcmp(argv[i],"-conns")) {
            no_of_conns = atoi(argv[i+1]);
            cout << "no_of_conns "<<no_of_conns << endl;
            i++;
        } else if (!strcmp(argv[i],"-end")) {
            end_time = atoi(argv[i+1]);
            cout << "endtime(us) "<< end_time << endl;
            i++;            
        } else if (!strcmp(argv[i],"-nodes")) {
            no_of_nodes = atoi(argv[i+1]);
            cout << "no_of_nodes "<<no_of_nodes << endl;
            i++;
        } else if (!strcmp(argv[i],"-tiers")) {
            tiers = atoi(argv[i+1]);
            cout << "tiers "<< tiers << endl;
            assert(tiers == 2 || tiers == 3);
            i++;
        } else if (!strcmp(argv[i],"-queue_type")) {
            if (!strcmp(argv[i+1], "composite")) {
                qt = COMPOSITE;
            } 
            else if (!strcmp(argv[i+1], "composite_ecn")) {
                qt = COMPOSITE_ECN;
            }
            else if (!strcmp(argv[i+1], "lossless")) {
                qt = LOSSLESS;
            }
            else if (!strcmp(argv[i+1], "lossless_input")) {
                qt = LOSSLESS_INPUT;
            }
            else {
                cout << "Unknown queue type " << argv[i+1] << endl;
                exit_error(argv[0]);
            }
            cout << "queue_type "<< qt << endl;
            i++;
        } else if (!strcmp(argv[i],"-host_queue_type")) {
            if (!strcmp(argv[i+1], "swift")) {
                snd_type = SWIFT_SCHEDULER;
            } 
            else if (!strcmp(argv[i+1], "prio")) {
                snd_type = PRIORITY;
            }
            else if (!strcmp(argv[i+1], "fair_prio")) {
                snd_type = FAIR_PRIO;
            }
            else {
                cout << "Unknown host queue type " << argv[i+1] << " expecting one of swift|prio|fair_prio" << endl;
                exit_error(argv[0]);
            }
            cout << "host queue_type "<< snd_type << endl;
            i++;
        } else if (!strcmp(argv[i],"-log")){
            if (!strcmp(argv[i+1], "sink")) {
                log_sink = true;
            } else if (!strcmp(argv[i+1], "sink")) {
                cout << "logging sinks\n";
                log_sink = true;
            } else if (!strcmp(argv[i+1], "tor_downqueue")) {
                cout << "logging tor downqueues\n";
                log_tor_downqueue = true;
            } else if (!strcmp(argv[i+1], "tor_upqueue")) {
                cout << "logging tor upqueues\n";
                log_tor_upqueue = true;
            } else if (!strcmp(argv[i+1], "switch")) {
                cout << "logging total switch queues\n";
                log_switches = true;
            } else if (!strcmp(argv[i+1], "traffic")) {
                cout << "logging traffic\n";
                log_traffic = true;
            } else if (!strcmp(argv[i+1], "queue_usage")) {
                cout << "logging queue usage\n";
                log_queue_usage = true;
            } else {
                exit_error(argv[0]);
            }
            i++;
        } else if (!strcmp(argv[i],"-tm")){
            tm_file = argv[i+1];
            cout << "traffic matrix input file: "<< tm_file << endl;
            i++;
        } else if (!strcmp(argv[i],"-topo")){
            topo_file = argv[i+1];
            cout << "FatTree topology input file: "<< topo_file << endl;
            i++;
        } else if (!strcmp(argv[i],"-q")){
            queuesize = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-logtime")){
            logtime = atof(argv[i+1]);            
            cout << "logtime "<< logtime << " ms" << endl;
            i++;
        } else if (!strcmp(argv[i],"-linkspeed")){
            // linkspeed specified is in Mbps
            linkspeed = speedFromMbps(atof(argv[i+1]));
            i++;
        } else if (!strcmp(argv[i],"-seed")){
            seed = atoi(argv[i+1]);
            cout << "random seed "<< seed << endl;
            i++;
        } else if (!strcmp(argv[i],"-mtu")){
            packet_size = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-paths")){
            path_entropy_size = atoi(argv[i+1]);
            cout << "no of paths " << path_entropy_size << endl;
            i++;
        } else if (!strcmp(argv[i],"-hop_latency")){
            hop_latency = timeFromUs(atof(argv[i+1]));
            cout << "Hop latency set to " << timeAsUs(hop_latency) << endl;
            i++;
        } else if (!strcmp(argv[i],"-switch_latency")){
            switch_latency = timeFromUs(atof(argv[i+1]));
            cout << "Switch latency set to " << timeAsUs(hop_latency) << endl;
            i++;
        } else if (!strcmp(argv[i],"-start_delta")){
            start_delta = atof(argv[i+1]);
            cout << "Start connectios with a random delay of upto " << start_delta << "us" << endl;
            i++;
        } else if (!strcmp(argv[i],"-ar_sticky_delta")){
            ar_sticky_delta = atof(argv[i+1]);
            cout << "Adaptive routing sticky delta " << ar_sticky_delta << "us" << endl;
            i++;
        }
         else if (!strcmp(argv[i],"-pfc_thresholds")){
            low_pfc = atoi(argv[i+1]);
            high_pfc = atoi(argv[i+2]);
            cout << "PFC thresholds high " << high_pfc << " low " << low_pfc << endl;
            i+=2;
        } else if (!strcmp(argv[i],"-ar_method")){
            if (!strcmp(argv[i+1],"pause")){
                cout << "Adaptive routing based on pause state " << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_pause;
            }
            else if (!strcmp(argv[i+1],"queue")){
                cout << "Adaptive routing based on queue size " << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_queuesize;
            }
            else if (!strcmp(argv[i+1],"bandwidth")){
                cout << "Adaptive routing based on bandwidth utilization " << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_bandwidth;
            }
            else if (!strcmp(argv[i+1],"flowcount")){
                cout << "Adaptive routing based on bandwidth utilization " << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_flow_count;
            }
            else if (!strcmp(argv[i+1],"pqb")){
                cout << "Adaptive routing based on pause, queuesize and bandwidth utilization " << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_pqb;
            }
            else if (!strcmp(argv[i+1],"pq")){
                cout << "Adaptive routing based on pause, queuesize" << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_pq;
            }
            else if (!strcmp(argv[i+1],"pb")){
                cout << "Adaptive routing based on pause, bandwidth utilization" << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_pb;
            }
            else if (!strcmp(argv[i+1],"qb")){
                cout << "Adaptive routing based on queuesize, bandwidth utilization" << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_qb; 
            }
            else {
                cout << "Unknown AR method expecting one of pause, queue, bandwidth, pqb, pq, pb, qb" << endl;
                exit(1);
            }
            i++;
        }  else if (!strcmp(argv[i],"-strat")){
            if (!strcmp(argv[i+1], "perm")) {
                route_strategy = SCATTER_PERMUTE;
            } else if (!strcmp(argv[i+1], "rand")) {
                route_strategy = SCATTER_RANDOM;
            } else if (!strcmp(argv[i+1], "ecmp")) {
                route_strategy = SCATTER_ECMP;
            } else if (!strcmp(argv[i+1], "pull")) {
                route_strategy = PULL_BASED;
            } else if (!strcmp(argv[i+1], "single")) {
                route_strategy = SINGLE_PATH;
            } else if (!strcmp(argv[i+1], "ecmp_host")) {
                route_strategy = ECMP_FIB;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
            } else if (!strcmp(argv[i+1], "ecmp_ar")) {
                route_strategy = ECMP_FIB;
                path_entropy_size = 1;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ADAPTIVE_ROUTING);
            } else if (!strcmp(argv[i+1], "ecmp_host_ar")) {
                route_strategy = ECMP_FIB;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP_ADAPTIVE);
                FatTreeSwitch::set_ar_fraction(atoi(argv[i+2]));
                cout << "AR fraction: " << atoi(argv[i+2]) << endl;
                i++;
            } else if (!strcmp(argv[i+1], "ecmp_rr")) {
                route_strategy = ECMP_FIB;
                path_entropy_size = 1;
                FatTreeSwitch::set_strategy(FatTreeSwitch::RR);
            }
            i++;
        } else {
            exit_error(argv[0]);
        }
                
        i++;
    }

    srand(seed);
    srandom(seed);

    cout << "Parsed args\n";
    Packet::set_packet_size(packet_size);

    FatTreeSwitch::_ar_sticky = FatTreeSwitch::PER_FLOWLET;
    FatTreeSwitch::_sticky_delta = timeFromUs(ar_sticky_delta);

    LosslessInputQueue::_high_threshold = Packet::data_packet_size()*high_pfc;
    LosslessInputQueue::_low_threshold = Packet::data_packet_size()*low_pfc;

    eventlist.setEndtime(timeFromUs((uint32_t)end_time));
    queuesize = memFromPkt(queuesize);
    
    switch (route_strategy) {
    case ECMP_FIB:
    case SCATTER_ECMP:
        if (path_entropy_size > 10000) {
            fprintf(stderr, "Route Strategy is ECMP.  Must specify path count using -paths\n");
            exit(1);
        }
        break;
    case SINGLE_PATH:
        if (path_entropy_size < 10000 && path_entropy_size > 1) {
            fprintf(stderr, "Route Strategy is SINGLE_PATH, but multiple paths are specifiec using -paths\n");
            exit(1);
        }
        break;
    case NOT_SET:
        fprintf(stderr, "Route Strategy not set.  Use the -strat param.  \nValid values are perm, rand, pull, rg and single\n");
        exit(1);
    default:
        break;
    }

    // prepare the loggers

    cout << "Logging to " << filename.str() << endl;
    //Logfile 
    Logfile logfile(filename.str(), eventlist);

    logfile.setStartTime(timeFromSec(0));

    RoceSinkLoggerSampling sinkLogger = RoceSinkLoggerSampling(timeFromMs(logtime), eventlist);
    if (log_sink) {
        logfile.addLogger(sinkLogger);
    }
    RoceTrafficLogger traffic_logger = RoceTrafficLogger();
    if (log_traffic) {
        logfile.addLogger(traffic_logger);
    }

    RoceSrc::setMinRTO(1000); //increase RTO to avoid spurious retransmits

    RoceSrc* roceSrc;
    RoceSink* roceSnk;

    Route* routeout, *routein;

    QueueLoggerFactory *qlf = 0;
    if (log_tor_downqueue || log_tor_upqueue) {
        qlf = new QueueLoggerFactory(&logfile, QueueLoggerFactory::LOGGER_SAMPLING, eventlist);
        qlf->set_sample_period(timeFromUs(10.0));
    } else if (log_queue_usage) {
        qlf = new QueueLoggerFactory(&logfile, QueueLoggerFactory::LOGGER_EMPTY, eventlist);
        qlf->set_sample_period(timeFromUs(10.0));
    }
#ifdef FAT_TREE
    FatTreeTopology* top;
    if (topo_file) {
        top = FatTreeTopology::load(topo_file, qlf, eventlist, queuesize, qt, snd_type);
    } else {
        FatTreeTopology::set_tiers(tiers);
        top = new FatTreeTopology(no_of_nodes, linkspeed, queuesize, qlf, 
                                               &eventlist,NULL,qt,hop_latency,switch_latency,snd_type);
    }
#endif

#ifdef OV_FAT_TREE
    OversubscribedFatTreeTopology* top = new OversubscribedFatTreeTopology(lf, &eventlist,ff);
#endif

#ifdef MH_FAT_TREE
    MultihomedFatTreeTopology* top = new MultihomedFatTreeTopology(lf, &eventlist,ff);
#endif

#ifdef STAR
    StarTopology* top = new StarTopology(lf, &eventlist,ff);
#endif

#ifdef BCUBE
    BCubeTopology* top = new BCubeTopology(lf, &eventlist,ff);
    cout << "BCUBE " << K << endl;
#endif

#ifdef VL2
    VL2Topology* top = new VL2Topology(lf, &eventlist,ff);
#endif

    if (log_switches) {
        top->add_switch_loggers(logfile, timeFromUs(20.0));
    }

    vector<const Route*>*** net_paths;
    net_paths = new vector<const Route*>**[no_of_nodes];

    int **path_refcounts;
    path_refcounts = new int*[no_of_nodes];

    int* is_dest = new int[no_of_nodes];
    
    for (size_t s = 0; s < no_of_nodes; s++) {
        is_dest[s] = 0;
        net_paths[s] = new vector<const Route*>*[no_of_nodes];
        path_refcounts[s] = new int[no_of_nodes];
        for (size_t d = 0; d < no_of_nodes; d++) {
            net_paths[s][d] = NULL;
            path_refcounts[s][d] = 0;
        }
    }
    
    ConnectionMatrix* conns = new ConnectionMatrix(no_of_nodes);

    if (tm_file){
        cout << "Loading connection matrix from  " << tm_file << endl;

        if (!conns->load(tm_file))
            exit(-1);
    }
    else {
        cout << "Loading connection matrix from  standard input" << endl;        
        conns->load(cin);
    }

    if (conns->N != no_of_nodes){
        cout << "Connection matrix number of nodes is " << conns->N << " while I am using " << no_of_nodes << endl;
        exit(-1);
    }
    
    vector<connection*>* all_conns;
    
    // used just to print out stats data at the end
    //list <const Route*> routes;

    all_conns = conns->getAllConnections();
    vector <RoceSrc*> roce_srcs;

    for (size_t c = 0; c < all_conns->size(); c++){
        connection* crt = all_conns->at(c);
        int src = crt->src;
        int dest = crt->dst;
        path_refcounts[src][dest]++;
        path_refcounts[dest][src]++;
                        
        if (!net_paths[src][dest]&&route_strategy!=ECMP_FIB) {
            vector<const Route*>* paths = top->get_bidir_paths(src,dest,false);
            net_paths[src][dest] = paths;
            /*
              for (unsigned int i = 0; i < paths->size(); i++) {
              routes.push_back((*paths)[i]);
              }
            */
        }
        if (!net_paths[dest][src]&&route_strategy!=ECMP_FIB) {
            vector<const Route*>* paths = top->get_bidir_paths(dest,src,false);
            net_paths[dest][src] = paths;
        }
    }

    map <flowid_t, TriggerTarget*> flowmap;

    for (size_t c = 0; c < all_conns->size(); c++){
        connection* crt = all_conns->at(c);
        int src = crt->src;
        int dest = crt->dst;
        cout << "Connection " << crt->src << "->" <<crt->dst << " starting at " << timeAsUs(crt->start) << " size " << crt->size << endl;

        roceSrc = new RoceSrc(NULL, NULL, eventlist,linkspeed);

        roce_srcs.push_back(roceSrc);
        roceSrc->set_dst(dest);
                        
        if (crt->size>0){
            roceSrc->set_flowsize(crt->size);
        }

        if (crt->flowid) {
            roceSrc->set_flowid(crt->flowid);
            assert(flowmap.find(crt->flowid) == flowmap.end()); // don't have dups
            flowmap[crt->flowid] = roceSrc;
        }

        if (crt->trigger) {
            Trigger* trig = conns->getTrigger(crt->trigger, eventlist);
            trig->add_target(*roceSrc);
        }
        if (crt->send_done_trigger) {
            Trigger* trig = conns->getTrigger(crt->send_done_trigger, eventlist);
            roceSrc->set_end_trigger(*trig);
        }

        roceSnk = new RoceSink();
                        
        roceSrc->setName("Roce_" + ntoa(src) + "_" + ntoa(dest));

        logfile.writeName(*roceSrc);

        roceSnk->set_src(src);
                        
        roceSnk->setName("Roce_sink_" + ntoa(src) + "_" + ntoa(dest));
        logfile.writeName(*roceSnk);
                        
        ((HostQueue*)top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)][0])->addHostSender(roceSrc);

        if (route_strategy!=SINGLE_PATH && route_strategy!=ECMP_FIB){
            abort();
        } else if (route_strategy==ECMP_FIB) {
            Route* srctotor = new Route();
            
            srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)][0]);
            srctotor->push_back(top->pipes_ns_nlp[src][top->HOST_POD_SWITCH(src)][0]);
            srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)][0]->getRemoteEndpoint());

            Route* dsttotor = new Route();
            dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)][0]);
            dsttotor->push_back(top->pipes_ns_nlp[dest][top->HOST_POD_SWITCH(dest)][0]);
            dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)][0]->getRemoteEndpoint());


            if (crt->start != TRIGGER_START && start_delta > 0){
                crt->start += timeFromUs(drand48()*start_delta);
                cout << "Start is " << timeAsUs(crt->start) << endl;
            }
            roceSrc->connect(srctotor, dsttotor, *roceSnk, crt->start);

            //register src and snk to receive packets from their respective TORs. 
            assert(top->switches_lp[top->HOST_POD_SWITCH(src)]);
            assert(top->switches_lp[top->HOST_POD_SWITCH(src)]);
            top->switches_lp[top->HOST_POD_SWITCH(src)]->addHostPort(src,roceSrc->flow_id(),roceSrc);
            top->switches_lp[top->HOST_POD_SWITCH(dest)]->addHostPort(dest,roceSrc->flow_id(),roceSnk);
        } else {
            int choice = rand()%net_paths[src][dest]->size();
            routeout = new Route(*(net_paths[src][dest]->at(choice)));
            routeout->add_endpoints(roceSrc, roceSnk);
                                
            routein = new Route(*top->get_bidir_paths(dest,src,false)->at(choice));
            routein->add_endpoints(roceSnk, roceSrc);
            roceSrc->connect(routeout, routein, *roceSnk, timeFromUs((uint32_t)rand()%20));
        }

        path_refcounts[src][dest]--;
        path_refcounts[dest][src]--;

        // free up the routes if no other connection needs them 
        if (path_refcounts[src][dest] == 0 && net_paths[src][dest]) {
            vector<const Route*>::iterator i;
            for (i = net_paths[src][dest]->begin(); i != net_paths[src][dest]->end(); i++) {
                if ((*i)->reverse())
                    delete (*i)->reverse();
                delete *i;
            }
            delete net_paths[src][dest];
        }
        if (path_refcounts[dest][src] == 0 && net_paths[dest][src]) {
            vector<const Route*>::iterator i;
            for (i = net_paths[dest][src]->begin(); i != net_paths[dest][src]->end(); i++) {
                if ((*i)->reverse())
                    delete (*i)->reverse();
                delete *i;
            }
            delete net_paths[dest][src];
        }

        if (log_sink) {
            sinkLogger.monitorSink(roceSnk);
        }
    }

    for (size_t ix = 0; ix < no_of_nodes; ix++) {
        delete path_refcounts[ix];
    }

    Logged::dump_idmap();
    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    logfile.write("# hostnicrate = " + ntoa(linkspeed/1000000) + " Mbps");
    //logfile.write("# corelinkrate = " + ntoa(HOST_NIC*CORE_TO_HOST) + " pkt/sec");
    //logfile.write("# buffer = " + ntoa((double) (queues_na_ni[0][1]->_maxsize) / ((double) pktsize)) + " pkt");
    double rtt = timeAsSec(timeFromUs(RTT));
    logfile.write("# rtt =" + ntoa(rtt));
    
    // GO!
    cout << "Starting simulation" << endl;
    while (eventlist.doNextEvent()) {
    }

    cout << "Done" << endl;
    int new_pkts = 0, rtx_pkts = 0;
    for (size_t ix = 0; ix < roce_srcs.size(); ix++) {
        new_pkts += roce_srcs[ix]->_new_packets_sent;
        rtx_pkts += roce_srcs[ix]->_rtx_packets_sent;
    }
    cout << "New: " << new_pkts << " Rtx: " << rtx_pkts << endl;

    /*list <const Route*>::iterator rt_i;
      int counts[10]; int hop;
      for (int i = 0; i < 10; i++)
      counts[i] = 0;
      for (rt_i = routes.begin(); rt_i != routes.end(); rt_i++) {
      const Route* r = (*rt_i);
      //print_route(*r);
      #ifdef PRINTPATHS
      cout << "Path:" << endl;
      #endif
      hop = 0;
      for (int i = 0; i < r->size(); i++) {
      PacketSink *ps = r->at(i); 
      CompositeQueue *q = dynamic_cast<CompositeQueue*>(ps);
      if (q == 0) {
      #ifdef PRINTPATHS
      cout << ps->nodename() << endl;
      #endif
      } else {
      #ifdef PRINTPATHS
      cout << q->nodename() << " id=" << q->id << " " << q->num_packets() << "pkts " 
                     << q->num_headers() << "hdrs " << q->num_acks() << "acks " << q->num_nacks() << "nacks " << q->num_stripped() << "stripped"
                     << endl;
#endif
                counts[hop] += q->num_stripped();
                hop++;
            }
        } 
#ifdef PRINTPATHS
        cout << endl;
#endif
    }
    for (int i = 0; i < 10; i++)
    cout << "Hop " << i << " Count " << counts[i] << endl;*/
        
}
