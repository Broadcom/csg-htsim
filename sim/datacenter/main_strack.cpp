// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "config.h"
#include <sstream>

#include <iostream>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "network.h"
#include "randomqueue.h"
#include "shortflows.h"
#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "clock.h"
#include "strack.h"
#include "compositequeue.h"
#include "firstfit.h"
#include "topology.h"
#include "queue_lossless_input.h"
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

string ntoa(double n);
string itoa(uint64_t n);

//#define SWITCH_BUFFER (SERVICE * RTT / 1000)
#define USE_FIRST_FIT 0
#define FIRST_FIT_INTERVAL 100

EventList eventlist;

void exit_error(char* progr) {
    cout << "Usage " << progr << " [-nodes N]\n\t[-conns C]\n\t[-cwnd cwnd_size]\n\t[-q queue_size]\n\t[-oversubscribed_cc] Use receiver-driven AIMD to reduce total window when trims are not last hop\n\t[-queue_type composite|random|lossless|lossless_input|]\n\t[-tm traffic_matrix_file]\n\t[-strat route_strategy (single,rand,perm,pull,ecmp,\n\tecmp_host path_count,ecmp_ar,ecmp_rr,\n\tecmp_host_ar ar_thresh)]\n\t[-log log_level]\n\t[-seed random_seed]\n\t[-end end_time_in_usec]\n\t[-mtu MTU]\n\t[-hop_latency x] per hop wire latency in us,default 1\n\t[-switch_latency x] switching latency in us, default 0\n\t[-host_queue_type  swift|prio|fair_prio]" << endl;
    exit(1);
}

void print_path(std::ofstream &paths,const Route* rt){
    for (size_t i=1;i<rt->size()-1;i++) {
        BaseQueue* q = dynamic_cast<BaseQueue*>(rt->at(i));
        if (q!=NULL)
            paths << "Q:" << q->str() << " ";
        else 
            paths << "- ";
    }

    paths<<endl;
}

void filter_paths(uint32_t src_id, vector<const Route*>& paths, FatTreeTopology* top) {
    uint32_t num_servers = top->no_of_servers();
    uint32_t num_cores = top->no_of_cores();
    uint32_t num_pods = top->no_of_pods();
    uint32_t pod_switches = top->no_of_switches_per_pod();

    uint32_t path_classes = pod_switches/2;
    cout << "srv: " << num_servers << " cores: " << num_cores << " pods: " << num_pods << " pod_sw: " << pod_switches << " classes: " << path_classes << endl;
    uint32_t pclass = src_id % path_classes;
    cout << "src: " << src_id << " class: " << pclass << endl;

    for (uint32_t r = 0; r < paths.size(); r++) {
        const Route* rt = paths.at(r);
        if (rt->size() == 12) {
            BaseQueue* q = dynamic_cast<BaseQueue*>(rt->at(6));
            cout << "Q:" << atoi(q->str().c_str()+2) << " " << q->str() << endl;
            uint32_t core = atoi(q->str().c_str()+2);
            if (core % path_classes != pclass) {
                paths[r] = NULL;
            }
        }
    }
}

