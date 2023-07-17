// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "config.h"
#include <sstream>
#include <string.h>

#include <iostream>
#include <math.h>
#include "network.h"
#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "clock.h"
#include "ndp.h"
#include "compositequeue.h"

// Simulation params

void exit_error(char* progr){
    cout << "Usage " << progr << " [UNCOUPLED(DEFAULT)|COUPLED_INC|FULLY_COUPLED|COUPLED_EPSILON] rate rtt" << endl;
    exit(1);
}

int main(int argc, char **argv) {
    EventList eventlist;
    simtime_picosec end_time = timeFromSec(1);

    Clock c(timeFromSec(50/100.), eventlist);

    uint32_t cwnd = 50;

    int seed = 13;

    mem_b queuesize = 35; 
    mem_b ecn_threshold = 70; 

    stringstream filename(ios_base::out);
    filename << "logout.dat";

    bool rts = false;

    uint32_t mtu = 4000;
    linkspeed_bps SERVICE1 = speedFromMbps((uint64_t)100000);

    simtime_picosec RTT1=timeFromUs((uint32_t)1);

    int flow_count = 2;
    if (argc>1)
        flow_count = atoi(argv[1]);

    int i = 1;
    while (i<argc) {
        if (!strcmp(argv[i],"-o")) {
            filename.str(std::string());
            filename << argv[i+1];
            i++;
        } else if (!strcmp(argv[i],"-oversubscribed_cc")) {
            NdpSink::_oversubscribed_congestion_control = true;
        } else if (!strcmp(argv[i],"-conns")) {
            flow_count = atoi(argv[i+1]);
            cout << "no_of_conns "<<flow_count << endl;
            i++;
        } else if (!strcmp(argv[i],"-end")) {
            end_time = timeFromUs((uint32_t)atoi(argv[i+1]));
            cout << "endtime(us) "<< end_time << endl;
            i++;            
        } else if (!strcmp(argv[i],"-rts")) {
            rts = true;
            cout << "rts enabled "<< endl;
        } else if (!strcmp(argv[i],"-cwnd")) {
            cwnd = atoi(argv[i+1]);
            cout << "cwnd "<< cwnd << endl;
            i++;
        } else if (!strcmp(argv[i],"-q")){
            queuesize = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-ecn_thresh")){
            // fraction of queuesize, between 0 and 1
            ecn_threshold = atoi(argv[i+1]); 
            i++;
        } else if (!strcmp(argv[i],"-linkspeed")){
            // linkspeed specified is in Mbps
            SERVICE1 = speedFromMbps(atof(argv[i+1]));
            i++;
        } else if (!strcmp(argv[i],"-seed")){
            seed = atoi(argv[i+1]);
            cout << "random seed "<< seed << endl;
            i++;
        } else if (!strcmp(argv[i],"-mtu")){
            mtu = atoi(argv[i+1]);
            i++;
        } else {
            cout << "Unknown parameter " << argv[i] << endl;
            exit_error(argv[0]);
        }   
        i++;
    }
    srand(seed);
    srandom(seed);
    eventlist.setEndtime(end_time);

    cout << "Outputting to " << filename.str() << endl;
    Logfile logfile(filename.str(),eventlist);
  
    logfile.setStartTime(timeFromSec(0.0));

    Packet::set_packet_size(mtu);

    queuesize = memFromPkt(queuesize);
    ecn_threshold = memFromPkt(ecn_threshold);
    //TrafficLoggerSimple logger;

    //logfile.addLogger(logger);
    //QueueLoggerSampling qs1 = QueueLoggerSampling(timeFromMs(10),eventlist);logfile.addLogger(qs1);
    // Build the network

    Pipe pipe1(RTT1, eventlist); pipe1.setName("pipe1"); logfile.writeName(pipe1);
    Pipe pipe2(RTT1, eventlist); pipe2.setName("pipe2"); logfile.writeName(pipe2);

    CompositeQueue queue(SERVICE1, queuesize, eventlist,NULL); queue.setName("Queue1"); logfile.writeName(queue);
    queue.set_ecn_threshold(ecn_threshold);

    NdpSrc* ndpSrc;
    NdpSink* ndpSnk;
    NdpSinkLoggerSampling sinkLogger(timeFromUs((uint32_t)25),eventlist);
    logfile.addLogger(sinkLogger);
    NdpRtxTimerScanner ndpRtxScanner(timeFromMs(1),eventlist);
    route_t* routeout;
    route_t* routein;

    NdpSink::_oversubscribed_congestion_control = true; 
 
    vector<NdpSrc*> ndp_srcs;

    for (int i=0;i<flow_count;i++){
        ndpSrc = new NdpSrc(NULL,NULL,eventlist,rts);
        ndpSrc->setRouteStrategy(SINGLE_PATH);
        ndpSrc->setCwnd(cwnd*Packet::data_packet_size());
        
        ndpSrc->setName("NDP"); 
        logfile.writeName(*ndpSrc);

        ndp_srcs.push_back(ndpSrc);

        ndpSnk = new NdpSink(eventlist, SERVICE1, 10); 
        ndpSnk->setName("NdpSink");
        ndpSnk->setRouteStrategy(SINGLE_PATH);
        logfile.writeName(*ndpSnk);
        
        ndpRtxScanner.registerNdp(*ndpSrc);
        
        // tell it the route
        routeout = new route_t();
        // NDP expects each src host to have a FairPriorityQueue
        routeout->push_back(new FairPriorityQueue(SERVICE1, memFromPkt(1000),eventlist, NULL));
        routeout->push_back(&queue); 
        routeout->push_back(&pipe1);
        routeout->push_back(new CompositeQueue(SERVICE1, queuesize, eventlist,NULL));
        routeout->push_back(new Pipe(RTT1, eventlist));
        routeout->push_back(ndpSnk);
        
        routein  = new route_t();
        routein->push_back(&pipe2);
        routein->push_back(ndpSrc); 

        ndpSrc->connect(routeout, routein, *ndpSnk,timeFromUs(0.0));
        sinkLogger.monitorSink(ndpSnk);
    }

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize="+ntoa(pktsize)+" bytes");
    //        logfile.write("# buffer2="+ntoa((double)(queue2._maxsize)/((double)pktsize))+" pkt");
    double rtt = timeAsSec(RTT1);
    logfile.write("# rtt="+ntoa(rtt));

    // GO!
    while (eventlist.doNextEvent()) {}

    cout << "Done" << endl;

    int new_pkts = 0, rtx_pkts = 0, bounce_pkts = 0;
    for (size_t ix = 0; ix < ndp_srcs.size(); ix++) {
        new_pkts += ndp_srcs[ix]->_new_packets_sent;
        rtx_pkts += ndp_srcs[ix]->_rtx_packets_sent;
        bounce_pkts += ndp_srcs[ix]->_bounces_received;
    }
    cout << "New: " << new_pkts << " Rtx: " << rtx_pkts << " Bounced: " << bounce_pkts << endl;

}
