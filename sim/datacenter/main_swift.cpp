// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "config.h"
#include <sstream>

#include <iostream>
#include <string.h>
#include <math.h>
#include "network.h"
#include "randomqueue.h"
#include "subflow_control.h"
#include "shortflows.h"
#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "clock.h"
#include "swift.h"
#include "compositequeue.h"
//#include "firstfit.h"
#include "topology.h"
#include "connection_matrix.h"
//#include "vl2_topology.h"

#include "fat_tree_topology.h"
//#include "generic_topology.h"
//#include "oversubscribed_fat_tree_topology.h"
//#include "multihomed_fat_tree_topology.h"
//#include "star_topology.h"
//#include "bcube_topology.h"
#include <list>

// Simulation params

#define PRINT_PATHS 0

#define PERIODIC 0
#include "main.h"

uint32_t RTT = 1; // this is per link delay in us; identical RTT microseconds = 0.001 ms
#define DEFAULT_NODES 128
#define DEFAULT_QUEUE_SIZE 8

//FirstFit* ff = NULL;
//uint32_t subflow_count = 1;

//#define SWITCH_BUFFER (SERVICE * RTT / 1000)
#define USE_FIRST_FIT 0
#define FIRST_FIT_INTERVAL 100

EventList eventlist;

Logfile* lg;

void exit_error(char* progr) {
    cout << "Usage " << progr << " [UNCOUPLED(DEFAULT)|COUPLED_INC|FULLY_COUPLED|COUPLED_EPSILON] [epsilon][COUPLED_SCALABLE_TCP" << endl;
    exit(1);
}

