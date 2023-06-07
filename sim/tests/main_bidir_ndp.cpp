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
#include "ndp_transfer.h"
#include "clock.h"
#include "ndptunnel.h"
#include "compositequeue.h"

void exit_error(char* progr) {
    cout << "Usage " << progr << " -hdiv <h_divisor> -cwnd<initial_window>" << endl;
    exit(1);
}

// Simulation params

int main(int argc, char **argv) {
    EventList eventlist;
    eventlist.setEndtime(timeFromSec(0.130));
    Clock c(timeFromSec(50/100.), eventlist);

   int qs = 35;

    int cwnd = 50;
    stringstream filename(ios_base::out);
    filename << "logout.dat";
    int i = 1;
    int psize = 4000;
    while (i < argc) {
        if (!strcmp(argv[i],"-o")){
            filename.str(std::string());
            filename << argv[i+1];
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

                                               
    Pipe *pipe[20];
    Queue *queue[20];
    QueueLoggerSampling *queueLogger;
    simtime_picosec LINK_DELAY = RTT1/4;
    // we may not use all of these...
    for (int i = 0; i < 20; i++) {
        pipe[i] = new Pipe(LINK_DELAY, eventlist);
        string s = "pipe" + std::to_string(i);
        pipe[i]->setName(s);
        logfile.writeName(*pipe[i]);

        if (i == 0 || i == 13) {
            queueLogger = new QueueLoggerSampling(timeFromUs((uint32_t)50), eventlist);
            logfile.addLogger(*queueLogger);
            cout << "add queue logger\n";
            queue[i] = new FairPriorityQueue(SERVICE1, memFromPkt(1000),eventlist, queueLogger);
        } else {
            //queueLogger = new QueueLoggerSampling(timeFromUs((uint32_t)50), eventlist);
            //logfile.addLogger(*queueLogger);
            queue[i] = new CompositeQueue(SERVICE1, BUFFER, eventlist, NULL);
        }
        //queue[i] = new Queue(SERVICE1, BUFFER, eventlist, ql);
        s = "queue" + std::to_string(i);
        queue[i]->setName(s);
        logfile.writeName(*queue[i]);
    }


    
    NdpSrc* ndpSrc[4];
    NdpSink* ndpSnk[4];
    
    NdpRtxTimerScanner ndpRtxScanner(timeFromUs((uint32_t)100), eventlist);
    NdpSinkLoggerSampling sinkLogger = NdpSinkLoggerSampling(timeFromUs((uint32_t)100),eventlist);
    logfile.addLogger(sinkLogger);
    
    route_t* routeout;
    route_t* routein;

    // Set up flows
    for (int i = 0; i < 4; i++) {
        ndpSrc[i] = new NdpSrc(NULL,NULL,eventlist);
        ndpSrc[i]->setName("NDP" + std::to_string(i));
        ndpSrc[i]->setCwnd(Packet::data_packet_size()*cwnd);
        ndpSrc[i]->setRouteStrategy(SINGLE_PATH);
        logfile.writeName(*ndpSrc[i]);

        ndpSnk[i] = new NdpSink(eventlist, SERVICE1, 0.99);
        ndpSnk[i]->setName("NDPSink" + std::to_string(i));
        ndpSnk[i]->setRouteStrategy(SINGLE_PATH);
        logfile.writeName(*ndpSnk[i]);
        ndpRtxScanner.registerNdp(*ndpSrc[i]);
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
        routeout->push_back(ndpSnk[0]); 
        
        routein  = new route_t();
        routein->push_back(queue[13]); 
        routein->push_back(pipe[13]);
        routein->push_back(queue[12]); 
        routein->push_back(pipe[12]);
        routein->push_back(queue[11]); 
        routein->push_back(pipe[11]);
        routein->push_back(queue[10]); 
        routein->push_back(pipe[10]);
        routein->push_back(ndpSrc[0]); 

        ndpSrc[0]->connect(routeout,routein,*ndpSnk[0],timeFromMs((int)(40)));
        ndpSrc[0]->set_stoptime(timeFromMs((int)70));
        sinkLogger.monitorSink(ndpSnk[0]);

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
        routeout->push_back(ndpSnk[1]); 

        routein = new route_t();
        routein->push_back(queue[0]); 
        routein->push_back(pipe[0]);
        routein->push_back(queue[1]); 
        routein->push_back(pipe[1]);
        routein->push_back(queue[2]); 
        routein->push_back(pipe[2]);
        routein->push_back(queue[3]); 
        routein->push_back(pipe[3]);
        routein->push_back(ndpSrc[1]); 
        
        ndpSrc[1]->connect(routeout,routein,*ndpSnk[1],timeFromMs((int)(10)));
        ndpSrc[1]->set_stoptime(timeFromMs((int)100));
        sinkLogger.monitorSink(ndpSnk[1]);
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
