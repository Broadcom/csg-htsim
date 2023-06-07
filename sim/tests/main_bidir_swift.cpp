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

void exit_error(char* progr) {
    cout << "Usage " << progr << " -hdiv <h_divisor> -cwnd <initial_window>" << endl;
    exit(1);
}

// Simulation params

int main(int argc, char **argv) {
    EventList eventlist;
    eventlist.setEndtime(timeFromSec(0.130));
    Clock c(timeFromSec(50/100.), eventlist);

    int qs = 1000;

    int cwnd = 12;
    stringstream filename(ios_base::out);
    filename << "logout.dat";
    int i = 1;
    double hdiv = 6.55;
    int psize = 4000;
    while (i < argc) {
        if (!strcmp(argv[i],"-o")){
            filename.str(std::string());
            filename << argv[i+1];
            i++;
        } else if (!strcmp(argv[i],"-hdiv")){
            hdiv = atof(argv[i+1]);
            cout << "hdiv "<< hdiv << endl;
            i++;
        } else if (!strcmp(argv[i],"-cwnd")){
            cwnd = atoi(argv[i+1]);
            cout << "cwnd "<< cwnd << endl;
            i++;
        } else if (!strcmp(argv[i],"-psize")){
            psize = atoi(argv[i+1]);
            cout << "psize "<< psize << endl;
            i++;
        } else {
            cout << argv[i] << endl;
            exit_error(argv[0]);
        }
        i++;
    }
    
    
    srand(time(NULL));

    Packet::set_packet_size(psize);    
    linkspeed_bps SERVICE1 = speedFromMbps((uint64_t)100000);

    // RTT1 is the two-way delay over the four hops (there and back)
    // within a rack
    simtime_picosec RTT1=timeFromUs((uint32_t)5);
    mem_b BUFFER=memFromPkt(qs);

    cout << "Outputting to " << filename.str() << endl;
    Logfile logfile(filename.str(),eventlist);
  
    logfile.setStartTime(timeFromSec(0.0));
    QueueLoggerSampling queueLogger(timeFromUs((uint32_t)10), eventlist);
    logfile.addLogger(queueLogger);

    // Topology 1
    //
    // Aim of this test is to have two different
    // length paths and have swift senders compete through a
    // bottleneck.  This tests swift's hop count compensation of
    // _target_delay.
    //
    // Topology:
    //         P0           P1             P2            P3
    //   S1 --------> sw ---------> sw ---------> sw ---------> dst
    //                                            ^  
    //                                     P4     |  
    //                              S2 -----------+
    //

    //  This emulates two competing flows in a fat tree, where S2 is
    //  in the same rack as dst and S1 is in a different rack.  We'll
    //  assume all the links have equal latency and zero latency in
    //  switches.  Reverse path pipe numbers are fwd + 10.  Queue
    //  numbers are the pipes they feed.

    // Topology 2
    //
    // Topology:
    //            P5          P6     P7     P8       P9                               
    //   S3 -->sw --> sw --> sw --> sw -----------+
    //                                            |  
    //         P0           P1             P2     V       P3
    //   S1 --------> sw ---------> sw ---------> sw ---------> dst
    //                                            ^  
    //                                     P4     |  
    //                              S2 -----------+
    //
    //  This emulates three competing flows in a fat tree, where S2 is
    //  in the same rack as dst and S1 is in a different rack, and S3
    //  traverses a spine.  We'll assume all the links have equal
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
    for (int i = 0; i < 4; i++) {
        swiftSrc[i] = new SwiftSrc(swiftRtxScanner, NULL,NULL,eventlist);
        swiftSrc[i]->setName("SWIFT" + std::to_string(i));
        swiftSrc[i]->set_cwnd(cwnd);
        swiftSrc[i]->set_hdiv(hdiv);
        logfile.writeName(*swiftSrc[i]);

        swiftSnk[i] = new SwiftSink();
        swiftSnk[i]->setName("SWIFTSink" + std::to_string(i));
        logfile.writeName(*swiftSnk[i]);
    }

    for (int i = 0; i < 1; i++) {
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

        swiftSrc[0]->connect(*routeout,*routein,*swiftSnk[0],timeFromMs((int)(40)));
        swiftSrc[0]->set_stoptime(timeFromMs((int)70));
        sinkLogger.monitorSink(swiftSnk[0]);

        // tell it the route
        routeout  = new route_t();
        routeout->push_back(queue[13]); 
        routeout->push_back(pipe[13]);
        routeout->push_back(queue[12]); 
        routeout->push_back(pipe[12]);
        routeout->push_back(queue[11]); 
        routeout->push_back(pipe[11]);
        routeout->push_back(queue[10]); 
        routeout->push_back(pipe[10]);
        //routeout->push_back(swiftSnk[1]); 

        routein = new route_t();
        routein->push_back(queue[0]); 
        routein->push_back(pipe[0]);
        routein->push_back(queue[1]); 
        routein->push_back(pipe[1]);
        routein->push_back(queue[2]); 
        routein->push_back(pipe[2]);
        routein->push_back(queue[3]); 
        routein->push_back(pipe[3]);
        //routein->push_back(swiftSrc[1]); 
        
        swiftSrc[1]->connect(*routeout,*routein,*swiftSnk[1],timeFromMs((int)(10)));
        swiftSrc[1]->set_stoptime(timeFromMs((int)100));
        sinkLogger.monitorSink(swiftSnk[1]);
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
