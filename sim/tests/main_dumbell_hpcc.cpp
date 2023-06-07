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
#include "hpcc.h"
#include "queue_lossless_input.h"
#include "queue_lossless_output.h"

// Simulation params

void exit_error(char* progr){
    cout << "Usage " << progr << " [UNCOUPLED(DEFAULT)|COUPLED_INC|FULLY_COUPLED|COUPLED_EPSILON] rate rtt" << endl;
    exit(1);
}

int main(int argc, char **argv) {
    EventList eventlist;
    eventlist.setEndtime(timeFromMs(10));
    Clock c(timeFromSec(50/100.), eventlist);

    srand(time(NULL));

    Packet::set_packet_size(9000);    
    linkspeed_bps SERVICE1 = speedFromMbps((uint64_t)100000);

    simtime_picosec RTT1=timeFromUs((uint32_t)1);
    mem_b BUFFER=memFromPkt(1000);

    stringstream filename(ios_base::out);
    filename << "logout.dat";
    cout << "Outputting to " << filename.str() << endl;
    Logfile logfile(filename.str(),eventlist);
  
    logfile.setStartTime(timeFromSec(0.0));

    Pipe pipe1(RTT1, eventlist); pipe1.setName("pipe1"); logfile.writeName(pipe1);
    Pipe pipe2(RTT1, eventlist); pipe2.setName("pipe2"); logfile.writeName(pipe2);

    LosslessOutputQueue queue(SERVICE1, BUFFER, eventlist,NULL); queue.setName("Queue1"); logfile.writeName(queue);
    LosslessOutputQueue queue2(SERVICE1, BUFFER, eventlist,NULL); queue2.setName("Queue2"); logfile.writeName(queue2);
    
    HPCCSrc * src;
    HPCCSink * snk;
    HPCCSinkLoggerSampling sinkLogger(timeFromUs((uint32_t)10),eventlist);
    logfile.addLogger(sinkLogger);

    LosslessInputQueue::_low_threshold = memFromPkt(22);
    LosslessInputQueue::_high_threshold = memFromPkt(25);
    route_t* routeout;
    route_t* routein;

    int flow_count = 2;

    if (argc>1)
        flow_count = atoi(argv[1]);

    cout << "Flow count " << flow_count << endl;

    for (int i=0;i<flow_count;i++){
        src = new HPCCSrc(NULL,NULL,eventlist,SERVICE1);
            //src->set_flowsize(20000000);
        src->setName("HPCC" + ntoa(i)); 
        logfile.writeName(*src);
        snk = new HPCCSink(); 
        snk->setName("HPCCSink" + ntoa(i));
        logfile.writeName(*snk);

        // tell it the route
        routeout = new route_t();

        routeout->push_back(new LosslessInputQueue(eventlist, src)); 
        routeout->push_back(&queue); 
        routeout->push_back(&pipe1);
        routeout->push_back(snk);
        
        routein  = new route_t();
        routeout->push_back(&queue2); 
        routein->push_back(&pipe1);
        routein->push_back(src); 

        src->connect(routeout, routein, *snk,0);
        sinkLogger.monitorSink(snk);
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

