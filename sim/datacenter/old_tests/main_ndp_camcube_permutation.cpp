// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "config.h"
#include <sstream>

#include <iostream>
#include <string.h>
#include <math.h>
#include <list>
#include "network.h"
#include "randomqueue.h"
#include "subflow_control.h"
#include "shortflows.h"
#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "clock.h"
#include "ndp.h"
#include "compositequeue.h"
#include "compositeprioqueue.h"
#include "firstfit.h"
#include "topology.h"
#include "connection_matrix.h"

//#include "fat_tree_topology.h"
//#include "oversubscribed_fat_tree_topology.h"
//#include "multihomed_fat_tree_topology.h"
//#include "star_topology.h"
#include "camcubetopology.h"
#include <list>

// Simulation params

#define PRINT_PATHS 1

#define PERIODIC 0
#include "main.h"

//int RTT = 10; // this is per link delay; identical RTT microseconds = 0.02 ms
uint32_t RTT = 1; // this is per link delay in us; identical RTT microseconds = 0.02 ms
int N = 125;
//int N=128;

FirstFit* ff = NULL;
unsigned int subflow_count = 1;

string ntoa(double n);
string itoa(uint64_t n);

//#define SWITCH_BUFFER (SERVICE * RTT / 1000)
#define USE_FIRST_FIT 0
#define FIRST_FIT_INTERVAL 100

EventList eventlist;

Logfile* lg;

void exit_error(char* progr) {
    cout << "Usage " << progr << " [UNCOUPLED(DEFAULT)|COUPLED_INC|FULLY_COUPLED|COUPLED_EPSILON] [epsilon][COUPLED_SCALABLE_TCP" << endl;
    exit(1);
}

void print_path(std::ofstream &paths,const Route* rt){
    for (unsigned int i=1;i<rt->size()-1;i+=2){
        RandomQueue* q = (RandomQueue*)rt->at(i);
        paths << q->str() << "--->";
    }

    paths<<endl<<endl;
}

