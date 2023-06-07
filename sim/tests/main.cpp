// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "config.h"
#include <sstream>
#include <string.h>

#include <iostream>
#include <math.h>
#include "network.h"
#include "randomqueue.h"
#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "clock.h"
#include "mtcp.h"
#include "exoqueue.h"

#define CAP 1

// Simulation params
double targetwnd = 30;
int NUMFLOWS = 2;

#define TCP_1 0
#define TCP_2 0

#define RANDOM_BUFFER 3

#define FEEDER_BUFFER 2000

void exit_error(char* progr){
    cout << "Usage " << progr << " [UNCOUPLED(DEFAULT)|COUPLED_INC|FULLY_COUPLED|COUPLED_EPSILON] rate rtt" << endl;
    exit(1);
}

int main(int argc, char **argv) {
    EventList eventlist;
    eventlist.setEndtime(timeFromSec(60));
    Clock c(timeFromSec(50/100.), eventlist);
    int algo = UNCOUPLED;
    double epsilon = 1;
    int crt = 2;

    if (argc>1){
        if (!strcmp(argv[1],"UNCOUPLED"))
            algo = UNCOUPLED;
        else if (!strcmp(argv[1],"COUPLED_INC"))
            algo = COUPLED_INC;
        else if (!strcmp(argv[1],"FULLY_COUPLED"))
            algo = FULLY_COUPLED;
        else if (!strcmp(argv[1],"COUPLED_TCP"))
            algo = COUPLED_TCP;
        else if (!strcmp(argv[1],"COUPLED_EPSILON")) {
            algo = COUPLED_EPSILON;
            if (argc > 2){
                epsilon = atof(argv[2]);
                crt++;
                printf("Using epsilon %f\n",epsilon);
            }
        } else {
            exit_error(argv[0]);
        }
    }
    linkspeed_bps SERVICE1 = speedFromPktps(166);
    linkspeed_bps SERVICE2;
    if (argc>crt) 
        SERVICE2 = speedFromPktps(atoi(argv[crt++]));
    else
        SERVICE2 = speedFromPktps(400);

    simtime_picosec RTT1=timeFromMs(150);
    simtime_picosec RTT2;
    if (argc>crt)
        RTT2 = timeFromMs(atoi(argv[crt++]));
    else
        RTT2 = timeFromMs(10);
                      
    mem_b BUFFER1=memFromPkt(RANDOM_BUFFER+timeAsSec(RTT1)*speedAsPktps(SERVICE1)*12);//NUMFLOWS * targetwnd);

    int bufsize = timeAsSec(RTT2)*speedAsPktps(SERVICE2)*4; 
    if (bufsize<10) 
        bufsize = 10;
  
    mem_b BUFFER2=memFromPkt(RANDOM_BUFFER+bufsize);

    int rwnd;

    if (argc > crt)
        rwnd = atoi(argv[crt++]);
    else
        rwnd = 3 * timeAsSec(max(RTT1,RTT2)) * (speedAsPktps(SERVICE1)+speedAsPktps(SERVICE2));

    int run_paths = 2;
    //0 means run 3g only
    //1 means run wifi only
    if (argc > crt){
        run_paths = atoi(argv[crt++]);
    }

    srand(time(NULL));

    // prepare the loggers
    stringstream filename(ios_base::out);
    filename << "../data/logout." << speedAsPktps(SERVICE2) << "pktps." <<timeAsMs(RTT2) << "ms." << rwnd << "rwnd"; // rand();
    cout << "Outputting to " << filename.str() << endl;
    Logfile logfile(filename.str(),eventlist);
  
    logfile.setStartTime(timeFromSec(0.5));
    QueueLoggerSimple logQueue = QueueLoggerSimple(); logfile.addLogger(logQueue);
    //        QueueLoggerSimple logPQueue1 = QueueLoggerSimple(); logfile.addLogger(logPQueue1);
    //QueueLoggerSimple logPQueue3 = QueueLoggerSimple(); logfile.addLogger(logPQueue3);
    QueueLoggerSimple logPQueue = QueueLoggerSimple(); logfile.addLogger(logPQueue);
    MultipathTcpLoggerSimple mlogger = MultipathTcpLoggerSimple(); logfile.addLogger(mlogger);
  
    //TrafficLoggerSimple logger;

    Queue * pqueue;
    //logfile.addLogger(logger);
    TcpSinkLoggerSampling sinkLogger = TcpSinkLoggerSampling(timeFromMs(1000),eventlist);
    MemoryLoggerSampling memoryLogger = MemoryLoggerSampling(timeFromMs(10),eventlist);
    logfile.addLogger(sinkLogger);
    logfile.addLogger(memoryLogger);


    QueueLoggerSampling qs1 = QueueLoggerSampling(timeFromMs(1000),eventlist);logfile.addLogger(qs1);
    QueueLoggerSampling qs2 = QueueLoggerSampling(timeFromMs(1000),eventlist);logfile.addLogger(qs2);

    TcpLoggerSimple* logTcp = NULL;

    logTcp = new TcpLoggerSimple();
    logfile.addLogger(*logTcp);

    // Build the network
    Pipe pipe1(RTT1/2, eventlist); pipe1.setName("pipe1"); logfile.writeName(pipe1);
    Pipe pipe2(RTT2/2, eventlist); pipe2.setName("pipe2"); logfile.writeName(pipe2);
    Pipe pipe_back(timeFromMs(.1), eventlist); pipe_back.setName("pipe_back"); logfile.writeName(pipe_back);

    RandomQueue queue1(SERVICE1, BUFFER1, eventlist,&qs1,memFromPkt(RANDOM_BUFFER)); queue1.setName("Queue1"); logfile.writeName(queue1);

    RandomQueue queue2(SERVICE2, BUFFER2, eventlist,&qs2,memFromPkt(RANDOM_BUFFER)); queue2.setName("Queue2"); logfile.writeName(queue2);

    //ExoQueue queue22(0.001); 
    //        ExoQueue queue2(0.01);
    Queue pqueue2(SERVICE2*2, memFromPkt(FEEDER_BUFFER), eventlist,NULL); 
    pqueue2.setName("PQueue2"); 
    logfile.writeName(pqueue2);

    Queue pqueue3(SERVICE1*2, memFromPkt(FEEDER_BUFFER), eventlist,NULL); 
    pqueue3.setName("PQueue3"); 
    logfile.writeName(pqueue3);

    Queue pqueue4(SERVICE2*2, memFromPkt(FEEDER_BUFFER), eventlist,NULL); 
    pqueue4.setName("PQueue4"); 
    logfile.writeName(pqueue4);

    Queue queue_back(max(SERVICE1,SERVICE2)*4, memFromPkt(1000), 
                     eventlist,NULL); 
    queue_back.setName("queue_back"); 
    logfile.writeName(queue_back);

    TcpRtxTimerScanner tcpRtxScanner(timeFromMs(10), eventlist);

    //TCP flows on path 1
    TcpSrc* tcpSrc;
    TcpSink* tcpSnk;
    route_t* routeout;
    route_t* routein;
    double extrastarttime;

    for (int i=0;i<TCP_1;i++){
        tcpSrc = new TcpSrc(NULL,NULL,eventlist); 
        tcpSrc->setName("Tcp1"); 
        logfile.writeName(*tcpSrc);

        tcpSrc->_ssthresh = timeAsSec(RTT1) * speedAsPktps(SERVICE1) * 1000;
        tcpSnk = new TcpSink(); 
        tcpSnk->setName("TcpSink1"); 
        logfile.writeName(*tcpSnk);
        
        tcpRtxScanner.registerTcp(*tcpSrc);
        
        pqueue = new Queue(SERVICE1*2, memFromPkt(FEEDER_BUFFER), 
                           eventlist,NULL); 
        pqueue->setName("PQueue1_"+ntoa(i)); 
        logfile.writeName(*pqueue);

        // tell it the route
        routeout = new route_t(); 
        routeout->push_back(pqueue);
        routeout->push_back(&queue1); 
        routeout->push_back(&pipe1); 
        routeout->push_back(tcpSnk);
        
        routein  = new route_t(); 
        routein->push_back(&pipe1);
        routein->push_back(tcpSrc); 

        extrastarttime = drand()*50;
        tcpSrc->connect(*routeout, *routein, *tcpSnk, 
                        timeFromMs(extrastarttime));
        sinkLogger.monitorSink(tcpSnk);
        memoryLogger.monitorTcpSink(tcpSnk);
        memoryLogger.monitorTcpSource(tcpSrc);
    }

    //TCP flow on path 2
    for (int i=0;i<TCP_2;i++){
        tcpSrc = new TcpSrc(NULL, NULL, eventlist); 
        tcpSrc->setName("Tcp2"); 
        tcpSrc->_ssthresh = timeAsSec(RTT2) * speedAsPktps(SERVICE2) * 1000;
        logfile.writeName(*tcpSrc);

        tcpSnk = new TcpSink(); 
        tcpSnk->setName("TcpSink2"); 
        logfile.writeName(*tcpSnk);

        tcpRtxScanner.registerTcp(*tcpSrc);
        
        pqueue = new Queue(SERVICE2*2, memFromPkt(FEEDER_BUFFER), 
                           eventlist, NULL); 
        pqueue->setName("PQueue2_"+ntoa(i)); logfile.writeName(*pqueue);

        // tell it the route
        routeout = new route_t(); 
        routeout->push_back(pqueue); 
        routeout->push_back(&queue2); 
        routeout->push_back(&pipe2); 
        routeout->push_back(tcpSnk);
        
        routein  = new route_t(); //routein->push_back(&queue_back); routein->push_back(&pipe_back); 
        routein->push_back(&pipe2);
        routein->push_back(tcpSrc);

        extrastarttime = 50*drand();
        tcpSrc->connect(*routeout, *routein, *tcpSnk,
                        timeFromMs(extrastarttime));
        sinkLogger.monitorSink(tcpSnk);

        memoryLogger.monitorTcpSink(tcpSnk);
        memoryLogger.monitorTcpSource(tcpSrc);
    }

    MultipathTcpSrc* mtcp;
    MultipathTcpSink* mtcp_sink;
    mtcp = new MultipathTcpSrc(algo, eventlist, &mlogger, rwnd);
    mtcp->setName("MPTCPFlow");
    logfile.writeName(*mtcp);

    mtcp_sink = new MultipathTcpSink(eventlist);
    mtcp_sink->setName("mptcp_sink");
    logfile.writeName(*mtcp_sink);

    //MTCP flow 1
    tcpSrc = new TcpSrc(NULL,NULL,eventlist); 
    tcpSrc->setName("Subflow1"); 
    tcpSrc->_ssthresh = timeAsSec(RTT1) * speedAsPktps(SERVICE1) * 1000;
    logfile.writeName(*tcpSrc);

    tcpSnk = new TcpSink(); 
    tcpSnk->setName("Subflow1Sink"); 
    logfile.writeName(*tcpSnk);

    tcpSrc->_cap = CAP;
    tcpRtxScanner.registerTcp(*tcpSrc);
        
    // tell it the route
    routeout = new route_t(); 
    routeout->push_back(&pqueue3); 
    routeout->push_back(&queue1); 
    routeout->push_back(&pipe1); 
    routeout->push_back(tcpSnk);
        
    routein  = new route_t(); 
    //routein->push_back(&queue_back); routein->push_back(&pipe_back); 
    routein->push_back(&pipe1);
    routein->push_back(tcpSrc); 
    extrastarttime = 50*drand();

    //join multipath connection

    if (run_paths!=1){
        mtcp->addSubflow(tcpSrc);
        mtcp_sink->addSubflow(tcpSnk);
          
        tcpSrc->connect(*routeout,*routein,*tcpSnk,timeFromMs(extrastarttime));
        sinkLogger.monitorMultipathSink(tcpSnk);

        memoryLogger.monitorTcpSink(tcpSnk);
        memoryLogger.monitorTcpSource(tcpSrc);
    }
    //        BufferBloat * detect = new BufferBloat(eventlist,tcpSrc);

    //MTCP flow 2
    tcpSrc = new TcpSrc(NULL,NULL,eventlist); 
    tcpSrc->setName("Subflow2"); 
    tcpSrc->_ssthresh = timeAsSec(RTT2) * speedAsPktps(SERVICE2) * 1000;
    logfile.writeName(*tcpSrc);

    tcpSnk = new TcpSink(); 
    tcpSnk->setName("Subflow2Sink"); 
    logfile.writeName(*tcpSnk);

    tcpRtxScanner.registerTcp(*tcpSrc);
        
    // tell it the route
    routeout = new route_t(); 
    routeout->push_back(&pqueue4);
    routeout->push_back(&queue2); 
    routeout->push_back(&pipe2); 
    routeout->push_back(tcpSnk);
        
    routein  = new route_t(); 
    //routein->push_back(&queue_back); routein->push_back(&pipe_back); 
    routein->push_back(&pipe2);
    routein->push_back(tcpSrc); 
    extrastarttime = 50*drand();

    if (run_paths!=0){
        //join multipath connection
        mtcp->addSubflow(tcpSrc);
        mtcp_sink->addSubflow(tcpSnk);
          
        tcpSrc->connect(*routeout,*routein,*tcpSnk,timeFromMs(extrastarttime));
        sinkLogger.monitorMultipathSink(tcpSnk);

        memoryLogger.monitorTcpSink(tcpSnk);
        memoryLogger.monitorTcpSource(tcpSrc);
    }
    tcpSrc->_cap = CAP;

    //sinkLogger.monitorSink(mtcp_sink);
    mtcp->connect(mtcp_sink);

    memoryLogger.monitorMultipathTcpSink(mtcp_sink);
    memoryLogger.monitorMultipathTcpSource(mtcp);

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize="+ntoa(pktsize)+" bytes");
    logfile.write("# bottleneckrate1="+ntoa(speedAsPktps(SERVICE1))+" pkt/sec");
    logfile.write("# bottleneckrate2="+ntoa(speedAsPktps(SERVICE2))+" pkt/sec");
    logfile.write("# buffer1="+ntoa((double)(queue1._maxsize)/((double)pktsize))+" pkt");
    //        logfile.write("# buffer2="+ntoa((double)(queue2._maxsize)/((double)pktsize))+" pkt");
    double rtt = timeAsSec(RTT1);
    logfile.write("# rtt="+ntoa(rtt));
    rtt = timeAsSec(RTT2);
    logfile.write("# rtt="+ntoa(rtt));
    logfile.write("# numflows="+ntoa(NUMFLOWS));
    logfile.write("# targetwnd="+ntoa(targetwnd));

    // GO!
    while (eventlist.doNextEvent()) {}
}

