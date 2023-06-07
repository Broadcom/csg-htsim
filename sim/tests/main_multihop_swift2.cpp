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
#include "swift_transfer.h"
#include "clock.h"
#include "ndptunnel.h"
#include "compositequeue.h"

// Simulation params

int main(int argc, char **argv) {
    EventList eventlist;
    eventlist.setEndtime(timeFromSec(0.051));
    Clock c(timeFromSec(50/100.), eventlist);

    int qs = 1000;

    if (argc>1)
        qs = atoi(argv[1]);
    
    srand(time(NULL));

    Packet::set_packet_size(4000);    
    linkspeed_bps SERVICE1 = speedFromMbps((uint64_t)100000);

    // RTT1 is the two-way delay over the four hops (there and back)
    // within a rack
    simtime_picosec RTT1=timeFromUs((uint32_t)5);
    mem_b BUFFER=memFromPkt(qs);

    stringstream filename(ios_base::out);
    filename << "logout.dat";
    cout << "Outputting to " << filename.str() << endl;
    Logfile logfile(filename.str(),eventlist);
  
    logfile.setStartTime(timeFromSec(0.0));
    QueueLoggerSampling queueLogger(timeFromUs((uint32_t)10), eventlist);
    logfile.addLogger(queueLogger);
    QueueLoggerSampling queueLogger2(timeFromUs((uint32_t)10), eventlist);
    logfile.addLogger(queueLogger2);

    // Topology 3
    //
    // Topology:
    //               P5                         P6            P7
    //   S3 ----------+             +-----------> sw ---------> D3
    //                |             |             
    //         P0     V      P1     |        P2          P3
    //   S1 --------> sw ---------> sw ---------> sw ---------> D1,D2
    //                                            ^  
    //                                     P4     |  
    //                              S2 -----------+
    //
    //  This emulates three competing flows in a fat tree, where S1
    //  and S2 are in the same rack and S2 and (D1,D2) are in the same
    //  rack.  The end result is that S1 experiences congestion at the
    //  src ToR and at the dst.  We'll assume all the links have equal
    //  latency and zero latency in switches.  Reverse path pipe
    //  numbers are fwd + 10.  Queue numbers are the pipes they feed.
                                               
    Pipe *pipe[20];
    Queue *queue[20];
    simtime_picosec LINK_DELAY = RTT1/4;
    // we may not use all of these...
    for (int i = 0; i < 20; i++) {
        pipe[i] = new Pipe(LINK_DELAY, eventlist);
        string s = "pipe" + std::to_string(i);
        pipe[i]->setName(s);
        logfile.writeName(*pipe[i]);

        QueueLoggerSampling *ql = NULL;
        if (i == 3) ql = &queueLogger;  // only log queue[3] as that's the bottleneck queue
        if (i == 1) ql = &queueLogger2;  // only log queue[3] as that's the bottleneck queue
        queue[i] = new Queue(SERVICE1, BUFFER, eventlist, ql);
        s = "queue" + std::to_string(i);
        queue[i]->setName(s);
        logfile.writeName(*queue[i]);
    }
    
    SwiftSrc* swiftSrc[4];
    SwiftSink* swiftSnk[4];
    
    SwiftRtxTimerScanner swiftRtxScanner(timeFromUs((uint32_t)100), eventlist);
    SwiftSinkLoggerSampling sinkLogger = SwiftSinkLoggerSampling(timeFromUs((uint32_t)100),eventlist);
    logfile.addLogger(sinkLogger);
    
    route_t* routeout;
    route_t* routein;

    // Set up flows
    for (int i = 0; i < 3; i++) {
        swiftSrc[i] = new SwiftSrc(swiftRtxScanner, NULL,NULL,eventlist);
        swiftSrc[i]->setName("SWIFT" + std::to_string(i));
        logfile.writeName(*swiftSrc[i]);

        swiftSnk[i] = new SwiftSink();
        swiftSnk[i]->setName("SWIFTSink" + std::to_string(i));
        logfile.writeName(*swiftSnk[i]);
    }

    // tell it the route
    routeout = new route_t();
    routeout->push_back(queue[0]); 
    routeout->push_back(pipe[0]);
    routeout->push_back(queue[1]); 
    routeout->push_back(pipe[1]);
    routeout->push_back(queue[2]); 
    routeout->push_back(pipe[2]);
    routeout->push_back(queue[3]); 
    routeout->push_back(pipe[3]);
    //routeout->push_back(swiftSnk[0]); 
        
    routein  = new route_t();
    routein->push_back(queue[13]); 
    routein->push_back(pipe[13]);
    routein->push_back(queue[12]); 
    routein->push_back(pipe[12]);
    routein->push_back(queue[11]); 
    routein->push_back(pipe[11]);
    routein->push_back(queue[10]); 
    routein->push_back(pipe[10]);
    //routein->push_back(swiftSrc[0]); 

    swiftSrc[0]->connect(*routeout,*routein,*swiftSnk[0],timeFromMs((int)(0)));
    swiftSrc[0]->set_stoptime(timeFromMs((int)50));
        
    sinkLogger.monitorSink(swiftSnk[0]);
    // tell it the route
    routeout = new route_t();
    routeout->push_back(queue[4]); 
    routeout->push_back(pipe[4]);
    routeout->push_back(queue[3]); 
    routeout->push_back(pipe[3]);
    //routeout->push_back(swiftSnk[1]); 
        
    routein  = new route_t();
    routein->push_back(queue[13]); 
    routein->push_back(pipe[13]);
    routein->push_back(queue[14]); 
    routein->push_back(pipe[14]);
    //routein->push_back(swiftSrc[1]); 

    swiftSrc[1]->connect(*routeout,*routein,*swiftSnk[1],timeFromMs((int)(10)));
    swiftSrc[1]->set_stoptime(timeFromMs((int)30));
    sinkLogger.monitorSink(swiftSnk[1]);

    // tell it the route
    routeout = new route_t();
    routeout->push_back(queue[5]); 
    routeout->push_back(pipe[5]);
    routeout->push_back(queue[1]); 
    routeout->push_back(pipe[1]);
    routeout->push_back(queue[6]); 
    routeout->push_back(pipe[6]);
    routeout->push_back(queue[7]); 
    routeout->push_back(pipe[7]);
    //routeout->push_back(swiftSnk[2]); 
        
    routein  = new route_t();
    routein->push_back(queue[17]); 
    routein->push_back(pipe[17]);
    routein->push_back(queue[16]); 
    routein->push_back(pipe[16]);
    routein->push_back(queue[11]); 
    routein->push_back(pipe[11]);
    routein->push_back(queue[15]); 
    routein->push_back(pipe[15]);
    //routein->push_back(swiftSrc[2]); 

    swiftSrc[2]->connect(*routeout,*routein,*swiftSnk[2],timeFromMs((int)(20)));
    swiftSrc[2]->set_stoptime(timeFromMs((int)40));
    sinkLogger.monitorSink(swiftSnk[2]);

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize="+ntoa(pktsize)+" bytes");
    //        logfile.write("# buffer2="+ntoa((double)(queue2._maxsize)/((double)pktsize))+" pkt");
    double rtt = timeAsSec(RTT1);
    logfile.write("# rtt="+ntoa(rtt));

    // GO!
    while (eventlist.doNextEvent()) {}
}