int main(int argc, char **argv) {
    Clock c(timeFromSec(5 / 100.), eventlist);
    mem_b queuesize = DEFAULT_QUEUE_SIZE;
    linkspeed_bps linkspeed = speedFromMbps((double)HOST_NIC);
    int packet_size = 4000;
    uint32_t path_entropy_size = 10000000;
    uint32_t no_of_conns = 0, cwnd = 15, no_of_nodes = DEFAULT_NODES;
    uint32_t tiers = 3; // we support 2 and 3 tier fattrees
    double logtime = 1; //0.25; // ms;
    stringstream filename(ios_base::out);
    simtime_picosec hop_latency = timeFromUs((uint32_t)1);
    simtime_picosec switch_latency = timeFromUs((uint32_t)0);
    queue_type qt = COMPOSITE;

    bool log_sink = false;
    bool rts = false;
    bool log_tor_downqueue = false;
    bool log_tor_upqueue = false;
    bool log_traffic = false;
    bool log_switches = false;
    bool log_queue_usage = false;
    double ecn_thresh = 0.5; // default marking threshold for ECN load balancing
    RouteStrategy route_strategy = NOT_SET;
    int seed = 13;
    int path_burst = 1;
    int i = 1;

    bool oversubscribed_congestion_control = false;

    filename << "logout.dat";
    int end_time = 1000;//in microseconds

    queue_type snd_type = FAIR_PRIO;

    float ar_sticky_delta = 10;
    FatTreeSwitch::sticky_choices ar_sticky = FatTreeSwitch::PER_PACKET;
    uint64_t high_pfc = 15, low_pfc = 12;

    char* tm_file = NULL;

    while (i<argc) {
        if (!strcmp(argv[i],"-o")) {
            filename.str(std::string());
            filename << argv[i+1];
            i++;
        } else if (!strcmp(argv[i],"-oversubscribed_cc")) {
            oversubscribed_congestion_control = true;
        } else if (!strcmp(argv[i],"-conns")) {
            no_of_conns = atoi(argv[i+1]);
            cout << "no_of_conns "<<no_of_conns << endl;
            i++;
        } else if (!strcmp(argv[i],"-end")) {
            end_time = atoi(argv[i+1]);
            cout << "endtime(us) "<< end_time << endl;
            i++;            
        } else if (!strcmp(argv[i],"-rts")) {
            rts = true;
            cout << "rts enabled "<< endl;
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
        } else if (!strcmp(argv[i],"-cwnd")) {
            cwnd = atoi(argv[i+1]);
            cout << "cwnd "<< cwnd << endl;
            i++;
        } else if (!strcmp(argv[i],"-tm")){
            tm_file = argv[i+1];
            cout << "traffic matrix input file: "<< tm_file << endl;
            i++;
        } else if (!strcmp(argv[i],"-q")){
            queuesize = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-ecn_thresh")){
            // fraction of queuesize, between 0 and 1
            ecn_thresh = atof(argv[i+1]); 
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
        } else if (!strcmp(argv[i],"-path_burst")){
            path_burst = atoi(argv[i+1]);
            cout << "path burst " << path_burst << endl;
            i++;
        } else if (!strcmp(argv[i],"-hop_latency")){
            hop_latency = timeFromUs(atof(argv[i+1]));
            cout << "Hop latency set to " << timeAsUs(hop_latency) << endl;
            i++;
        } else if (!strcmp(argv[i],"-switch_latency")){
            switch_latency = timeFromUs(atof(argv[i+1]));
            cout << "Switch latency set to " << timeAsUs(switch_latency) << endl;
            i++;
        } else if (!strcmp(argv[i],"-ar_sticky_delta")){
            ar_sticky_delta = atof(argv[i+1]);
            cout << "Adaptive routing sticky delta " << ar_sticky_delta << "us" << endl;
            i++;
        } else if (!strcmp(argv[i],"-pfc_thresholds")){
            low_pfc = atoi(argv[i+1]);
            high_pfc = atoi(argv[i+2]);
            cout << "PFC thresholds high " << high_pfc << " low " << low_pfc << endl;
            i++;
        } else if (!strcmp(argv[i],"-ar_granularity")){
            if (!strcmp(argv[i+1],"packet"))
                ar_sticky = FatTreeSwitch::PER_PACKET;
            else if (!strcmp(argv[i+1],"flow"))
                ar_sticky = FatTreeSwitch::PER_FLOWLET;
            else  {
                cout << "Expecting -ar_granularity packet|flow, found " << argv[i+1] << endl;
                exit(1);
            }   
            i++;
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
        } else if (!strcmp(argv[i],"-strat")){
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
            } else if (!strcmp(argv[i+1], "rr_ecmp")) {
                //this is the host route strategy;
                route_strategy = ECMP_FIB_ECN;
                qt = COMPOSITE_ECN_LB;
                //this is the switch route strategy. 
                FatTreeSwitch::set_strategy(FatTreeSwitch::RR_ECMP);
            } else if (!strcmp(argv[i+1], "ecmp_host_ecn")) {
                route_strategy = ECMP_FIB_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                qt = COMPOSITE_ECN_LB;
            } else if (!strcmp(argv[i+1], "ecmp_host_random_ecn")) {
                route_strategy = ECMP_RANDOM_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                qt = COMPOSITE_ECN_LB;
            } else if (!strcmp(argv[i+1], "reactive_ecn")) {
                // Jitu's suggestion for something really simple
                // One path at a time, but switch whenever we get a trim or ecn
                //this is the host route strategy;
                route_strategy = REACTIVE_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                qt = COMPOSITE_ECN_LB;
            } else if (!strcmp(argv[i+1], "ecmp_ar")) {
                route_strategy = ECMP_FIB;
                path_entropy_size = 1;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ADAPTIVE_ROUTING);
            } else if (!strcmp(argv[i+1], "ecmp_host_ar")) {
                route_strategy = ECMP_FIB;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP_ADAPTIVE);
                //the stuff below obsolete
                //FatTreeSwitch::set_ar_fraction(atoi(argv[i+2]));
                //cout << "AR fraction: " << atoi(argv[i+2]) << endl;
                //i++;
            } else if (!strcmp(argv[i+1], "ecmp_rr")) {
                // switch round robin
                route_strategy = ECMP_FIB;
                path_entropy_size = 1;
                FatTreeSwitch::set_strategy(FatTreeSwitch::RR);
            }
            i++;
        } else {
            cout << "Unknown parameter " << argv[i] << endl;
            exit_error(argv[0]);
        }
                
        i++;
    }

    srand(seed);
    srandom(seed);
    cout << "Parsed args\n";
    Packet::set_packet_size(packet_size);


    if (oversubscribed_congestion_control){
        cout << "Using oversubscribed congestion control, Strack does not support it now." << endl;
        exit(-1);
    }

    FatTreeSwitch::_ar_sticky = ar_sticky;
    FatTreeSwitch::_sticky_delta = timeFromUs(ar_sticky_delta);
    FatTreeSwitch::_ecn_threshold_fraction = ecn_thresh;

    LosslessInputQueue::_high_threshold = Packet::data_packet_size()*high_pfc;
    LosslessInputQueue::_low_threshold = Packet::data_packet_size()*low_pfc;


    eventlist.setEndtime(timeFromUs((uint32_t)end_time));
    queuesize = memFromPkt(queuesize);
    
    switch (route_strategy) {
    case ECMP_FIB_ECN:
    case ECMP_RANDOM_ECN:
    case REACTIVE_ECN:
        if (qt != COMPOSITE_ECN_LB) {
            fprintf(stderr, "Route Strategy is ECMP ECN.  Must use an ECN queue\n");
            exit(1);
        }
        if (ecn_thresh <= 0 || ecn_thresh >= 1) {
            fprintf(stderr, "Route Strategy is ECMP ECN.  ecn_thresh must be between 0 and 1\n");
            exit(1);
        }
        // no break, fall through
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

    cout << "Linkspeed set to " << linkspeed/1000000000 << "Gbps" << endl;
    logfile.setStartTime(timeFromSec(0));

    STrackSinkLoggerSampling sinkLogger = STrackSinkLoggerSampling(timeFromMs(logtime), eventlist);
    if (log_sink) {
        logfile.addLogger(sinkLogger);
    }
    NdpTrafficLogger traffic_logger = NdpTrafficLogger();
    if (log_traffic) {
        logfile.addLogger(traffic_logger);
    }

#if PRINT_PATHS
    filename << ".paths";
    cout << "Logging path choices to " << filename.str() << endl;
    std::ofstream paths(filename.str().c_str());
    if (!paths){
        cout << "Can't open for writing paths file!"<<endl;
        exit(1);
    }
#endif

    // STrackSrc::setMinRTO(50000); //increase RTO to avoid spurious retransmits
    STrackSrc::setPathEntropySize(path_entropy_size);
    STrackSrc::setRouteStrategy(route_strategy);
    STrackSink::setRouteStrategy(route_strategy);

    STrackSrc* strackSrc;
    STrackSink* strackSnk;

    Route* routeout, *routein;

    // scanner interval must be less than min RTO
    NdpRtxTimerScanner ndpRtxScanner(timeFromUs((uint32_t)9), eventlist);
   
    QueueLoggerFactory *qlf = 0;
    if (log_tor_downqueue || log_tor_upqueue) {
        qlf = new QueueLoggerFactory(&logfile, QueueLoggerFactory::LOGGER_SAMPLING, eventlist);
        qlf->set_sample_period(timeFromUs(10.0));
    } else if (log_queue_usage) {
        qlf = new QueueLoggerFactory(&logfile, QueueLoggerFactory::LOGGER_EMPTY, eventlist);
        qlf->set_sample_period(timeFromUs(10.0));
    }
#ifdef FAT_TREE
    FatTreeTopology::set_tiers(tiers);
    FatTreeTopology* top = new FatTreeTopology(no_of_nodes, linkspeed, queuesize, qlf, 
                                               &eventlist, NULL, qt, hop_latency,
                                               switch_latency,
                                               cwnd*Packet::data_packet_size(),
                                               snd_type);
    // FatTreeTopology* top = new FatTreeTopology(no_of_nodes, linkspeed, queuesize, 
    //                                            NULL, &eventlist, NULL, qt, SWIFT_SCHEDULER, 0,
    //                                            cwnd*Packet::data_packet_size());
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

        if (!conns->load(tm_file)){
            cout << "Failed to load connection matrix " << tm_file << endl;
            exit(-1);
        }
    }
    else {
        cout << "Loading connection matrix from  standard input" << endl;        
        conns->load(cin);
    }

    if (conns->N != no_of_nodes){
        cout << "Connection matrix number of nodes is " << conns->N << " while I am using " << no_of_nodes << endl;
        exit(-1);
    }
    
    cout << " Loading connection matrix done" << endl;
    //handle link failures specified in the connection matrix.
    for (size_t c = 0; c < conns->failures.size(); c++){
        failure* crt = conns->failures.at(c);

        cout << "Adding link failure switch type" << crt->switch_type << " Switch ID " << crt->switch_id << " link ID "  << crt->link_id << endl;
        top->add_failed_link(crt->switch_type,crt->switch_id,crt->link_id);
    }

    // used just to print out stats data at the end
    //list <const Route*> routes;

    vector<connection*>* all_conns = conns->getAllConnections();
    vector <STrackSrc*> strack_srcs;
    vector <STrackSink*> strack_sinks;


    for (size_t c = 0; c < all_conns->size(); c++){
        connection* crt = all_conns->at(c);
        int src = crt->src;
        int dest = crt->dst;
        path_refcounts[src][dest]++;
        path_refcounts[dest][src]++;
                        
        if (!net_paths[src][dest] ) {
            vector<const Route*>* paths = top->get_bidir_paths(src,dest,false);
            net_paths[src][dest] = paths;
            /*
              for (unsigned int i = 0; i < paths->size(); i++) {
              routes.push_back((*paths)[i]);
              }
            */
        }
        if (!net_paths[dest][src]) {
            vector<const Route*>* paths = top->get_bidir_paths(dest,src,false);
            net_paths[dest][src] = paths;
        }
    }

    map <flowid_t, TriggerTarget*> flowmap;

    STrackRtxTimerScanner strackRtxScanner(timeFromMs(10), eventlist);

    simtime_picosec RTT1=timeFromUs((uint32_t)8);
    Pipe* in_pipes[no_of_conns];
    FairScheduler* in_queues[no_of_conns];

    for (size_t c = 0; c < all_conns->size(); c++){
        connection* crt = all_conns->at(c);
        int src = crt->src;
        int dest = crt->dst;
        cout << "Connection " << crt->src << "->" <<crt->dst << " starting at " << crt->start << " size " << crt->size << " total_phy_paths " << net_paths[src][dest]->size() << endl;

        strackSrc = new STrackSrc(strackRtxScanner, NULL,NULL,eventlist);
                // cout << "dada" << endl;
        strackSrc->set_cwnd(cwnd*Packet::data_packet_size());
        strackSrc->set_base_rtt(RTT1);
        strackSrc->set_dst(dest);
        strackSrc->set_src(src);
        strackSrc->setName("STRACK"+ntoa(c)); logfile.writeName(*strackSrc);

        if (crt->size>0){
            cout << " set flow size " << crt->size << endl;
            strackSrc->set_flowsize(crt->size);
        }
        if (crt->trigger) {
            Trigger* trig = conns->getTrigger(crt->trigger, eventlist);
            trig->add_target(*strackSrc);
        }

        if (crt->send_done_trigger) {
            Trigger* trig = conns->getTrigger(crt->send_done_trigger, eventlist);
            strackSrc->set_end_trigger(*trig);
        }

        strackSnk = new STrackSink();
        strackSnk->setName("STRACKSink"+ntoa(c)); logfile.writeName(*strackSnk);
        strackSnk->set_src(src);
        strackSnk->set_dst(dest);
        
        strack_srcs.push_back(strackSrc);
        strack_sinks.push_back(strackSnk);
        // int choice = rand()%net_paths[src][dest]->size();
        // routeout = new Route(*(net_paths[src][dest]->at(choice)));

        // routein = new Route(*(net_paths[dest][src]->at(choice)));

        // strackSrc->connect(*routeout,*routein,*strackSnk,crt->start);//drand()*timeFromMs(1));

        // strackSrc->set_paths(net_paths[src][dest]);
        // strackSnk->set_paths(net_paths[dest][src]);
               


        // sinkLogger.monitorSink(strackSnk);
        switch (route_strategy) {
        case SCATTER_PERMUTE:
        case SCATTER_RANDOM:
        case SCATTER_ECMP:
        case PULL_BASED:
            // STrackSrc->connect(NULL, NULL, *strackSnk, crt->start);
            // STrackSrc->set_paths(net_paths[src][dest]);
            // strackSnk->set_paths(net_paths[dest][src]);
            break;
        case ECMP_FIB:
        case ECMP_FIB_ECN:
        case ECMP_RANDOM_ECN:
        case REACTIVE_ECN:
            {
                Route* srctotor = new Route();
                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                srctotor->push_back(top->pipes_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]->getRemoteEndpoint());

                Route* dsttotor = new Route();
                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                dsttotor->push_back(top->pipes_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]->getRemoteEndpoint());


                strackSrc->connect(*srctotor, *dsttotor, *strackSnk, crt->start);
                strackSrc->set_paths(path_entropy_size);
                strackSnk->set_paths(path_entropy_size);

                //register src and snk to receive packets from their respective TORs. 
                assert(top->switches_lp[top->HOST_POD_SWITCH(src)]);
                assert(top->switches_lp[top->HOST_POD_SWITCH(dest)]);
                top->switches_lp[top->HOST_POD_SWITCH(src)]->addHostPort(src,strackSrc->flow_id(),strackSrc);
                top->switches_lp[top->HOST_POD_SWITCH(dest)]->addHostPort(dest,strackSrc->flow_id(),strackSnk);
                break;
            }
        case SINGLE_PATH:
            {
                // assert(route_strategy==SINGLE_PATH);
                // int choice = rand()%net_paths[src][dest]->size();
                // routeout = new Route(*(net_paths[src][dest]->at(choice)));
                // routeout->add_endpoints(STrackSrc, strackSnk);
                                
                // routein = new Route(*top->get_bidir_paths(dest,src,false)->at(choice));
                // routein->add_endpoints(strackSnk, STrackSrc);
                // STrackSrc->connect(routeout, routein, *strackSnk, crt->start);
                break;
            }
        case NOT_SET:
            abort();
        }

    //     path_refcounts[src][dest]--;
    //     path_refcounts[dest][src]--;


    //     // set up the triggers
    //     // xxx

    //     // free up the routes if no other connection needs them 
    //     if (path_refcounts[src][dest] == 0 && net_paths[src][dest]) {
    //         vector<const Route*>::iterator i;
    //         for (i = net_paths[src][dest]->begin(); i != net_paths[src][dest]->end(); i++) {
    //             if ((*i)->reverse())
    //                 delete (*i)->reverse();
    //             delete *i;
    //         }
    //         delete net_paths[src][dest];
    //     }
    //     if (path_refcounts[dest][src] == 0 && net_paths[dest][src]) {
    //         vector<const Route*>::iterator i;
    //         for (i = net_paths[dest][src]->begin(); i != net_paths[dest][src]->end(); i++) {
    //             if ((*i)->reverse())
    //                 delete (*i)->reverse();
    //             delete *i;
    //         }
    //         delete net_paths[dest][src];
    //     }

    //     if (log_sink) {
    //         sinkLogger.monitorSink(strackSnk);
    //     }
    }

    // for (size_t ix = 0; ix < no_of_nodes; ix++) {
    //     delete path_refcounts[ix];
    // }

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

    top->report_stats();

    cout << "Done" << endl;
    // int new_pkts = 0, rtx_pkts = 0, bounce_pkts = 0;
    // for (size_t ix = 0; ix < strack_srcs.size(); ix++) {
    //     new_pkts += strack_srcs[ix]->_new_packets_sent;
    //     rtx_pkts += strack_srcs[ix]->_rtx_packets_sent;
    //     bounce_pkts += strack_srcs[ix]->_bounces_received;
    // }


    // cout << "New: " << new_pkts << " Rtx: " << rtx_pkts << " Bounced: " << bounce_pkts << endl;

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

string ntoa(double n) {
    stringstream s;
    s << n;
    return s.str();
}

string itoa(uint64_t n) {
    stringstream s;
    s << n;
    return s.str();
}
