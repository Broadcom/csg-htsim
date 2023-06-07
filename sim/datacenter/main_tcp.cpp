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
#include "mtcp.h"
#include "tcp.h"
#include "tcp_transfer.h"
#include "cbr.h"
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

uint32_t RTT = 10; // this is per link delay; identical RTT microseconds = 0.001 ms
uint32_t DEFAULT_NODES = 16;
//uint32_t N=128;

FirstFit* ff = NULL;
size_t subflow_count = 1;

//#define SWITCH_BUFFER (SERVICE * RTT / 1000)
#define USE_FIRST_FIT 0
#define FIRST_FIT_INTERVAL 100

EventList eventlist;

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
    eventlist.setEndtime(timeFromSec(4));
    Clock c(timeFromSec(50 / 100.), eventlist);
    linkspeed_bps linkspeed = speedFromMbps((double)HOST_NIC);
    int algo = COUPLED_EPSILON;
    double epsilon = 1;
    uint32_t no_of_conns = 0, no_of_nodes = DEFAULT_NODES;
    stringstream filename(ios_base::out);

    int i = 1;
    filename << "logout.dat";

    while (i<argc) {
        if (!strcmp(argv[i],"-o")){
            filename.str(std::string());
            filename << argv[i+1];
            i++;
        }
        else if (!strcmp(argv[i],"-sub")){
            subflow_count = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-conns")){
            no_of_conns = atoi(argv[i+1]);
            cout << "no_of_conns "<<no_of_conns << endl;
            i++;
        } else if (!strcmp(argv[i],"-nodes")){
            no_of_nodes = atoi(argv[i+1]);
            cout << "no_of_nodes "<<no_of_nodes << endl;
            i++;
        } else if (!strcmp(argv[i], "UNCOUPLED"))
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
    cout << "conns " << no_of_conns << endl;
    cout << "requested nodes " << no_of_nodes << endl;

      
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


    uint32_t tot_subs = 0;
    uint32_t cnt_con = 0;

    logfile.setStartTime(timeFromSec(0));

    TcpSinkLoggerSampling sinkLogger = TcpSinkLoggerSampling(timeFromMs(1000), eventlist);
    logfile.addLogger(sinkLogger);

    //TcpLoggerSimple logTcp;logfile.addLogger(logTcp);


    TcpSrc* tcpSrc;
    TcpSink* tcpSnk;

    //CbrSrc* cbrSrc;
    //CbrSink* cbrSnk;

    Route* routeout, *routein;
    double extrastarttime;

    TcpRtxTimerScanner tcpRtxScanner(timeFromMs(10), eventlist);
   
    MultipathTcpSrc* mtcp;
    
    uint32_t dest;

    QueueLoggerFactory qlf(&logfile, QueueLoggerFactory::LOGGER_SAMPLING, eventlist);
    qlf.set_sample_period(timeFromUs(1000.0));

#if USE_FIRST_FIT
    if (subflow_count==1){
        ff = new FirstFit(timeFromMs(FIRST_FIT_INTERVAL),eventlist);
    }
#endif

#ifdef FAT_TREE
    FatTreeTopology* top = new FatTreeTopology(no_of_nodes, linkspeed, memFromPkt(8), &qlf, &eventlist,ff,RANDOM,0);
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
    no_of_nodes = top->no_of_nodes();
    cout << "actual nodes " << no_of_nodes << endl;

    vector<const Route*>*** net_paths;
    net_paths = new vector<const Route*>**[no_of_nodes];

    int* is_dest = new int[no_of_nodes];
    
    for (uint32_t i=0;i<no_of_nodes;i++){
        is_dest[i] = 0;
        net_paths[i] = new vector<const Route*>*[no_of_nodes];
        for (uint32_t j = 0;j<no_of_nodes;j++)
            net_paths[i][j] = NULL;
    }
    
    if (ff)
        ff->net_paths = net_paths;
    
    vector<uint32_t>* destinations;

    // Permutation connections
    ConnectionMatrix* conns = new ConnectionMatrix(no_of_nodes);
    //conns->setLocalTraffic(top);

    
    cout << "Running perm with " << no_of_conns << " connections" << endl;
    conns->setPermutation(no_of_conns);
    //conns->setStaggeredPermutation(top,(double)no_of_conns/100.0);
    //conns->setStaggeredRandom(top,512,1);
    //conns->setHotspot(no_of_conns,512/no_of_conns);
    //conns->setManytoMany(128);

    //conns->setVL2();


    //conns->setRandom(no_of_conns);

    map<uint32_t, vector<uint32_t>*>::iterator it;
    
    uint32_t connID = 0;
    for (it = conns->connections.begin(); it!=conns->connections.end();it++){
        uint32_t src = (*it).first;
        destinations = (*it).second;

        vector<int> subflows_chosen;
      
        for (uint32_t dst_id = 0;dst_id<destinations->size();dst_id++){
            connID++;
            dest = destinations->at(dst_id);
            if (!net_paths[src][dest])
                net_paths[src][dest] = top->get_paths(src,dest);

            /*bool cbr = 1;
              if (cbr){
              cbrSrc = new CbrSrc(eventlist,speedFromPktps(7999),timeFromMs(0),timeFromMs(0));
              cbrSnk = new CbrSink();
              
              cbrSrc->setName("cbr_" + ntoa(src) + "_" + ntoa(dest)+"_"+ntoa(dst_id));
              logfile.writeName(*cbrSrc);
              
              cbrSnk->setName("cbr_sink_" + ntoa(src) + "_" + ntoa(dest)+"_"+ntoa(dst_id));
              logfile.writeName(*cbrSnk);
              
              // tell it the route
              if (net_paths[src][dest]->size()==1){
              choice = 0;
              }
              else {
              choice = rand()%net_paths[src][dest]->size();
              }
              
              routeout = new Route(*(net_paths[src][dest]->at(choice)));
              routeout->push_back(cbrSnk);
          
              cbrSrc->connect(*routeout, *cbrSnk, timeFromMs(0));
              }*/
        
            {
                //we should create multiple connections. How many?
                //if (connID%3!=0)
                //continue;

                for (uint32_t connection=0;connection<1;connection++){
                    //            if (algo == COUPLED_EPSILON)
                    //mtcp = new MultipathTcpSrc(algo, eventlist, NULL, epsilon);
                    //else
                    mtcp = new MultipathTcpSrc(algo, eventlist, NULL);
            
                    //uint64_t bb = generateFlowSize();

                    //            if (subflow_control)
                    //subflow_control->add_flow(src,dest,mtcp);

                    subflows_chosen.clear();

                    uint32_t it_sub;
                    size_t crt_subflow_count = subflow_count;
                    tot_subs += crt_subflow_count;
                    cnt_con ++;

                    it_sub = crt_subflow_count > net_paths[src][dest]->size()?net_paths[src][dest]->size():crt_subflow_count;

#ifdef MH_FAT_TREE
                    int use_all = it_sub==net_paths[src][dest]->size();
#endif
                    //if (connID%10!=0)
                    //it_sub = 1;
            
                    for (uint32_t inter = 0; inter < it_sub; inter++) {
                        //              if (connID%10==0){
                        tcpSrc = new TcpSrc(NULL, NULL, eventlist);
                        tcpSnk = new TcpSink();
                        /*}
                          else {
                          tcpSrc = new TcpSrcTransfer(NULL,NULL,eventlist,bb,net_paths[src][dest]);
                          tcpSnk = new TcpSinkTransfer();
                          }*/

                        //if (connection==1)
                        //tcpSrc->set_app_limit(9000);
              
                        tcpSrc->setName("mtcp_" + ntoa(src) + "_" + ntoa(inter) + "_" + ntoa(dest)+"("+ntoa(connection)+")");
                        logfile.writeName(*tcpSrc);
              
                        tcpSnk->setName("mtcp_sink_" + ntoa(src) + "_" + ntoa(inter) + "_" + ntoa(dest)+ "("+ntoa(connection)+")");
                        logfile.writeName(*tcpSnk);
              
                        tcpRtxScanner.registerTcp(*tcpSrc);

                        /*int found;
                          do {
                          found = 0;
                
                          //if (net_paths[src][dest]->size()==K*K/4 && it_sub <= K/2)
                          //choice = rand()%(K/2);
                          //else 
                          choice = rand()%net_paths[src][dest]->size();
                
                          for (uint32_t cnt = 0;cnt<subflows_chosen.size();cnt++){
                          if (subflows_chosen.at(cnt)==choice){
                          found = 1;
                          break;
                          }
                          }
                          }while(found);
                        //*/
                        size_t choice = 0;

#ifdef FAT_TREE
                        choice = rand()%net_paths[src][dest]->size();
#endif

#ifdef OV_FAT_TREE
                        choice = rand()%net_paths[src][dest]->size();
#endif

#ifdef MH_FAT_TREE
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
                        } else
                            choice = rand()%net_paths[src][dest]->size();
#endif
                        //cout << "Choice "<<choice<<" out of "<<net_paths[src][dest]->size();
                        subflows_chosen.push_back(choice);

                        /*if (net_paths[src][dest]->size()==K*K/4 && it_sub<=K/2){
                          uint32_t choice2 = rand()%(K/2);*/

                        if (choice>=net_paths[src][dest]->size()){
                            printf("Weird path choice %lu out of %lu\n",choice,net_paths[src][dest]->size());
                            exit(1);
                        }
                
#if PRINT_PATHS
                        paths << "Route from "<< ntoa(src) << " to " << ntoa(dest) << "  (" << choice << ") -> " ;
                        print_path(paths,net_paths[src][dest]->at(choice));
#endif

                        routeout = new Route(*(net_paths[src][dest]->at(choice)));
                        routeout->push_back(tcpSnk);
              
                        routein = new Route();
                        routein->push_back(tcpSrc);
                        extrastarttime = 0 * drand();
            
                        //join multipath connection
              
                        mtcp->addSubflow(tcpSrc);
              
                        if (inter == 0) {
                            mtcp->setName("multipath" + ntoa(src) + "_" + ntoa(dest)+"("+ntoa(connection)+")");
                            logfile.writeName(*mtcp);
                        }
              
                        tcpSrc->connect(*routeout, *routein, *tcpSnk, timeFromMs(extrastarttime));
            
#ifdef PACKET_SCATTER
                        tcpSrc->set_paths(net_paths[src][dest]);
                        cout << "Using PACKET SCATTER!!!!"<<endl;
#endif
              
                        if (ff&&!inter)
                            ff->add_flow(src,dest,tcpSrc);
              
                        sinkLogger.monitorMultipathSink(tcpSnk);
                    }
                }
            }
        }
    }
    //    ShortFlows* sf = new ShortFlows(2560, eventlist, net_paths,conns,lg, &tcpRtxScanner);

    cout << "Mean number of subflows " << ntoa((double)tot_subs/cnt_con)<<endl;

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    logfile.write("# subflows=" + ntoa(subflow_count));
    logfile.write("# hostnicrate = " + ntoa(linkspeed/1000000) + " Mbps");
    logfile.write("# corelinkrate = " + ntoa(HOST_NIC*CORE_TO_HOST) + " pkt/sec");
    //logfile.write("# buffer = " + ntoa((double) (queues_na_ni[0][1]->_maxsize) / ((double) pktsize)) + " pkt");
    double rtt = timeAsSec(timeFromUs(RTT));
    logfile.write("# rtt =" + ntoa(rtt));

    // GO!
    while (eventlist.doNextEvent()) {
    }
}
