// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-        
#include "config.h"
#include <sstream>

#include <iostream>
#include <string.h>
#include <math.h>
#include "network.h"
#include "randomqueue.h"
#include "shortflows.h"
#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "clock.h"
#include "tcp.h"
#include "compositequeue.h"
#include "firstfit.h"
#include "topology.h"
#include "connection_matrix.h"
#include "dctcp.h"
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
#define DEFAULT_QUEUE_SIZE 8

FirstFit* ff = NULL;
uint32_t subflow_count = 1;

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

class StopLogger : public EventSource {
public:
    StopLogger(EventList& eventlist, const string& name) : EventSource(eventlist, name) {};
    void doNextEvent() {
	//nothing to do, just prevent flow restarting
    }
private:
};

int main(int argc, char **argv) {
    Packet::set_packet_size(9000);
    eventlist.setEndtime(timeFromSec(0.6));
    Clock c(timeFromSec(5 / 100.), eventlist);
    mem_b queuesize = memFromPkt(DEFAULT_QUEUE_SIZE);
    linkspeed_bps linkspeed = speedFromMbps((double)10000);
    int no_of_conns = 0, cwnd = 15, no_of_nodes = DEFAULT_NODES,
	flowsize=Packet::data_packet_size()*50;
    stringstream filename(ios_base::out);

    int i = 1;
    filename << "logout.dat";

    while (i<argc) {
	if (!strcmp(argv[i],"-o")) {
	    filename.str(std::string());
	    filename << argv[i+1];
	    i++;
	} else if (!strcmp(argv[i],"-sub")) {
	    subflow_count = atoi(argv[i+1]);
	    i++;
	} else if (!strcmp(argv[i],"-conns")) {
	    no_of_conns = atoi(argv[i+1]);
	    cout << "no_of_conns "<<no_of_conns << endl;
	    i++;
	} else if (!strcmp(argv[i],"-nodes")) {
	    no_of_nodes = atoi(argv[i+1]);
	    cout << "no_of_nodes "<<no_of_nodes << endl;
	    i++;
	} else if (!strcmp(argv[i],"-cwnd")) {
	    cwnd = atoi(argv[i+1]);
	    cout << "cwnd "<< cwnd << endl;
	    i++;
	} else if (!strcmp(argv[i],"-flowsize")){
	    flowsize = atoi(argv[i+1]);
	    cout << "flowsize "<< flowsize << endl;
	    i++;
	} else if (!strcmp(argv[i],"-q")){
	    queuesize = memFromPkt(atoi(argv[i+1]));
	    i++;
	} else
	    exit_error(argv[0]);
	
	i++;
    }
    srand(13);

    cout << "Using subflow count " << subflow_count <<endl;

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

    TcpSinkLoggerSampling sinkLogger = TcpSinkLoggerSampling(timeFromMs(1), eventlist);
    logfile.addLogger(sinkLogger);
    TcpTrafficLogger traffic_logger = TcpTrafficLogger();
    logfile.addLogger(traffic_logger);

    TcpSrc* tcpSrc;
    TcpSink* tcpSnk;

    Route* routeout, *routein;
    double extrastarttime;

    // scanner interval must be less than min RTO
    TcpRtxTimerScanner tcpRtxScanner(timeFromMs(10), eventlist);
   
    int dest;

#if USE_FIRST_FIT
    if (subflow_count==1){
	ff = new FirstFit(timeFromMs(FIRST_FIT_INTERVAL),eventlist);
    }
#endif

#ifdef FAT_TREE
    FatTreeTopology* top = new FatTreeTopology(no_of_nodes, linkspeed, queuesize, NULL, 
					       &eventlist,ff,LOSSLESS_INPUT_ECN,0);
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
    
#ifdef USE_FIRST_FIT
    if (ff)
	ff->net_paths = net_paths;
#endif
    
    vector<uint32_t>* destinations;

    // Permutation connections
    ConnectionMatrix* conns = new ConnectionMatrix(no_of_nodes);
    //conns->setLocalTraffic(top);

    
    //cout << "Running perm with " << no_of_conns << " connections" << endl;
    //conns->setPermutation(no_of_conns);
    cout << "Running incast with " << no_of_conns << " connections" << endl;
    conns->setIncast(no_of_conns, (no_of_nodes-1)-no_of_conns);
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
		subflows_chosen.clear();

		uint32_t crt_subflow_count = subflow_count;
		tot_subs += crt_subflow_count;
		cnt_con ++;
	  
#ifdef MH_FAT_TREE
		int it_sub;
		it_sub = crt_subflow_count > net_paths[src][dest]->size()?net_paths[src][dest]->size():crt_subflow_count;
#endif
	  
		//if (connID%10!=0)
		//it_sub = 1;
	  
		//tcpSrc = new DCTCPSrc(NULL, &traffic_logger, eventlist);
		tcpSrc = new DCTCPSrc(NULL, NULL, eventlist);
		tcpSnk = new TcpSink();
	  
		tcpSrc->set_ssthresh(10*Packet::data_packet_size());
		tcpSrc->set_flowsize(flowsize);
		tcpSrc->_rto = timeFromMs(10);

		tcpSrc->setName("tcp_" + ntoa(src) + "_" + ntoa(dest)+"("+ntoa(connection)+")");
		logfile.writeName(*tcpSrc);
	  
		tcpSnk->setName("tcp_sink_" + ntoa(src) + "_" + ntoa(dest)+ "("+ntoa(connection)+")");
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
		//cout << "Choice "<<choice<<" out of "<<net_paths[src][dest]->size();
		subflows_chosen.push_back(choice);
	  
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
		/*				if (src>=12){
						assert(net_paths[src][dest]->size()>1);
						net_paths[src][dest]->erase(net_paths[src][dest]->begin());
						paths << "Killing entry!" << endl;
				  
						if (choice>=net_paths[src][dest]->size())
						choice = 0;
						}*/
#endif
	  
		routeout = new Route(*(net_paths[src][dest]->at(choice)));
		routeout->push_back(tcpSnk);
	  
		routein = new Route(*top->get_paths(dest,src)->at(choice));
		routein->push_back(tcpSrc);

		extrastarttime = 80;
	  
		tcpSrc->connect(*routeout, *routein, *tcpSnk, timeFromMs(extrastarttime));
	  
#ifdef PACKET_SCATTER
		tcpSrc->set_paths(net_paths[src][dest]);
		tcpSnk->set_paths(net_paths[dest][src]);

		cout << "Using PACKET SCATTER!!!!"<<endl;
#endif

	  
		//	  if (ff)
		//	    ff->add_flow(src,dest,ndpSrc);
	  
		sinkLogger.monitorSink(tcpSnk);
	    }
	}
    }
    //    ShortFlows* sf = new ShortFlows(2560, eventlist, net_paths,conns,lg, &ndpRtxScanner);

    cout << "Mean number of subflows " << ntoa((double)tot_subs/cnt_con)<<endl;

    // Set up a long subflow to a node on the same switch as the incast destination
    int long_dest_no = 1; //a dest on the same LS ans dst 0
    int long_src_no = 5;  //a source not on the same LS as dst 1
    if (no_of_nodes <= 128)
	long_src_no = 5;  //a source not on the same LS as dst 1
    else if (no_of_nodes == 432)
	long_src_no = 7;  //a source not on the same LS as dst 1
    else
	abort(); // add node here
    TcpSrc* longSrc = new DCTCPSrc(NULL, &traffic_logger, eventlist);
    //longSrc->set_flowsize(flowsize);
    TcpSink* longSnk = new TcpSink();
    longSrc->set_ssthresh(10*Packet::data_packet_size());
    longSrc->_rto = timeFromMs(10);
    longSrc->setName("long_" + ntoa(long_src_no) + "_" + ntoa(long_dest_no));
    logfile.writeName(*longSrc);
    longSnk->setName("long_sink_" + ntoa(long_src_no) + "_" + ntoa(long_dest_no));
    logfile.writeName(*longSnk);
    tcpRtxScanner.registerTcp(*longSrc);

    vector<const Route*>* srcpaths = top->get_paths(long_src_no, long_dest_no);
    routeout = new Route(*(srcpaths->at(0)));
    routeout->push_back(longSnk);

    vector<const Route*>* dstpaths = top->get_paths(long_dest_no, long_src_no);
    routein = new Route(*(dstpaths->at(0)));
    routein->push_back(longSrc);

    longSrc->connect(*routeout, *routein, *longSnk, timeFromMs(0));
    //longSrc->set_paths(srcpaths);
    //longSnk->set_paths(dstpaths);
    sinkLogger.monitorSink(longSnk);

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

    cout << "Done" << endl;
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
	for (uint32_t i = 0; i < r->size(); i++) {
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
	cout << "Hop " << i << " Count " << counts[i] << endl;
    */	
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
