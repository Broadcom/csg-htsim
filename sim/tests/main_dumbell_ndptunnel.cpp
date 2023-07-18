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
#include "tcp_transfer.h"
#include "clock.h"
#include "ndptunnel.h"
#include "compositequeue.h"

// Simulation params

int main(int argc, char **argv) {
    EventList eventlist;
    eventlist.setEndtime(timeFromSec(5));
    Clock c(timeFromSec(50/100.), eventlist);

    unsigned seed = time(NULL);

    int i = 1;
    while (i < argc) {
        if (!strcmp(argv[i], "-seed")) {
            seed = atoi(argv[i + 1]);
            cout << "random seed " << seed << endl;
            i++;
        }
        i++;
    }

    srand(seed);

    Packet::set_packet_size(9000);    
    linkspeed_bps SERVICE1 = speedFromMbps((uint64_t)10000);

    simtime_picosec RTT1=timeFromUs((uint32_t)1);
    mem_b BUFFER=memFromPkt(15);

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

    Queue queue3(SERVICE1, BUFFER*20, eventlist,NULL); queue3.setName("Queue3"); logfile.writeName(queue3);
    
    NdpTunnelSrc* ndpSrc;
    NdpTunnelSink* ndpSnk;
    TcpSrc* tcpSrc;
    TcpSink* tcpSnk;
    
    NdpTunnelRtxTimerScanner ndpRtxScanner(timeFromMs(1),eventlist);
    TcpRtxTimerScanner tcpRtxScanner(timeFromMs(10), eventlist);

    TcpSinkLoggerSampling sinkLogger = TcpSinkLoggerSampling(timeFromMs(100),eventlist);
    logfile.addLogger(sinkLogger);

    
    route_t* routeout;
    route_t* routein;

    NdpTunnelPullPacer pacer(eventlist, SERVICE1, 1);
    
    for (int i=0;i<1000;i++){
        ndpSrc = new NdpTunnelSrc(NULL,NULL,eventlist);
        ndpSrc->setRouteStrategy(SINGLE_PATH);
        ndpSrc->setName("NDP");
        ndpSrc->setCwnd(15*Packet::data_packet_size());
        logfile.writeName(*ndpSrc);

        ndpSnk = new NdpTunnelSink(&pacer); 
        ndpSnk->setName("NdpTunnelSink");
        ndpSnk->setRouteStrategy(SINGLE_PATH);
        logfile.writeName(*ndpSnk);
        
        ndpRtxScanner.registerNdp(*ndpSrc);

        tcpSrc = new TcpSrc(NULL,NULL,eventlist);
        //tcpSrc = new TcpSrcTransfer(NULL,NULL,eventlist,90000,NULL,NULL);

        tcpSrc->setName("TCP"+ntoa(i)); logfile.writeName(*tcpSrc);
        tcpSnk = new TcpSink();
        //tcpSnk = new TcpSinkTransfer();
        tcpSnk->setName("TCPSink"+ntoa(i)); logfile.writeName(*tcpSnk);

        tcpRtxScanner.registerTcp(*tcpSrc);
        
        // tell it the route
        routeout = new route_t(); 
        routeout->push_back(new PriorityQueue(SERVICE1, memFromPkt(1000),eventlist, NULL));
        routeout->push_back(&queue); 
        routeout->push_back(&pipe1);
        routeout->push_back(ndpSnk);
        
        routein  = new route_t();
        routein->push_back(&pipe2);
        routein->push_back(ndpSrc); 

        ndpSrc->connect(*routeout, *routein, *ndpSnk,0);

        routeout = new route_t(); 
        routeout->push_back(ndpSrc); 
        routeout->push_back(tcpSnk);

        routein  = new route_t();
        routein->push_back(tcpSrc);

        tcpSrc->connect(*routeout,*routein,*tcpSnk,drand()*timeFromMs(100));
        sinkLogger.monitorSink(tcpSnk);
    }

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize="+ntoa(pktsize)+" bytes");
    //        logfile.write("# buffer2="+ntoa((double)(queue2._maxsize)/((double)pktsize))+" pkt");
    double rtt = timeAsSec(RTT1);
    logfile.write("# rtt="+ntoa(rtt));

    // GO!
    while (eventlist.doNextEvent()) {}
}
