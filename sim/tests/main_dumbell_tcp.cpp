
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
#include "randomqueue.h"
#include "compositequeue.h"

string ntoa(double n);
string itoa(uint64_t n);

// Simulation params

int main(int argc, char **argv) {
    EventList eventlist;
    eventlist.setEndtime(timeFromSec(5));
    Clock c(timeFromSec(50/100.), eventlist);

    int cnt = 10;
    int qs = 100;

    if (argc>1)
        cnt = atoi(argv[1]);

    if (argc>2)
        qs = atoi(argv[2]);
    
    srand(time(NULL));

    Packet::set_packet_size(9000);    
    linkspeed_bps SERVICE1 = speedFromMbps((uint64_t)10000);

    simtime_picosec RTT1=timeFromUs((uint32_t)10);
    mem_b BUFFER=memFromPkt(qs);

    stringstream filename(ios_base::out);
    filename << "logout.dat";
    cout << "Outputting to " << filename.str() << endl;
    Logfile logfile(filename.str(),eventlist);
  
    logfile.setStartTime(0);
    //TrafficLoggerSimple logger;

    //logfile.addLogger(logger);
    //QueueLoggerSampling qs1 = QueueLoggerSampling(timeFromMs(10),eventlist);logfile.addLogger(qs1);
    // Build the network

    Pipe pipe1(RTT1, eventlist); pipe1.setName("pipe1"); logfile.writeName(pipe1);
    Pipe pipe2(RTT1, eventlist); pipe2.setName("pipe2"); logfile.writeName(pipe2);

    RandomQueue queue3(SERVICE1, BUFFER, eventlist,NULL,memFromPkt(5)); queue3.setName("Queue3"); logfile.writeName(queue3);
    RandomQueue queue4(SERVICE1/3, BUFFER/3, eventlist,NULL,memFromPkt(0)); queue3.setName("Queue4"); logfile.writeName(queue4);
    
    
    TcpSrc* tcpSrc;
    TcpSink* tcpSnk;
    
    TcpRtxTimerScanner tcpRtxScanner(timeFromMs(10), eventlist);
    TcpSinkLoggerSampling sinkLogger = TcpSinkLoggerSampling(timeFromMs(100),eventlist);
    logfile.addLogger(sinkLogger);
    
    route_t* routeout;
    route_t* routein;

    for (int i=0;i<cnt;i++){
        tcpSrc = new TcpSrc(NULL,NULL,eventlist);
        //tcpSrc = new TcpSrcTransfer(NULL,NULL,eventlist,90000,NULL,NULL);

        tcpSrc->setName("TCP"+ntoa(i)); logfile.writeName(*tcpSrc);
        tcpSnk = new TcpSink();
        //tcpSnk = new TcpSinkTransfer();
        tcpSnk->setName("TCPSink"+ntoa(i)); logfile.writeName(*tcpSnk);

        tcpRtxScanner.registerTcp(*tcpSrc);
        
        // tell it the route
        routeout = new route_t();

        if (!i)
            routeout->push_back(&queue4); 
        else
            routeout->push_back(new Queue(SERVICE1, BUFFER*10, eventlist,NULL));
        
        routeout->push_back(&queue3); 
        routeout->push_back(&pipe1);

        routeout->push_back(tcpSnk);
        
        routein  = new route_t();
        routein->push_back(&pipe2);
        routein->push_back(tcpSrc); 

        tcpSrc->connect(*routeout,*routein,*tcpSnk,drand()*timeFromMs(1));
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
