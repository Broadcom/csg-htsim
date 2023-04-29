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
#include "swift_scheduler.h"
#include "clock.h"
#include "ndptunnel.h"
#include "compositequeue.h"

string ntoa(double n);
string itoa(uint64_t n);

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
    // Aim of this test is for S1 to have two paths to D1 and S2 to have one path.
    // We'll start two subflows for each sender, but D2's subflows will both take the same path.
    // Three subflows share link P7.  
    //
    // Topology:
    //         P0           P1             P2            P3           P4
    //   S1 --------> sw ---------> sw ---------> sw ---------> sw --------> D1
    //                 |     P10                        P11     ^ 
    //                 +------------+              +------------+
    //                              V              |
    //   S2 --------> sw ---------> sw ---------> sw ---------> sw --------> D2
    //          P5           P6            P7            P8           P9
    // reverse path links are fwd path +20
                                               
    Pipe *pipe[40];
    BaseQueue *queue[40];
    simtime_picosec LINK_DELAY = RTT1/4;
    // we may not use all of these...
    for (int i = 0; i < 40; i++) {
        pipe[i] = new Pipe(LINK_DELAY, eventlist);
        string s = "pipe" + std::to_string(i);
        pipe[i]->setName(s);
        logfile.writeName(*pipe[i]);

        QueueLoggerSampling *ql = NULL;
        if (i == 4) ql = &queueLogger;  // only log queue[3] as that's the bottleneck queue
        if (i == 0 || i == 5) {
            queue[i] = new FairScheduler(SERVICE1, eventlist, NULL); 
        } else {
            queue[i] = new Queue(SERVICE1, BUFFER, eventlist, ql);
        }
        s = "queue" + std::to_string(i);
        queue[i]->setName(s);
        logfile.writeName(*queue[i]);
    }
    
    SwiftSrc* swiftSrc[2];
    SwiftSink* swiftSnk[2];
    
    SwiftRtxTimerScanner swiftRtxScanner(timeFromUs((uint32_t)100), eventlist);
    SwiftSinkLoggerSampling sinkLogger = SwiftSinkLoggerSampling(timeFromUs((uint32_t)100),eventlist);
    logfile.addLogger(sinkLogger);
    
    route_t* routeout;
    route_t* routein;

    // Set up flows
    for (int i = 0; i < 2; i++) {
        swiftSrc[i] = new SwiftSrc(swiftRtxScanner, NULL,NULL,eventlist);
        swiftSrc[i]->setName("SWIFT" + std::to_string(i));
        swiftSrc[i]->set_cwnd(cwnd);
        swiftSrc[i]->set_hdiv(hdiv);
        logfile.writeName(*swiftSrc[i]);

        swiftSnk[i] = new SwiftSink();
        swiftSnk[i]->setName("SWIFTSink" + std::to_string(i));
        logfile.writeName(*swiftSnk[i]);
    }

    // Fwd path for S1a
    routeout = new route_t();
    routeout->push_back(queue[0]); 
    routeout->push_back(pipe[0]);
    routeout->push_back(queue[1]); 
    routeout->push_back(pipe[1]);
    routeout->push_back(queue[2]); 
    routeout->push_back(pipe[2]);
    routeout->push_back(queue[3]); 
    routeout->push_back(pipe[3]);
    routeout->push_back(queue[4]); 
    routeout->push_back(pipe[4]);
        
    routein  = new route_t();
    routein->push_back(queue[24]); 
    routein->push_back(pipe[24]);
    routein->push_back(queue[23]); 
    routein->push_back(pipe[23]);
    routein->push_back(queue[22]); 
    routein->push_back(pipe[22]);
    routein->push_back(queue[21]); 
    routein->push_back(pipe[21]);
    routein->push_back(queue[20]); 
    routein->push_back(pipe[20]);

    swiftSrc[0]->connect(*routeout,*routein,*swiftSnk[0],timeFromMs((int)(50)));

    // Fwd path for S1b
    routeout = new route_t();
    routeout->push_back(queue[0]); 
    routeout->push_back(pipe[0]);
    routeout->push_back(queue[10]); 
    routeout->push_back(pipe[10]);
    routeout->push_back(queue[7]); 
    routeout->push_back(pipe[7]);
    routeout->push_back(queue[11]); 
    routeout->push_back(pipe[11]);
    routeout->push_back(queue[4]); 
    routeout->push_back(pipe[4]);
        
    routein  = new route_t();
    routein->push_back(queue[24]); 
    routein->push_back(pipe[24]);
    routein->push_back(queue[31]); 
    routein->push_back(pipe[31]);
    routein->push_back(queue[27]); 
    routein->push_back(pipe[27]);
    routein->push_back(queue[30]); 
    routein->push_back(pipe[30]);
    routein->push_back(queue[20]); 
    routein->push_back(pipe[20]);

    swiftSrc[0]->connect(*routeout,*routein,*swiftSnk[0],timeFromMs((int)(50)));
    swiftSrc[0]->set_stoptime(timeFromMs((int)100));
    sinkLogger.monitorSink(swiftSnk[0]);

    routeout = new route_t();
    routeout->push_back(queue[5]); 
    routeout->push_back(pipe[5]);
    routeout->push_back(queue[6]); 
    routeout->push_back(pipe[6]);
    routeout->push_back(queue[7]); 
    routeout->push_back(pipe[7]);
    routeout->push_back(queue[8]); 
    routeout->push_back(pipe[8]);
    routeout->push_back(queue[9]); 
    routeout->push_back(pipe[9]);
        
    routein  = new route_t();
    routein->push_back(queue[29]); 
    routein->push_back(pipe[29]);
    routein->push_back(queue[28]); 
    routein->push_back(pipe[28]);
    routein->push_back(queue[27]); 
    routein->push_back(pipe[27]);
    routein->push_back(queue[26]); 
    routein->push_back(pipe[26]);
    routein->push_back(queue[25]); 
    routein->push_back(pipe[25]);

    // connect twice
    swiftSrc[1]->connect(*routeout,*routein,*swiftSnk[1],timeFromMs((int)(25)));
    swiftSrc[1]->connect(*routeout,*routein,*swiftSnk[1],timeFromMs((int)(25)));
    swiftSrc[1]->set_stoptime(timeFromMs((int)75));
    sinkLogger.monitorSink(swiftSnk[1]);

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
