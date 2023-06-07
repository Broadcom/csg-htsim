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
#include "strack.h"
#include "clock.h"
#include "ndptunnel.h"
#include "compositequeue.h"

void exit_error(char* progr) {
    cout << "Usage " << progr << " -conns <no_of_connections> -cwnd <initial_window>" << endl;
    exit(1);
}


// Simulation params

int main(int argc, char **argv) {
    int cwnd = 12;
    int no_of_conns = 20;
    stringstream filename(ios_base::out);
    filename << "logout.dat";
    int i = 1;
    while (i < argc) {
        if (!strcmp(argv[i],"-o")){
            filename.str(std::string());
            filename << argv[i+1];
            i++;
        } else if (!strcmp(argv[i],"-conns")){
            no_of_conns = atoi(argv[i+1]);
            cout << "no_of_conns "<<no_of_conns << endl;
            i++;
        } else if (!strcmp(argv[i],"-cwnd")){
            cwnd = atoi(argv[i+1]);
            cout << "cwnd "<< cwnd << endl;
            i++;
        } else {
            cout << argv[i] << endl;
            exit_error(argv[0]);
        }
        i++;
    }
        
    EventList eventlist;
    eventlist.setEndtime(timeFromSec(0.2));
    Clock c(timeFromSec(50/100.), eventlist);

    int qs = 1000;

    //if (argc>1)
    //        qs = atoi(argv[1]);
    
    srand(time(NULL));

    Packet::set_packet_size(4000);    
    linkspeed_bps SERVICE1 = speedFromMbps((uint64_t)10000);

    simtime_picosec RTT1=timeFromUs((uint32_t)10);
    mem_b BUFFER=memFromPkt(qs);

    cout << "Outputting to " << filename.str() << endl;
    Logfile logfile(filename.str(),eventlist);
  
    logfile.setStartTime(timeFromSec(0.0));
    QueueLoggerSampling queueLogger(timeFromUs((uint32_t)10), eventlist);
    logfile.addLogger(queueLogger);

    //logfile.addLogger(logger);
    //QueueLoggerSampling qs1 = QueueLoggerSampling(timeFromMs(10),eventlist);logfile.addLogger(qs1);
    // Build the network

    Pipe* in_pipes[no_of_conns];
    FairScheduler* in_queues[no_of_conns];

    Pipe bottleneck_pipe(RTT1/4, eventlist);
    bottleneck_pipe.setName("b_pipe");
    logfile.writeName(bottleneck_pipe);

    Pipe rev_pipe(RTT1/2, eventlist);
    rev_pipe.setName("rev_pipe");
    logfile.writeName(rev_pipe);

    Queue bottleneck_queue(SERVICE1, BUFFER, eventlist, &queueLogger);
    bottleneck_queue.setName("Queue");
    logfile.writeName(bottleneck_queue);
    
    STrackSrc* strackSrc;
    STrackSink* strackSnk;
    
    STrackRtxTimerScanner strackRtxScanner(timeFromMs(10), eventlist);
    STrackSinkLoggerSampling sinkLogger = STrackSinkLoggerSampling(timeFromUs((uint32_t)1000),eventlist);
    logfile.addLogger(sinkLogger);
    
    route_t* routeout;
    route_t* routein;

    for (int i=0;i<no_of_conns;i++){
        strackSrc = new STrackSrc(strackRtxScanner, NULL,NULL,eventlist);
        strackSrc->set_cwnd(cwnd*Packet::data_packet_size());
        strackSrc->set_base_rtt(RTT1);
        strackSrc->setName("STRACK"+ntoa(i)); logfile.writeName(*strackSrc);
        
        strackSnk = new STrackSink();
        //strackSnk = new STrackSinkTransfer();
        strackSnk->setName("STRACKSink"+ntoa(i)); logfile.writeName(*strackSnk);

        in_pipes[i] = new Pipe(RTT1/4, eventlist);
        in_pipes[i]->setName("i_pipe");
        logfile.writeName(*in_pipes[i]);
        
        in_queues[i] = new FairScheduler(SERVICE1, eventlist, NULL);  // buffer size unlimited
        in_queues[i]->setName("i_queue");
        logfile.writeName(*in_queues[i]);

        // tell it the route
        routeout = new route_t(); 
        routeout->push_back(in_queues[i]); 
        routeout->push_back(in_pipes[i]);
        routeout->push_back(&bottleneck_queue); 
        routeout->push_back(&bottleneck_pipe);
        //routeout->push_back(strackSnk);
        
        routein  = new route_t();
        routein->push_back(&rev_pipe);
        //routein->push_back(strackSrc); 
        
        strackSrc->connect(*routeout,*routein,*strackSnk,timeFromMs(i*5));//drand()*timeFromMs(1));
        sinkLogger.monitorSink(strackSnk);
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
