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
    eventlist.setEndtime(timeFromSec(0.5));
    Clock c(timeFromSec(50/100.), eventlist);

    srand(time(NULL));

    Packet::set_packet_size(9000);    
    linkspeed_bps SERVICE1 = speedFromMbps((uint64_t)10000);

    simtime_picosec RTT1=timeFromUs((uint32_t)1);
    mem_b BUFFER=memFromPkt(10);

    stringstream filename(ios_base::out);
    filename << "logout.dat";
    cout << "Outputting to " << filename.str() << endl;
    Logfile logfile(filename.str(),eventlist);
  
    logfile.setStartTime(timeFromSec(0.0));
    //TrafficLoggerSimple logger;

    //logfile.addLogger(logger);
    //QueueLoggerSampling qs1 = QueueLoggerSampling(timeFromMs(10),eventlist);logfile.addLogger(qs1);
    // Build the network

    Pipe pipe1(RTT1, eventlist); pipe1.setName("pipe1"); logfile.writeName(pipe1);
    Pipe pipe2(RTT1, eventlist); pipe2.setName("pipe2"); logfile.writeName(pipe2);

    CompositeQueue queue(SERVICE1, BUFFER, eventlist,NULL); queue.setName("Queue1"); logfile.writeName(queue);

    NdpSrc* ndpSrc[2];
    NdpSink* ndpSnk;
    NdpSinkLoggerSampling sinkLogger(timeFromUs((uint32_t)10),eventlist);
    logfile.addLogger(sinkLogger);
    NdpRtxTimerScanner ndpRtxScanner(timeFromMs(1),eventlist);
    route_t* routeout;
    route_t* routein;


    for (int i=0;i<2;i++){
        ndpSrc[i] = new NdpSrc(NULL,NULL,eventlist);
        ndpSrc[i]->setRouteStrategy(SINGLE_PATH);
        ndpSrc[i]->setName("NDP"); 
        logfile.writeName(*ndpSrc[i]);

        ndpSnk = new NdpSink(eventlist, SERVICE1, 1); 
        ndpSnk->setName("NdpSink");
        ndpSnk->setRouteStrategy(SINGLE_PATH);
        logfile.writeName(*ndpSnk);
        
        ndpRtxScanner.registerNdp(*ndpSrc[i]);
        
        // tell it the route
        routeout = new route_t();
        // NDP expects each src host to have a FairPriorityQueue
        routeout->push_back(new FairPriorityQueue(SERVICE1, memFromPkt(1000),eventlist, NULL));
        routeout->push_back(&queue); 
        routeout->push_back(&pipe1);
        routeout->push_back(ndpSnk);
        
        routein  = new route_t();
        routein->push_back(&pipe2);
        routein->push_back(ndpSrc[i]); 
        simtime_picosec starttime = 0;
        if (i == 1) {
            starttime = TRIGGER_START;
        }
        ndpSrc[i]->connect(routeout, routein, *ndpSnk, starttime);
        ndpSrc[i]->set_flowsize(2000000);
        sinkLogger.monitorSink(ndpSnk);
    }

    // we want the termination of the first connection to cause the second connection to start
    SingleShotTrigger trigger(eventlist, 0);
    ndpSrc[0]->set_end_trigger(trigger);
    trigger.add_target(*ndpSrc[1]);

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize="+ntoa(pktsize)+" bytes");
    //        logfile.write("# buffer2="+ntoa((double)(queue2._maxsize)/((double)pktsize))+" pkt");
    double rtt = timeAsSec(RTT1);
    logfile.write("# rtt="+ntoa(rtt));

    // GO!
    while (eventlist.doNextEvent()) {}
}