int main(int argc, char **argv) {
    Packet::set_packet_size(9000);
    eventlist.setEndtime(timeFromSec(0.501));
    Clock c(timeFromSec(5 / 100.), eventlist);
    int algo = COUPLED_EPSILON;
    double epsilon = 1;
    int param = 0, cwnd = 15;
    stringstream filename(ios_base::out);

    int i = 1;
    filename << "logout.dat";

    while (i<argc) {
        if (!strcmp(argv[i],"-o")){
            filename.str(std::string());
            filename << argv[i+1];
            i++;
        }
        else
            if (!strcmp(argv[i],"-sub")){
                subflow_count = atoi(argv[i+1]);
                i++;
            }
            else
                if (!strcmp(argv[i],"-param")){
                    param = atoi(argv[i+1]);
                    cout << "param "<<param << endl;
                    i++;
                }
                else if (!strcmp(argv[i],"-cwnd")){
                    cwnd = atoi(argv[i+1]);
                    cout << "cwnd "<< cwnd << endl;
                    i++;
                }
                else 
                    if (!strcmp(argv[i], "UNCOUPLED"))
                        algo = UNCOUPLED;
                    else if (!strcmp(argv[i], "COUPLED_INC"))
                        algo = COUPLED_INC;
                    else if (!strcmp(argv[i], "FULLY_COUPLED"))
                        algo = FULLY_COUPLED;
                    else if (!strcmp(argv[i], "COUPLED_TCP"))
                        algo = COUPLED_TCP;
                    else if (!strcmp(argv[i], "COUPLED_SCALABLE_TCP"))
                        algo = COUPLED_SCALABLE_TCP;
                    else if (!strcmp(argv[i], "COUPLED_EPSILON")) {
                        algo = COUPLED_EPSILON;
                        if (argc > i+1){
                            epsilon = atof(argv[i+1]);
                            i++;
                        }
                        printf("Using epsilon %f\n", epsilon);
                    } else
                        exit_error(argv[0]);

        i++;
    }
    srand(time(NULL));
      
    cout << "Using subflow count " << subflow_count <<endl;

      
    cout <<  "Using algo="<<algo<< " epsilon=" << epsilon << endl;
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


    int tot_subs = 0;
    int cnt_con = 0;

    lg = &logfile;

    logfile.setStartTime(timeFromSec(0));

    NdpSinkLoggerSampling sinkLogger = NdpSinkLoggerSampling(timeFromMs(100), eventlist);
    logfile.addLogger(sinkLogger);
    NdpTrafficLogger traffic_logger = NdpTrafficLogger();
    logfile.addLogger(traffic_logger);
    NdpSrc* ndpSrc;
    NdpSink* ndpSnk;

    Route* routeout, *routein;
    double extrastarttime;

    NdpRtxTimerScanner ndpRtxScanner(timeFromMs(10), eventlist);
   
    int dest;

#if USE_FIRST_FIT
    if (subflow_count==1){
        ff = new FirstFit(timeFromMs(FIRST_FIT_INTERVAL),eventlist);
    }
#endif

#ifdef FAT_TREE
    FatTreeTopology* top = new FatTreeTopology(&logfile, &eventlist,ff,COMPOSITE,1);
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

#ifdef CAMCUBE
    CamCubeTopology* top = new CamCubeTopology(&logfile,&eventlist,COMPOSITE_PRIO);
    cout << "CAMCUBE " << K << endl;
#endif

#ifdef VL2
    VL2Topology* top = new VL2Topology(&logfile,&eventlist,ff);
#endif

    vector<const Route*>*** net_paths;
    net_paths = new vector<const Route*>**[N];

    int* is_dest = new int[N];
    
    for (int i=0;i<N;i++){
        is_dest[i] = 0;
        net_paths[i] = new vector<const Route*>*[N];
        for (int j = 0;j<N;j++)
            net_paths[i][j] = NULL;
    }
    
#ifdef USE_FIRST_FIT
    if (ff)
        ff->net_paths = net_paths;
#endif
    
    vector<int>* destinations;

    // Permutation connections
    ConnectionMatrix* conns = new ConnectionMatrix(N);
    //conns->setLocalTraffic(top);

    
    cout << "Running perm with " << param << " connections" << endl;
    conns->setPermutation(param);
    //conns->setIncast(1,62);
    //conns->setStride(param);
    //conns->setStaggeredPermutation(top,(double)param/100.0);
    //conns->setStaggeredRandom(top,512,1);
    //conns->setHotspot(param,512/param);
    //conns->setManytoMany(128);
    //conns->setVL2();


    //conns->setRandom(param);

    map<int,vector<int>*>::iterator it;

    // used just to print out stats data at the end
    list <const Route*> routes;
    
    list <NdpSrc*> ndp_srcs; 
    list <NdpSink*> ndp_sinks; 
    
    int connID = 0;
    for (it = conns->connections.begin(); it!=conns->connections.end();it++){
        int src = (*it).first;
        destinations = (vector<int>*)(*it).second;

        vector<int> subflows_chosen;
      
        for (unsigned int dst_id = 0;dst_id<destinations->size();dst_id++){
            connID++;
            dest = destinations->at(dst_id);
            if (!net_paths[src][dest]) {
                vector<const Route*>* paths = top->get_paths(src,dest);
                net_paths[src][dest] = paths;
                int min_length = 1000;
                for (unsigned int i = 0; i < paths->size(); i++) {
                    routes.push_back(paths->at(i));
                    if ((*paths)[i]->size()<min_length)
                        min_length = (*paths)[i]->size();
                }
            }
            if (!net_paths[dest][src]) {
                vector<const Route*>* paths = top->get_paths(dest,src);
                net_paths[dest][src] = paths;
            }

            for (int connection=0;connection<1;connection++){
                subflows_chosen.clear();

                int it_sub;
                int crt_subflow_count = subflow_count;
                tot_subs += crt_subflow_count;
                cnt_con ++;
          
                it_sub = crt_subflow_count > net_paths[src][dest]->size()?net_paths[src][dest]->size():crt_subflow_count;
          
                //if (connID%10!=0)
                //it_sub = 1;
          
                ndpSrc = new NdpSrc(NULL, NULL, eventlist);
                ndpSrc->setCwnd(cwnd*Packet::data_packet_size());
                ndp_srcs.push_back(ndpSrc);
                ndpSnk = new NdpSink(eventlist,  2 /*pull at sum on both incoming line rates*/);
                ndp_sinks.push_back(ndpSnk);
          
                ndpSrc->setName("ndp_" + ntoa(src) + "_" + ntoa(dest)+"("+ntoa(connection)+")");
                logfile.writeName(*ndpSrc);
          
                ndpSnk->setName("ndp_sink_" + ntoa(src) + "_" + ntoa(dest)+ "("+ntoa(connection)+")");
                logfile.writeName(*ndpSnk);
          
                ndpRtxScanner.registerNdp(*ndpSrc);
          
                cout << "CONN FROM  " << src << " TO " << dest << endl;


                int choice = 0;
          
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
                    for (int dd=0;dd<net_paths[src][dest]->size();dd++){
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
                //cout << "Choice "<<choice<<" out of "<<net_paths[src][dest]->size();
                subflows_chosen.push_back(choice);
          
                /*if (net_paths[src][dest]->size()==K*K/4 && it_sub<=K/2){
                  int choice2 = rand()%(K/2);*/
          
                if (choice>=net_paths[src][dest]->size()){
                    printf("Weird path choice %d out of %lu\n",choice,net_paths[src][dest]->size());
                    exit(1);
                }
          
#if PRINT_PATHS
                for (int ll=0;ll<net_paths[src][dest]->size();ll++){
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
                routeout->push_back(ndpSnk);
          
                routein = new Route();

                routein = new Route(*top->get_paths(dest,src)->at(choice));
                routein->push_back(ndpSrc);

                extrastarttime = 0 * drand();
          
                ndpSrc->connect(*routeout, *routein, *ndpSnk, timeFromMs(extrastarttime));
          
#ifdef NDP_PACKET_SCATTER
                ndpSrc->set_paths(net_paths[src][dest]);
                ndpSnk->set_paths(net_paths[dest][src]);

                cout << "Using PACKET SCATTER!!!! with path count outgoing "<< net_paths[src][dest]->size()<< " incoming " << net_paths[dest][src]->size() << "Hop count " ;

                for (int lk=0;lk<net_paths[src][dest]->size();lk++)
                    cout << net_paths[src][dest]->at(lk)->size()<<" ";
                cout << endl;

                vector<const Route*>* rts = net_paths[src][dest];
                const Route* rt = rts->at(0);
                PacketSink* first_queue = rt->at(0);
                if (ndpSrc->_log_me) {
                    cout << "First hop: " << first_queue->nodename() << endl;
                    QueueLoggerSimple queue_logger = QueueLoggerSimple();
                    logfile.addLogger(queue_logger);
                    ((Queue*)first_queue)->setLogger(&queue_logger);
                    
                    ndpSrc->set_traffic_logger(&traffic_logger);
                }
#endif

          
                //          if (ff)
                //            ff->add_flow(src,dest,ndpSrc);
          
                sinkLogger.monitorSink(ndpSnk);
            }
        }
    }
    //    ShortFlows* sf = new ShortFlows(2560, eventlist, net_paths,conns,lg, &ndpRtxScanner);

    cout << "Mean number of subflows " << ntoa((double)tot_subs/cnt_con)<<endl;

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    logfile.write("# subflows=" + ntoa(subflow_count));
    logfile.write("# hostnicrate = " + ntoa(HOST_NIC) + " pkt/sec");
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
        for (int i = 0; i < r->size(); i++) {
            PacketSink *ps = r->at(i); 
            CompositePrioQueue *q = dynamic_cast<CompositePrioQueue*>(ps);
            if (q == 0) {
                cout << ps->nodename() << endl;
            } else {
                cout << q->nodename() << " id=" << q->id << " " << q->num_packets() << " pkts " 
                     << q->num_headers() << " hdrs " << q->num_pulls() << " pulls " << q->num_acks() << " acks " << q->num_nacks() << " nacks " << q->num_stripped() << " stripped"
                     << endl;
                counts[hop] += q->num_stripped();
                hop++;
            }
        } 
        cout << endl;
    }
    for (int i = 0; i < 10; i++)
        cout << "Hop " << i << " Count " << counts[i] << endl;
    list <NdpSrc*>::iterator src_i;
    for (src_i = ndp_srcs.begin(); src_i != ndp_srcs.end(); src_i++) {
        cout << "Src, packets sent: " << (*src_i)->_packets_sent/9000 << " pulls: " << (*src_i)->_pulls_received << " i_pulls: " << (*src_i)->_implicit_pulls << " total pulls: " << (*src_i)->_pulls_received + (*src_i)->_implicit_pulls << endl;
    }
    list <NdpSink*>::iterator sink_i;
    for (sink_i = ndp_sinks.begin(); sink_i != ndp_sinks.end(); sink_i++) {
        cout << "Sink, received counts: ";
        for (int i = 0; i < 20; i++) {
            if ((*sink_i)->_path_lens[i] > 0)
                cout << i << ":" << (*sink_i)->_path_lens[i] << " ";
        }
        cout << endl;
        cout << "      trimmed counts: ";
        for (int i = 0; i < 20; i++) {
            if ((*sink_i)->_trimmed_path_lens[i] > 0)
                cout << i << ":" << (*sink_i)->_trimmed_path_lens[i] << " ";
        }
        cout << endl;
    }

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
