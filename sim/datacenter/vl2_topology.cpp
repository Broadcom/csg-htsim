// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "vl2_topology.h"
#include <vector>
#include "string.h"
#include <sstream>
#include <iostream>
#include "main.h"

string ntoa(double n);
string itoa(uint64_t n);

VL2Topology::VL2Topology(Logfile* lg, EventList* ev,FirstFit * fit,simtime_picosec rtt){
    logfile = lg;
    eventlist = ev;
    ff = fit;
    _rtt = rtt;
    _no_of_nodes = NS * NT;

    init_network();
}

void VL2Topology::init_network(){
    QueueLoggerSampling* queueLogger;

    for (uint32_t j=0;j<NI;j++)
        for (uint32_t k=0;k<NA;k++){
            queues_ni_na[j][k] = NULL;
            pipes_ni_na[j][k] = NULL;
            queues_na_ni[k][j] = NULL;
            pipes_na_ni[k][j] = NULL;
        }
  
    for (uint32_t j=0;j<NA;j++)
        for (uint32_t k=0;k<NT;k++){
            queues_na_nt[j][k] = NULL;
            pipes_na_nt[j][k] = NULL;
            queues_nt_na[k][j] = NULL;
            pipes_nt_na[k][j] = NULL;
        }
  
    for (uint32_t j=0;j<NT;j++)
        for (uint32_t k=0;k<NS;k++){
            queues_nt_ns[j][k] = NULL;
            pipes_nt_ns[j][k] = NULL;
            queues_ns_nt[k][j] = NULL;
            pipes_ns_nt[k][j] = NULL;
        }

    //set all links/pipes to NULL

    // ToR switch to server
    for (uint32_t j = 0; j < NT; j++) {
        for (uint32_t k = 0; k < NS; k++) {
            // Downlink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);
            queues_nt_ns[j][k] = new RandomQueue(speedFromPktps(HOST_NIC), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
            queues_nt_ns[j][k]->setName("Queue-nt-ns-" + ntoa(j) + "-" + ntoa(k));
            logfile->writeName(*(queues_nt_ns[j][k]));

            pipes_nt_ns[j][k] = new Pipe(_rtt, *eventlist);
            pipes_nt_ns[j][k]->setName("Pipe-nt-ns-" + ntoa(j) + "-" + ntoa(k));
            logfile->writeName(*(pipes_nt_ns[j][k]));

            // Uplink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);
            queues_ns_nt[k][j] = new RandomQueue(speedFromPktps(HOST_NIC), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
            queues_ns_nt[k][j]->setName("Queue-ns-nt-" + ntoa(k) + "-" + ntoa(j));
            logfile->writeName(*(queues_ns_nt[k][j]));

            pipes_ns_nt[k][j] = new Pipe(_rtt, *eventlist);
            pipes_ns_nt[k][j]->setName("Pipe-ns-nt-" + ntoa(k) + "-" + ntoa(j));
            logfile->writeName(*(pipes_ns_nt[k][j]));

#if PRINT_TOPOLOGY    
            topology << HOST_ID(k,j) <<" "<< TOR_ID(j) << " "<<1 << endl;
            topology << TOR_ID(j) << " "<<HOST_ID(k,j) << " "<<1 << endl;
#endif

            if (ff){
                ff->add_queue(queues_nt_ns[j][k]);
                ff->add_queue(queues_ns_nt[k][j]);
            }
        }
    }

    //ToR switch to aggregation switch
    for (uint32_t j = 0; j < NT; j++) {
        //Connect the ToR switch to NT2A aggregation switches
        for (uint32_t l=0; l<NT2A;l++){
            uint32_t k;
            if (l==0)
                k = TOR_AGG1(j);
            else 
                k = TOR_AGG2(j);

            // Downlink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);
            queues_na_nt[k][j] = new RandomQueue(speedFromPktps(CORE_TO_HOST*HOST_NIC), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
            queues_na_nt[k][j]->setName("Queue-na-nt-" + ntoa(k) + "-" + ntoa(j));
            logfile->writeName(*(queues_na_nt[k][j]));
          

            pipes_na_nt[k][j] = new Pipe(_rtt, *eventlist);
            pipes_na_nt[k][j]->setName("Pipe-na-nt-" + ntoa(k) + "-" + ntoa(j));
            logfile->writeName(*(pipes_na_nt[k][j]));
          
            // Uplink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);
            queues_nt_na[j][k] = new RandomQueue(speedFromPktps(CORE_TO_HOST*HOST_NIC), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
            queues_nt_na[j][k]->setName("Queue-nt-na-" + ntoa(j) + "-" + ntoa(k));
            logfile->writeName(*(queues_nt_na[j][k]));
          
            pipes_nt_na[j][k] = new Pipe(_rtt, *eventlist);
            pipes_nt_na[j][k]->setName("Pipe-nt-na-" + ntoa(j) + "-" + ntoa(k));
            logfile->writeName(*(pipes_nt_na[j][k]));

#if PRINT_TOPOLOGY    
            topology << AGG_ID(k) << " "<<TOR_ID(j) << " "<< 2 << endl;
            topology << TOR_ID(j) << " "<<AGG_ID(k) << " "<< 2 << endl;
#endif
            if (ff){
                ff->add_queue(queues_nt_na[j][k]);
                ff->add_queue(queues_na_nt[k][j]);
            }
        }
    }
    
    // Aggregation switch to intermediate switch

    for (uint32_t j = 0; j < NI; j++) {
        for (uint32_t k = 0; k < NA; k++) {
            // Downlink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);

            queues_ni_na[j][k] = new RandomQueue(speedFromPktps(CORE_TO_HOST*HOST_NIC), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
            queues_ni_na[j][k]->setName("Queue-ni-na-" + ntoa(j) + "-" + ntoa(k));

            //          if (j==0)
            //queues_ni_na[j][k]->set_packet_loss_rate(0);

            logfile->writeName(*(queues_ni_na[j][k]));
          
            pipes_ni_na[j][k] = new Pipe(_rtt, *eventlist);
            pipes_ni_na[j][k]->setName("Pipe-ni-na-" + ntoa(j) + "-" + ntoa(k));
            logfile->writeName(*(pipes_ni_na[j][k]));
          
            // Uplink

            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);

            queues_na_ni[k][j] = new RandomQueue(speedFromPktps(CORE_TO_HOST*HOST_NIC), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
            queues_na_ni[k][j]->setName("Queue-na-ni-" + ntoa(k) + "-" + ntoa(j));
            logfile->writeName(*(queues_na_ni[k][j]));

            pipes_na_ni[k][j] = new Pipe(_rtt, *eventlist);
            pipes_na_ni[k][j]->setName("Pipe-na-ni-" + ntoa(k) + "-" + ntoa(j));
            logfile->writeName(*(pipes_na_ni[k][j]));

#if PRINT_TOPOLOGY    
            topology << AGG_ID(k) << " " <<INT_ID(j) << " "<<1 << endl;
            topology << INT_ID(j) << " " <<AGG_ID(k) << " "<<1 << endl;
#endif 
            if (ff){
                ff->add_queue(queues_ni_na[j][k]);
                ff->add_queue(queues_na_ni[k][j]);
            }
        }
    }
}

vector<const Route*>* VL2Topology::get_paths(uint32_t src, uint32_t dest){
    vector<const Route*>* paths = new vector<const Route*>();

    Route* routeout;

    if (HOST_TOR(src)==HOST_TOR(dest)){
        Queue* pqueue = new Queue(speedFromPktps(CORE_TO_HOST*HOST_NIC), memFromPkt(FEEDER_BUFFER), *eventlist, NULL);
        pqueue->setName("PQueue_" + ntoa(src) + "_" + ntoa(dest));
        logfile->writeName(*pqueue);
  
        routeout = new Route();
        routeout->push_back(pqueue);

        routeout->push_back(queues_ns_nt[HOST_TOR_ID(src)][HOST_TOR(src)]);
        routeout->push_back(pipes_ns_nt[HOST_TOR_ID(src)][HOST_TOR(src)]);

        routeout->push_back(queues_nt_ns[HOST_TOR(dest)][HOST_TOR_ID(dest)]);
        routeout->push_back(pipes_nt_ns[HOST_TOR(dest)][HOST_TOR_ID(dest)]);

        paths->push_back(routeout);
        return paths;
    }

    //  for (uint32_t i=0;i<2*NI;i++){
    for (uint32_t i=0;i<4*NI;i++){
        routeout = new Route();

        Queue* pqueue = new Queue(speedFromPktps(CORE_TO_HOST*HOST_NIC), memFromPkt(FEEDER_BUFFER), *eventlist, NULL);
        pqueue->setName("PQueue_" + ntoa(src) + "_" + ntoa(dest));
        logfile->writeName(*pqueue);
  
        routeout->push_back(pqueue);
  
        //server to TOR switch
        routeout->push_back(queues_ns_nt[HOST_TOR_ID(src)][HOST_TOR(src)]);
        routeout->push_back(pipes_ns_nt[HOST_TOR_ID(src)][HOST_TOR(src)]);

        assert(queues_ns_nt[HOST_TOR_ID(src)][HOST_TOR(src)]!=NULL);
        assert(pipes_ns_nt[HOST_TOR_ID(src)][HOST_TOR(src)]!=NULL);

        uint32_t agg_switch;
        //    if (src%NS<(NS/2))
        if (i<2*NI)
            //use link to first agg switch
            agg_switch = TOR_AGG1(HOST_TOR(src));
        else
            agg_switch = TOR_AGG2(HOST_TOR(src));
    
        //TOR switch to AGG switch
        routeout->push_back(queues_nt_na[HOST_TOR(src)][agg_switch]);
        routeout->push_back( pipes_nt_na[HOST_TOR(src)][agg_switch]);
    
        assert(queues_nt_na[HOST_TOR(src)][agg_switch]!=NULL);
        assert(pipes_nt_na[HOST_TOR(src)][agg_switch]!=NULL);
    
        //AGG to UINT32_T switch
        //now I need to connect to the choice1 server
        //0<=choice1<NI

        assert(queues_na_ni[agg_switch][i/4]!=NULL);
        assert(pipes_na_ni[agg_switch][i/4]!=NULL);    

        routeout->push_back(queues_na_ni[agg_switch][i/4]);
        routeout->push_back( pipes_na_ni[agg_switch][i/4]);    

        //now I need to connect to the choice2 agg server - this is more tricky!

        uint32_t agg_switch_2;
        if (i%NT2A==0)
            agg_switch_2 = TOR_AGG1(HOST_TOR(dest));
        else
            agg_switch_2 = TOR_AGG2(HOST_TOR(dest));
    
        assert(queues_ni_na[i/4][agg_switch_2]!=NULL);
        assert(pipes_ni_na[i/4][agg_switch_2]!=NULL);

        //INT to agg switch
        routeout->push_back(queues_ni_na[i/4][agg_switch_2]);
        routeout->push_back(pipes_ni_na[i/4][agg_switch_2]);

        //agg to TOR
        routeout->push_back(queues_na_nt[agg_switch_2][HOST_TOR(dest)]);
        routeout->push_back(pipes_na_nt[agg_switch_2][HOST_TOR(dest)]);
        assert(queues_na_nt[agg_switch_2][HOST_TOR(dest)]!=NULL);
        assert(pipes_na_nt[agg_switch_2][HOST_TOR(dest)]!=NULL);

        //tor to server
        routeout->push_back(queues_nt_ns[HOST_TOR(dest)][HOST_TOR_ID(dest)]);
        routeout->push_back(pipes_nt_ns[HOST_TOR(dest)][HOST_TOR_ID(dest)]);

        assert(queues_nt_ns[HOST_TOR(dest)][HOST_TOR_ID(dest)]!=NULL);
        assert(pipes_nt_ns[HOST_TOR(dest)][HOST_TOR_ID(dest)]!=NULL);

        paths->push_back(routeout);
    }

    return paths;
}

