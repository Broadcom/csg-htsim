// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        

#include "config.h"
#include <sstream>

#include <iostream>
#include <string.h>
#include <math.h>
#include "network.h"
#include "randomqueue.h"
//#include "subflow_control.h"
#include "shortflows.h"
#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "clock.h"
#include "swift.h"
#include "compositequeue.h"
#include "firstfit.h"
#include "topology.h"
#include "connection_matrix.h"
//#include "vl2_topology.h"

#include "fat_tree_topology.h"
//#include "oversubscribed_fat_tree_topology.h"
//#include "multihomed_fat_tree_topology.h"
//#include "star_topology.h"
//#include "bcube_topology.h"
#include <list>

// Simulation params

#define PRINT_PATHS 0

#define PERIODIC 0
#include "main.h"

//int RTT = 10; // this is per link delay; identical RTT microseconds = 0.02 ms
uint32_t RTT = 1; // this is per link delay in us; identical RTT microseconds = 0.02 ms
int DEFAULT_NODES = 432;
#define DEFAULT_QUEUE_SIZE 2000
//int N=128;

FirstFit* ff = NULL;

string ntoa(double n);
string itoa(uint64_t n);

//#define SWITCH_BUFFER (SERVICE * RTT / 1000)
//#define USE_FIRST_FIT 0
//#define FIRST_FIT_INTERVAL 100

EventList eventlist;

Logfile* lg;

void exit_error(char* progr) {
    cout << "Usage " << progr << " [UNCOUPLED(DEFAULT)|COUPLED_INC|FULLY_COUPLED|COUPLED_EPSILON] [epsilon][COUPLED_SCALABLE_TCP" << endl;
    exit(1);
}

void print_path(std::ofstream &paths,const Route* rt){
    for (uint32_t i=1;i<rt->size()-1;i+=2){
        RandomQueue* q = (RandomQueue*)rt->at(i);
        if (q!=NULL)
            paths << q->str() << " ";
        else 
            paths << "NULL ";
    }

    paths<<endl;
}