int main(int argc, char **argv) {
    Clock c(timeFromSec(5 / 100.), eventlist);
    uint32_t no_of_conns = DEFAULT_NODES, cwnd = 15, no_of_nodes = DEFAULT_NODES,flowsize = 0;
    mem_b queuesize = DEFAULT_QUEUE_SIZE;
    linkspeed_bps linkspeed = speedFromMbps((double)HOST_NIC);
    stringstream filename(ios_base::out);
    uint32_t packet_size = 4000;
    bool plb = false;
    uint32_t no_of_subflows = 1;
    simtime_picosec tput_sample_time = timeFromUs((uint32_t)12);
    simtime_picosec endtime = timeFromMs(1.2);
    char* tm_file = NULL;
    char* topo_file = NULL;

    int i = 1;
    filename << "logout.dat";

    while (i<argc) {
        if (!strcmp(argv[i],"-o")){
            filename.str(std::string());
            filename << argv[i+1];
            i++;
        } else if (!strcmp(argv[i],"-conns")){
            no_of_conns = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-nodes")){
            no_of_nodes = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-tm")){
            tm_file = argv[i+1];
            cout << "traffic matrix input file: "<< tm_file << endl;
            i++;
        } else if (!strcmp(argv[i],"-topo")){
            topo_file = argv[i+1];
            cout << "topology input file: "<< topo_file << endl;
            i++;
        } else if (!strcmp(argv[i],"-cwnd")){
            cwnd = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-flowsize")){
            flowsize = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-linkspeed")){
            // linkspeed specified is in Mbps
            linkspeed = speedFromMbps(atof(argv[i+1]));
            i++;
        } else if (!strcmp(argv[i],"-end")){
            endtime = timeFromUs(atof(argv[i+1]));
            i++;
        } else if (!strcmp(argv[i],"-q")){
            queuesize = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-mtu")){
            packet_size = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-subflows")){
            no_of_subflows = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-tsample")){
            tput_sample_time = timeFromUs((uint32_t)atoi(argv[i+1]));
            i++;            
        } else if (!strcmp(argv[i],"-plb")){
            if (strcmp(argv[i+1], "off") == 0) {
                plb = false;
            } else if (strcmp(argv[i+1], "on") == 0) {
                plb = true;
            } else {
                exit_error(argv[0]);
            }
            i++;            
        } else {
            exit_error(argv[i]);
        }
        i++;
    }
    Packet::set_packet_size(packet_size);
    eventlist.setEndtime(endtime);

    queuesize = queuesize*Packet::data_packet_size();
    srand(13);
      

    cout << "conns " << no_of_conns << endl;
    cout << "requested nodes " << no_of_nodes << endl;
    cout << "cwnd " << cwnd << endl;
    cout << "flowsize " << flowsize << endl;
    cout << "mtu " << packet_size << endl;
    cout << "plb " << plb << endl;
    cout << "subflows " << no_of_subflows << endl;
      
    // prepare the loggers

    cout << "Logging to " << filename.str() << endl;
    //Logfile 
    Logfile logfile(filename.str(), eventlist);

#if PRINT_PATHS
    filename << ".paths";
    cout << "Logging path choices to " << filename.str() << endl;
    std::ofstream paths(filename.str().c_str());
    if (!paths){
        cout << "Can't open for writing paths file!"<<endl;
        exit(1);
    }
#endif


    lg = &logfile;

    logfile.setStartTime(timeFromSec(0));
    SwiftSinkLoggerSampling sinkLogger = SwiftSinkLoggerSampling(tput_sample_time, eventlist);
    logfile.addLogger(sinkLogger);
    SwiftTrafficLogger traffic_logger = SwiftTrafficLogger();
    logfile.addLogger(traffic_logger);
    SwiftSrc* swiftSrc;
    SwiftSink* swiftSnk;

    Route* routeout, *routein;

    SwiftRtxTimerScanner swiftRtxScanner(timeFromMs(10), eventlist);
   
#ifdef FAT_TREE
    /*
    FatTreeTopology* top = new FatTreeTopology(no_of_nodes, linkspeed, queuesize, 
                                               &logfile, &eventlist, NULL, RANDOM, SWIFT_SCHEDULER, 0);
    */
    FatTreeTopology* top = new FatTreeTopology(no_of_nodes, linkspeed, queuesize, 
                                               NULL, &eventlist, NULL, RANDOM, SWIFT_SCHEDULER, 0);
#endif

#ifdef OV_FAT_TREE
    OversubscribedFatTreeTopology* top = new OversubscribedFatTreeTopology(&logfile, &eventlist,ff);
#endif

#ifdef MH_FAT_TREE
    MultihomedFatTreeTopology* top = new MultihomedFatTreeTopology(&logfile, &eventlist,ff);
#endif

#ifdef STAR
    StarTopology* top = new StarTopology(&logfile, &eventlist,ff);
#endif

#ifdef BCUBE
    BCubeTopology* top = new BCubeTopology(&logfile,&eventlist,ff,COMPOSITE_PRIO);
    cout << "BCUBE " << K << endl;
#endif

#ifdef VL2
    VL2Topology* top = new VL2Topology(&logfile,&eventlist,ff);
#endif

#ifdef GENERIC_TOPO
    GenericTopology *top = new GenericTopology(&logfile, &eventlist);
    if (topo_file) {
        top->load(topo_file);
    }
#endif
    
    no_of_nodes = top->no_of_nodes();
    cout << "actual nodes " << no_of_nodes << endl;

    vector<const Route*>*** net_paths;
    net_paths = new vector<const Route*>**[no_of_nodes];

    int* is_dest = new int[no_of_nodes];
    
    for (uint32_t i=0; i<no_of_nodes; i++){
is_dest[i] = 0;
        net_paths[i] = new vector<const Route*>*[no_of_nodes];
        for (uint32_t j = 0; j<no_of_nodes; j++)
            net_paths[i][j] = NULL;
    }

    // Permutation connections
    ConnectionMatrix* conns = new ConnectionMatrix(no_of_nodes);

    if (tm_file){
        cout << "Loading connection matrix from  " << tm_file << endl;

        if (!conns->load(tm_file))
            exit(-1);
    }
    else {
        cout << "Loading connection matrix from standard input" << endl;        
        conns->load(cin);
    }

    if (conns->N != no_of_nodes){
        cout << "Connection matrix number of nodes is " << conns->N << " while I am using " << no_of_nodes << endl;
        exit(-1);
    }
    
    vector<connection*>* all_conns;

    // used just to print out stats data at the end
    list <const Route*> routes;
    
    list <SwiftSrc*> swift_srcs;
    // initialize all sources/sinks

    uint32_t connID = 0;
    all_conns = conns->getAllConnections();

    for (uint32_t c = 0; c < all_conns->size(); c++){
        connection* crt = all_conns->at(c);
        uint32_t src = crt->src;
        uint32_t dest = crt->dst;
        
        connID++;
        if (!net_paths[src][dest]) {
            vector<const Route*>* paths = top->get_paths(src,dest);
            net_paths[src][dest] = paths;
            for (uint32_t p = 0; p < paths->size(); p++) {
                routes.push_back((*paths)[p]);
            }
        }
        if (!net_paths[dest][src]) {
            vector<const Route*>* paths = top->get_paths(dest,src);
            net_paths[dest][src] = paths;
        }

        swiftSrc = new SwiftSrc(swiftRtxScanner, NULL, NULL, eventlist);
        swiftSrc->set_cwnd(cwnd*Packet::data_packet_size());

        if (flowsize){
            swiftSrc->set_flowsize(flowsize*Packet::data_packet_size());
        }
                
        swift_srcs.push_back(swiftSrc);
        if (plb) {
            swiftSrc->enable_plb();
        }
        swiftSnk = new SwiftSink();
        //ReorderBufferLoggerSampling* buf_logger = new ReorderBufferLoggerSampling(timeFromMs(0.01), eventlist);
        //logfile.addLogger(*buf_logger);
        //swiftSnk->add_buffer_logger(buf_logger);
        sinkLogger.monitorSink(swiftSnk);

        swiftSrc->setName("swift_" + ntoa(src) + "_" + ntoa(dest));
        logfile.writeName(*swiftSrc);

        if (no_of_subflows>1)
            swiftSnk->setName("mpswift_sink_" + ntoa(src) + "_" + ntoa(dest));
        else
            swiftSnk->setName("swift_sink_" + ntoa(src) + "_" + ntoa(dest));
        logfile.writeName(*swiftSnk);
          
        //swiftRtxScanner.registerSwift(*swiftSrc);
          
        uint32_t choice = 0;
          
#ifdef FAT_TREE
        choice = rand()%net_paths[src][dest]->size();
#endif
          
#ifdef OV_FAT_TREE
        choice = rand()%net_paths[src][dest]->size();
#endif
          
#ifdef MH_FAT_TREE
        int use_all = it_sub==net_paths[src][dest]->size();

        if (use_all)
            choice = inter;
        else
            choice = rand()%net_paths[src][dest]->size();
#endif
          
#ifdef VL2
        choice = rand()%net_paths[src][dest]->size();
#endif
          
#ifdef STAR
        choice = 0;
#endif
          
#ifdef BCUBE
        //choice = inter;
          
        int min = -1, max = -1,minDist = 1000,maxDist = 0;
        if (subflow_count==1){
            //find shortest and longest path 
            for (uint32_t dd=0;dd<net_paths[src][dest]->size();dd++){
                if (net_paths[src][dest]->at(dd)->size()<minDist){
                    minDist = net_paths[src][dest]->at(dd)->size();
                    min = dd;
                }
                if (net_paths[src][dest]->at(dd)->size()>maxDist){
                    maxDist = net_paths[src][dest]->at(dd)->size();
                    max = dd;
                }
            }
            choice = min;
        } 
        else
            choice = rand()%net_paths[src][dest]->size();
#endif
        if (choice>=net_paths[src][dest]->size()){
            printf("Weird path choice %d out of %lu\n",choice,net_paths[src][dest]->size());
            exit(1);
        }
          
#if PRINT_PATHS
        for (uint32_t ll=0;ll<net_paths[src][dest]->size();ll++){
            paths << "Route from "<< ntoa(src) << " to " << ntoa(dest) << "  (" << ll << ") -> " ;
            print_path(paths,net_paths[src][dest]->at(ll));
        }
#endif
          
        routeout = new Route(*(net_paths[src][dest]->at(choice)));
        //routeout->push_back(swiftSnk);
          
        routein = new Route(*top->get_paths(dest,src)->at(choice));
        //routein->push_back(swiftSrc);

        if (no_of_subflows == 1) {
            swiftSrc->connect(*routeout, *routein, *swiftSnk, timeFromUs((uint32_t)crt->start));
        }
        swiftSrc->set_paths(net_paths[src][dest]);
        if (no_of_subflows > 1) {
            // could probably use this for single-path case too, but historic reasons
            cout << "will start subflow " << c << " at " << crt->start << endl;
            swiftSrc->multipath_connect(*swiftSnk, timeFromUs((uint32_t)crt->start), no_of_subflows);
        }
          
        sinkLogger.monitorSink(swiftSnk);
    }
    //    ShortFlows* sf = new ShortFlows(2560, eventlist, net_paths,conns,lg, &swiftRtxScanner);

    cout << "Loaded " << connID << " connections in total\n";

    // Record the setup
    uint32_t pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    //    logfile.write("# subflows=" + ntoa(subflow_count));
    logfile.write("# hostnicrate = " + ntoa(linkspeed/1000000) + " Mbps");
    logfile.write("# corelinkrate = " + ntoa(HOST_NIC*CORE_TO_HOST) + " pkt/sec");
    //logfile.write("# buffer = " + ntoa((double) (queues_na_ni[0][1]->_maxsize) / ((double) pktsize)) + " pkt");
    double rtt = timeAsSec(timeFromUs(RTT));
    logfile.write("# rtt =" + ntoa(rtt));

    // enable logging on the first source - for debugging purposes
    //(*(swift_srcs.begin()))->log_me();

    // GO!
    while (eventlist.doNextEvent()) {
    }

    cout << "Done" << endl;

#if PRINT_PATHS
    list <const Route*>::iterator rt_i;
    int counts[10]; int hop;
    for (rt_i = routes.begin(); rt_i != routes.end(); rt_i++) {
        const Route* r = (*rt_i);
        //print_route(*r);
        cout << "Path:" << endl;
        for (uint32_t i = 0; i < r->size(); i++) {
            PacketSink *ps = r->at(i); 
            CompositeQueue *q = dynamic_cast<CompositeQueue*>(ps);
            if (q == 0) {
                cout << ps->nodename() << endl;
            } else {
                cout << q->nodename() << " id=" << q->get_id() << " " << q->num_packets() << "pkts " 
                     << q->num_headers() << "hdrs " << q->num_acks() << "acks " << q->num_nacks() << "nacks " << q->num_stripped() << "stripped"
                     << endl;
            }
        } 
        cout << endl;
    }
    for (uint32_t ix = 0; ix < 10; ix++)
        counts[ix] = 0;
    for (rt_i = routes.begin(); rt_i != routes.end(); rt_i++) {
        const Route* r = (*rt_i);
        //print_route(*r);
        hop = 0;
        for (uint32_t i = 0; i < r->size(); i++) {
            PacketSink *ps = r->at(i); 
            CompositeQueue *q = dynamic_cast<CompositeQueue*>(ps);
            if (q == 0) {
            } else {
                counts[hop] += q->num_stripped();
                q->_num_stripped = 0;
                hop++;
            }
        } 
        cout << endl;
    }
#endif    

    /*for (uint32_t i = 0; i < 10; i++)
        cout << "Hop " << i << " Count " << counts[i] << endl;
    list <SwiftSrc*>::iterator src_i;
    for (src_i = swift_srcs.begin(); src_i != swift_srcs.end(); src_i++) {
        cout << "Src, sent: " << (*src_i)->_packets_sent << "[new: " << (*src_i)->_new_packets_sent << " rtx: " << (*src_i)->_rtx_packets_sent << "] nacks: " << (*src_i)->_nacks_received << " pulls: " << (*src_i)->_pulls_received << " paths: " << (*src_i)->_paths.size() << endl;
    }
    for (src_i = swift_srcs.begin(); src_i != swift_srcs.end(); src_i++) {
        (*src_i)->print_stats();
        }*/
    /*
    uint64_t total_rtt = 0;
    cout << "RTT Histogram";
    for (uint32_t i = 0; i < 100000; i++) {
        if (SwiftSrc::_rtt_hist[i]!= 0) {
            cout << i << " " << SwiftSrc::_rtt_hist[i] << endl;
            total_rtt += SwiftSrc::_rtt_hist[i];
        }
    }
    cout << "RTT CDF";
    uint64_t cum_rtt = 0;
    for (uint32_t i = 0; i < 100000; i++) {
        if (SwiftSrc::_rtt_hist[i]!= 0) {
            cum_rtt += SwiftSrc::_rtt_hist[i];
            cout << i << " " << double(cum_rtt)/double(total_rtt) << endl;
        }
    }
    */
}
