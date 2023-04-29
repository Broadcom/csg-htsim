// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
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
#include "dctcp.h"
#include "dctcp_transfer.h"
#include "mtcp.h"
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

uint32_t RTT = 1; // this is per link delay in us; identical RTT microseconds = 0.02 ms
int DEFAULT_NODES = 128;
#define DEFAULT_QUEUE_SIZE 100

FirstFit* ff = NULL;
uint32_t subflow_count = 8;

string ntoa(double n);
string itoa(uint64_t n);

#define USE_FIRST_FIT 0
#define FIRST_FIT_INTERVAL 100

EventList eventlist;

Logfile* lg;

void exit_error(char* progr, char *param) {
    cerr << "Bad parameter: " << param << endl;
    cerr << "Usage " << progr << " (see src code for parameters)" << endl;
    exit(1);
}

void print_path(std::ofstream &paths, const Route* rt){
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
    TcpPacket::set_packet_size(9000);
    eventlist.setEndtime(timeFromSec(0.21));
    Clock c(timeFromSec(5 / 100.), eventlist);
    int no_of_conns = DEFAULT_NODES, no_of_nodes = DEFAULT_NODES, ssthresh = 15;
    mem_b queuesize = memFromPkt(DEFAULT_QUEUE_SIZE);
    linkspeed_bps linkspeed = speedFromMbps((double)10000);
    stringstream filename(ios_base::out);
    int failed_links = 0;
    int i = 1;
    filename << "logout.dat";

    while (i<argc) {
	if (i == argc-1) exit_error(argv[0], argv[i]);
	if (!strcmp(argv[i],"-o")){
	    filename.str(std::string());
	    filename << argv[i+1];
	    i++;
	} else if (!strcmp(argv[i],"-sub")){
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
	} else if (!strcmp(argv[i],"-ssthresh")){
	    ssthresh = atoi(argv[i+1]);
	    cout << "ssthresh "<< ssthresh << endl;
	    i++;
	} else if (!strcmp(argv[i],"-q")){
	    queuesize = memFromPkt(atoi(argv[i+1]));
	    cout << "queuesize "<<queuesize << endl;
	    i++;
	} else if (!strcmp(argv[i],"-fail")){
	    failed_links = atoi(argv[i+1]);
	    cout << "failed_links "<<failed_links << endl;
	    i++;
	} else
	    exit_error(argv[0], argv[i]);
	i++;
    }
    srand(time(NULL));
      
    cout << "Using subflow count " << subflow_count <<endl;
      
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

    TcpSinkLoggerSampling sinkLogger = TcpSinkLoggerSampling(timeFromMs(10), eventlist);
    logfile.addLogger(sinkLogger);
    TcpTrafficLogger traffic_logger = TcpTrafficLogger();
    logfile.addLogger(traffic_logger);
    TcpSrc* tcpSrc;
    TcpSink* tcpSnk;

    Route* routeout, *routein;
    double extrastarttime;

    TcpRtxTimerScanner tcpRtxScanner(timeFromMs(10), eventlist);
   
    int dest;

#if USE_FIRST_FIT
    if (subflow_count==1){
	ff = new FirstFit(timeFromMs(FIRST_FIT_INTERVAL),eventlist);
    }
#endif

#ifdef FAT_TREE
    FatTreeTopology* top = new FatTreeTopology(no_of_nodes, linkspeed, queuesize, 0, 
					       &eventlist,ff,ECN,failed_links);
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
    ConnectionMatrix* conns = new ConnectionMatrix(no_of_nodes);
    //conns->setLocalTraffic(top);

    
    cout << "Running perm with " << no_of_conns << " connections" << endl;
    conns->setPermutation(no_of_conns);
    //conns->setIncast(no_of_conns,10);
    //conns->setStride(no_of_conns);
    //conns->setStaggeredPermutation(top,(double)no_of_conns/100.0);
    //conns->setStaggeredRandom(top,512,1);
    //conns->setHotspot(no_of_conns,512/no_of_conns);
    //conns->setManytoMany(128);

    //conns->setVL2();
    //conns->setRandom(no_of_conns);


    // used just to print out stats data at the end
    list <const Route*> routes;
    
    int connID = 0;
    map<uint32_t,vector<uint32_t>*>::iterator it;
    for (it = conns->connections.begin(); it!=conns->connections.end();it++){
	int src = (*it).first;
	destinations = (vector<uint32_t>*)(*it).second;

	vector<uint32_t> subflows_chosen;
      
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
		cnt_con ++;
		
		//if (connID==1){
		//  tcpSrc = new DCTCPSrcTransfer(NULL, NULL, eventlist,90000,NULL,NULL);
		//  tcpSnk = new DCTCPSinkTransfer();
		//}
		//else {
		    tcpSrc = new DCTCPSrc(NULL, NULL, eventlist);
		    tcpSnk = new TcpSink();
		    //}
		tcpSrc->set_ssthresh(ssthresh*Packet::data_packet_size());
		
		tcpSrc->setName("dctcp_" + ntoa(src) + "_" + ntoa(dest));
		logfile.writeName(*tcpSrc);
		
		tcpSnk->setName("dctcp_sink_" + ntoa(src) + "_" + ntoa(dest));
		logfile.writeName(*tcpSnk);
		
		tcpRtxScanner.registerTcp(*tcpSrc);
		
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
		    routeout->push_back(tcpSnk);
		    
		    routein = new Route(*(top->get_paths(dest,src)->at(choice)));
		    routein->push_back(tcpSrc);
		    
		    extrastarttime = 0 * drand();
		    
		    tcpSrc->connect(*routeout, *routein, *tcpSnk, timeFromMs(extrastarttime));
	  
#ifdef PACKET_SCATTER
		    tcpSrc->set_paths(net_paths[src][dest]);
		    tcpSnk->set_paths(net_paths[dest][src]);

		    cout << "Using PACKET SCATTER!!!!"<<endl;
#endif

	  
		    //	  if (ff)
		    //	    ff->add_flow(src,dest,tcpSrc);
	  
		    sinkLogger.monitorSink(tcpSnk);
		}
	    }
	}

    cout << "Mean number of subflows " << ntoa((double)tot_subs/cnt_con)<<endl;

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    logfile.write("# subflows=" + ntoa(subflow_count));
    logfile.write("# hostnicrate = " + ntoa(HOST_NIC) + " pkt/sec");
    logfile.write("# corelinkrate = " + ntoa(HOST_NIC*CORE_TO_HOST) + " pkt/sec");
    double rtt = timeAsSec(timeFromUs(RTT));
    logfile.write("# rtt =" + ntoa(rtt));

    // GO!
    while (eventlist.doNextEvent()) {
    }

    cout << "Done" << endl;
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