int main(int argc, char **argv) {
    eventlist.setEndtime(timeFromSec(0.02));
    Clock c(timeFromSec(5 / 100.), eventlist);
    int no_of_conns = 0, cwnd = 15, no_of_nodes = DEFAULT_NODES;
    stringstream filename(ios_base::out);
    RouteStrategy route_strategy = SINGLE_PATH;
    double extrastarttime = 0;
    double logtime = 0.25; // ms;
    int psize = 9000;
    int flowsize = 1000000;
    int queuepkts = DEFAULT_QUEUE_SIZE;
    linkspeed_bps linkspeed = speedFromMbps((double)HOST_NIC);
    int i = 1;
    filename << "logout.dat";

    while (i<argc) {
        if (!strcmp(argv[i],"-o")){
            filename.str(std::string());
            filename << argv[i+1];
            i++;
        } else if (!strcmp(argv[i],"-conns")){
            no_of_conns = atoi(argv[i+1]);
            cout << "no_of_conns "<<no_of_conns << endl;
            i++;
        } else if (!strcmp(argv[i],"-nodes")){
            no_of_nodes = atoi(argv[i+1]);
            cout << "no_of_nodes "<<no_of_nodes << endl;
            i++;
        } else if (!strcmp(argv[i],"-cwnd")){
            cwnd = atoi(argv[i+1]);
            cout << "cwnd "<< cwnd << endl;
            i++;
        } else if (!strcmp(argv[i],"-extra")){
            extrastarttime = atof(argv[i+1]);
            cout << "extra "<< extrastarttime << endl;
            i++;
        } else if (!strcmp(argv[i],"-psize")){
            psize = atof(argv[i+1]);
            cout << "psize "<< psize << endl;
            i++;
        } else if (!strcmp(argv[i],"-flowsize")){
            flowsize = atof(argv[i+1]);
            cout << "flowsize "<< flowsize << endl;
            i++;
        } else if (!strcmp(argv[i],"-q")){
            queuepkts = atoi(argv[i+1]);            
            cout << "queuepkts "<< queuepkts << endl;
            i++;
        } else if (!strcmp(argv[i],"-logtime")){
            logtime = atof(argv[i+1]);            
            cout << "logtime "<< logtime << endl;
            i++;
        } else if (!strcmp(argv[i],"-strat")){
            if (!strcmp(argv[i+1], "perm")) {
                route_strategy = SCATTER_PERMUTE;
            } else if (!strcmp(argv[i+1], "rand")) {
                route_strategy = SCATTER_RANDOM;
            } else if (!strcmp(argv[i+1], "pull")) {
                route_strategy = PULL_BASED;
            } else if (!strcmp(argv[i+1], "single")) {
                route_strategy = SINGLE_PATH;
            }
            i++;
        } else
            exit_error(argv[0]);

        i++;
    }
    Packet::set_packet_size(psize);
    mem_b queuesize = memFromPkt(queuepkts);
    srand(13);
      
    if (route_strategy == NOT_SET) {
        fprintf(stderr, "Route Strategy not set.  Use the -strat param.  \nValid values are perm, rand, pull, rg and single\n");
        exit(1);
    }

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
    cout << "Queuesize: " << queuesize << endl;

    lg = &logfile;

    logfile.setStartTime(timeFromSec(0));
    SwiftSinkLoggerSampling sinkLogger = SwiftSinkLoggerSampling(timeFromMs(logtime), eventlist);
    logfile.addLogger(sinkLogger);
    SwiftTrafficLogger traffic_logger = SwiftTrafficLogger();
    logfile.addLogger(traffic_logger);
    //NdpSrc::setMinRTO(50000); //increase RTO to avoid spurious retransmits
    //NdpSrc::setRouteStrategy(route_strategy);
    //NdpSink::setRouteStrategy(route_strategy);

    SwiftSrc* swiftSrc;
    SwiftSink* swiftSnk;

    Route* routeout, *routein;

    SwiftRtxTimerScanner swiftRtxScanner(timeFromMs(0.01), eventlist);
   
    int dest;

    QueueLoggerFactory qlf(&logfile, QueueLoggerFactory::LOGGER_SAMPLING, eventlist);
    qlf.set_sample_period(timeFromUs(1000.0));

#ifdef FAT_TREE
    FatTreeTopology* top = new FatTreeTopology(no_of_nodes, linkspeed, queuesize, &qlf, 
                                               &eventlist,ff,RANDOM,0);
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
    BCubeTopology* top = new BCubeTopology(&logfile,&eventlist,ff);
    cout << "BCUBE " << K << endl;
#endif

#ifdef VL2
    VL2Topology* top = new VL2Topology(&logfile,&eventlist,ff);
#endif

    vector<const Route*>*** net_paths;
    net_paths = new vector<const Route*>**[no_of_nodes];

    int* is_dest = new int[no_of_nodes];
    
    for (int i=0;i<no_of_nodes;i++){
        is_dest[i] = 0;
        net_paths[i] = new vector<const Route*>*[no_of_nodes];
        for (int j = 0;j<no_of_nodes;j++)
            net_paths[i][j] = NULL;
    }
    
#if USE_FIRST_FIT
    if (ff)
        ff->net_paths = net_paths;
#endif
    
    vector<uint32_t>* destinations;

    // Permutation connections
    ConnectionMatrix* conns = new ConnectionMatrix(no_of_conns);
    //conns->setLocalTraffic(top);

    
    //cout << "Running perm with " << no_of_conns << " connections" << endl;
    //conns->setPermutation(no_of_conns);
    cout << "Running incast with " << no_of_conns << " connections" << endl;
    conns->setIncast(no_of_conns, no_of_nodes-no_of_conns);
    //conns->setStride(no_of_conns);
    //conns->setStaggeredPermutation(top,(double)no_of_conns/100.0);
    //conns->setStaggeredRandom(top,512,1);
    //conns->setHotspot(no_of_conns,512/no_of_conns);
    //conns->setManytoMany(128);

    //conns->setVL2();


    //conns->setRandom(no_of_conns);

    map<uint32_t,vector<uint32_t>*>::iterator it;

    // used just to print out stats data at the end
    list <const Route*> routes;
    
    int connID = 0;
    for (it = conns->connections.begin(); it!=conns->connections.end();it++){
        int src = (*it).first;
        destinations = (vector<uint32_t>*)(*it).second;

        for (uint32_t dst_id = 0;dst_id<destinations->size();dst_id++){
            connID++;
            dest = destinations->at(dst_id);
            if (!net_paths[src][dest]) {
                vector<const Route*>* paths = top->get_paths(src,dest);
                net_paths[src][dest] = paths;
                for (uint32_t i = 0; i < paths->size(); i++) {
                    routes.push_back((*paths)[i]);
                }
            }
            if (!net_paths[dest][src]) {
                vector<const Route*>* paths = top->get_paths(dest,src);
                net_paths[dest][src] = paths;
            }

            for (int connection=0;connection<1;connection++){
                swiftSrc = new SwiftSrc(swiftRtxScanner, NULL, NULL, eventlist);
                cout << "setting cwnd " << cwnd*Packet::data_packet_size() << endl;
                if (connID != 1) {
                    swiftSrc->set_flowsize(flowsize);
                } else {
                    swiftSrc->set_flowsize(20000000);
                }
                swiftSrc->set_cwnd(cwnd*Packet::data_packet_size());
                swiftSnk = new SwiftSink();
          
                swiftSrc->setName("swift_" + ntoa(src) + "_" + ntoa(dest)+"("+ntoa(connection)+")");
                logfile.writeName(*swiftSrc);
          
                swiftSnk->setName("swift_sink_" + ntoa(src) + "_" + ntoa(dest)+ "("+ntoa(connection)+")");
                logfile.writeName(*swiftSnk);
          
                //swiftRtxScanner.registerSwift(*swiftSrc);
          
                uint32_t choice = 0;
          
#ifdef FAT_TREE
                choice = rand()%net_paths[src][dest]->size();
#endif
          
#ifdef OV_FAT_TREE
                choice = rand()%net_paths[src][dest]->size();
#endif
          
#ifdef VL2
                choice = rand()%net_paths[src][dest]->size();
#endif
          
#ifdef STAR
                choice = 0;
#endif
          
                /*if (net_paths[src][dest]->size()==K*K/4 && it_sub<=K/2){
                  int choice2 = rand()%(K/2);*/
          
                if (choice>=net_paths[src][dest]->size()){
                    printf("Weird path choice %d out of %lu\n",choice,net_paths[src][dest]->size());
                    exit(1);
                }
          
#if PRINT_PATHS
                for (uint32_t ll=0;ll<net_paths[src][dest]->size();ll++){
                    paths << "Route from "<< ntoa(src) << " to " << ntoa(dest) << "  (" << ll << ") -> " ;
                    print_path(paths,net_paths[src][dest]->at(ll));
                }
                /*                                if (src>=12){
                                                assert(net_paths[src][dest]->size()>1);
                                                net_paths[src][dest]->erase(net_paths[src][dest]->begin());
                                                paths << "Killing entry!" << endl;
                                  
                                                if (choice>=net_paths[src][dest]->size())
                                                choice = 0;
                                                }*/
#endif
          
                routeout = new Route(*(net_paths[src][dest]->at(choice)));
                //routeout->push_back(swiftSnk);
          
                routein = new Route(*top->get_paths(dest,src)->at(choice));
                //routein->push_back(swiftSrc);

                //extrastarttime = timeFromMs(5) * drand();
                double extra = extrastarttime * drand();
                if (connID == 1) {
                    swiftSrc->connect(*routeout, *routein, *swiftSnk, 0);
                } else {
                    swiftSrc->connect(*routeout, *routein, *swiftSnk, timeFromMs(1) + timeFromSec(extra));
                }
          
                sinkLogger.monitorSink(swiftSnk);
            }
        }
    }
    //    ShortFlows* sf = new ShortFlows(2560, eventlist, net_paths,conns,lg, &ndpRtxScanner);

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    logfile.write("# hostnicrate = " + ntoa(linkspeed/1000000) + " Mbps");
    logfile.write("# corelinkrate = " + ntoa(HOST_NIC*CORE_TO_HOST) + " pkt/sec");
    //logfile.write("# buffer = " + ntoa((double) (queues_na_ni[0][1]->_maxsize) / ((double) pktsize)) + " pkt");
    double rtt = timeAsSec(timeFromUs(RTT));
    logfile.write("# rtt =" + ntoa(rtt));

    // GO!
    while (eventlist.doNextEvent()) {
    }

    cout << "Done" << endl;
    list <const Route*>::iterator rt_i;
    int counts[10]; int hop;
    for (int i = 0; i < 10; i++)
        counts[i] = 0;
    for (rt_i = routes.begin(); rt_i != routes.end(); rt_i++) {
        const Route* r = (*rt_i);
        //print_route(*r);
        cout << "Path:" << endl;
        hop = 0;
        for (uint32_t i = 0; i < r->size(); i++) {
            PacketSink *ps = r->at(i); 
            CompositeQueue *q = dynamic_cast<CompositeQueue*>(ps);
            if (q == 0) {
                cout << ps->nodename() << endl;
            } else {
                cout << q->nodename() << " id=" << q->get_id() << " " << q->num_packets() << "pkts " 
                     << q->num_headers() << "hdrs " << q->num_acks() << "acks " << q->num_nacks() << "nacks " << q->num_stripped() << "stripped"
                     << endl;
                counts[hop] += q->num_stripped();
                hop++;
            }
        } 
        cout << endl;
    }
    for (int i = 0; i < 10; i++)
        cout << "Hop " << i << " Count " << counts[i] << endl;
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
