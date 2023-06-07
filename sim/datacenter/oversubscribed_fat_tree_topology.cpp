// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "oversubscribed_fat_tree_topology.h"
#include <vector>
#include "string.h"
#include <sstream>

#include <iostream>
#include "main.h"
#include "queue.h"
#include "compositequeue.h"
#include "ecnqueue.h"

string ntoa(double n);
string itoa(uint64_t n);

OversubscribedFatTreeTopology::OversubscribedFatTreeTopology(mem_b queuesize, Logfile* lg, EventList* ev,FirstFit * fit,queue_type q,simtime_picosec rtt, int N){
    logfile = lg;
    eventlist = ev;
    ff = fit;
    _rtt = rtt;
    _N = N;
    
    _no_of_nodes = K * K * K;
  
    qt = q;
    _queuesize = queuesize;

    init_network();
}

Queue* OversubscribedFatTreeTopology::alloc_src_queue(QueueLogger* queueLogger){
    return  new PriorityQueue(speedFromMbps((uint64_t)HOST_NIC), memFromPkt(FEEDER_BUFFER), *eventlist, queueLogger);
}

Queue* OversubscribedFatTreeTopology::alloc_queue(QueueLogger* queueLogger){
    return alloc_queue(queueLogger, HOST_NIC);
}

Queue* OversubscribedFatTreeTopology::alloc_queue(QueueLogger* queueLogger, uint64_t speed){
    if (qt==RANDOM)
        return new RandomQueue(speedFromMbps(speed), _queuesize, *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
    else if (qt==COMPOSITE)
        return new CompositeQueue(speedFromMbps(speed), _queuesize, *eventlist, queueLogger);
    else if (qt==ECN)
        return new ECNQueue(speedFromMbps(speed), _queuesize, *eventlist, queueLogger, memFromPkt(15));
    assert(0);
}

void OversubscribedFatTreeTopology::init_network(){
    QueueLoggerSampling* queueLogger;

    for (uint32_t j=0;j<NC;j++)
        for (uint32_t k=0;k<NK;k++){
            queues_nc_nup[j][k] = NULL;
            pipes_nc_nup[j][k] = NULL;
            queues_nup_nc[k][j] = NULL;
            pipes_nup_nc[k][j] = NULL;
        }

    for (uint32_t j=0;j<NK;j++)
        for (uint32_t k=0;k<NK;k++){
            queues_nup_nlp[j][k] = NULL;
            pipes_nup_nlp[j][k] = NULL;
            queues_nlp_nup[k][j] = NULL;
            pipes_nlp_nup[k][j] = NULL;
        }
  
    for (uint32_t j=0;j<NK;j++)
        for (uint32_t k=0;k<NSRV;k++){
            queues_nlp_ns[j][k] = NULL;
            pipes_nlp_ns[j][k] = NULL;
            queues_ns_nlp[k][j] = NULL;
            pipes_ns_nlp[k][j] = NULL;
        }

    // lower layer pod switch to server
    for (uint32_t j = 0; j < NK; j++) {
        for (uint32_t l = 0; l < 2*K; l++) {
            uint32_t k = j * 2*K + l;
            // Downlink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            //queueLogger = NULL;
            logfile->addLogger(*queueLogger);

            queues_nlp_ns[j][k] = alloc_queue(queueLogger);
            queues_nlp_ns[j][k]->setName("LS_" + ntoa(j) + "-" + "DST_" +ntoa(k));
            logfile->writeName(*(queues_nlp_ns[j][k]));
          
            pipes_nlp_ns[j][k] = new Pipe(_rtt, *eventlist);
            pipes_nlp_ns[j][k]->setName("Pipe-nt-ns-" + ntoa(j) + "-" + ntoa(k));
            logfile->writeName(*(pipes_nlp_ns[j][k]));
          
            // Uplink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);
            queues_ns_nlp[k][j] = alloc_src_queue(queueLogger);
            queues_ns_nlp[k][j]->setName("SRC_" + ntoa(k) + "-" + "LS_"+ntoa(j));
            logfile->writeName(*(queues_ns_nlp[k][j]));
          
            pipes_ns_nlp[k][j] = new Pipe(_rtt, *eventlist);
            pipes_ns_nlp[k][j]->setName("Pipe-ns-nt-" + ntoa(k) + "-" + ntoa(j));
            logfile->writeName(*(pipes_ns_nlp[k][j]));
          
            if (ff){
                ff->add_queue(queues_nlp_ns[j][k]);
                ff->add_queue(queues_ns_nlp[k][j]);
            }
        }
    }

    /*    for (uint32_t i = 0;i<NSRV;i++){
          for (uint32_t j = 0;j<NK;j++){
          printf("%p/%p ",queues_ns_nlp[i][j], queues_nlp_ns[j][i]);
          }
          printf("\n");
          }*/
    
    //Lower layer in pod to upper layer in pod!
    for (uint32_t j = 0; j < NK; j++) {
        uint32_t podid = 2*j/K;
        //Connect the lower layer switch to the upper layer switches in the same pod
        for (uint32_t k=MIN_POD_ID(podid); k<=MAX_POD_ID(podid);k++){
            // Downlink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);
            queues_nup_nlp[k][j] = alloc_queue(queueLogger);
            
            queues_nup_nlp[k][j]->setName("US_" + ntoa(k) + "-" + "LS_"+ntoa(j));
            logfile->writeName(*(queues_nup_nlp[k][j]));
        
            pipes_nup_nlp[k][j] = new Pipe(_rtt, *eventlist);
            pipes_nup_nlp[k][j]->setName("Pipe-na-nt-" + ntoa(k) + "-" + ntoa(j));
            logfile->writeName(*(pipes_nup_nlp[k][j]));
        
            // Uplink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);
            queues_nlp_nup[j][k] = alloc_queue(queueLogger);

            queues_nlp_nup[j][k]->setName("LS_" + ntoa(j) + "-" + "US_"+ntoa(k));
            logfile->writeName(*(queues_nlp_nup[j][k]));
        
            pipes_nlp_nup[j][k] = new Pipe(_rtt, *eventlist);
            pipes_nlp_nup[j][k]->setName("Pipe-nt-na-" + ntoa(j) + "-" + ntoa(k));
            logfile->writeName(*(pipes_nlp_nup[j][k]));
        
            if (ff){
                ff->add_queue(queues_nlp_nup[j][k]);
                ff->add_queue(queues_nup_nlp[k][j]);
            }
        }
    }

    /*for (uint32_t i = 0;i<NK;i++){
      for (uint32_t j = 0;j<NK;j++){
      printf("%p/%p ",queues_nlp_nup[i][j], queues_nup_nlp[j][i]);
      }
      printf("\n");
      }*/
    
    // Upper layer in pod to core!
    for (uint32_t j = 0; j < NK; j++) {
        uint32_t podpos = j%(K/2);
        for (uint32_t l = 0; l < K/2; l++) {
            uint32_t k = podpos * K/2 + l;
            // Downlink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);

            queues_nup_nc[j][k] = alloc_queue(queueLogger);
            queues_nup_nc[j][k]->setName("US_" + ntoa(j) + "-" + "CS_"+ ntoa(k));
            logfile->writeName(*(queues_nup_nc[j][k]));
        
            pipes_nup_nc[j][k] = new Pipe(_rtt, *eventlist);
            pipes_nup_nc[j][k]->setName("Pipe-nup-nc-" + ntoa(j) + "-" + ntoa(k));
            logfile->writeName(*(pipes_nup_nc[j][k]));
        
            // Uplink
        
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);
        
            //        if (k==0&&j==0)
            //queues_nc_nup[k][j] = alloc_queue(queueLogger,HOST_NIC/10);
            //else
            queues_nc_nup[k][j] = alloc_queue(queueLogger);
            queues_nc_nup[k][j]->setName("CS_" + ntoa(k) + "-" + "US_"+ntoa(j));


            logfile->writeName(*(queues_nc_nup[k][j]));
        
            pipes_nc_nup[k][j] = new Pipe(_rtt, *eventlist);
            pipes_nc_nup[k][j]->setName("Pipe-nc-nup-" + ntoa(k) + "-" + ntoa(j));
            logfile->writeName(*(pipes_nc_nup[k][j]));
        
            if (ff){
                ff->add_queue(queues_nup_nc[j][k]);
                ff->add_queue(queues_nc_nup[k][j]);
            }
        }
    }

    /*    for (uint32_t i = 0;i<NK;i++){
          for (uint32_t j = 0;j<NC;j++){
          printf("%p/%p ",queues_nup_nc[i][j], queues_nc_nup[j][i]);
          }
          printf("\n");
          }*/
}

void check_non_null(Route* rt);

vector<const Route*>* OversubscribedFatTreeTopology::get_bidir_paths(uint32_t src, uint32_t dest, bool reverse){
    vector<const Route*>* paths = new vector<const Route*>();

    Route* routeout;

    if (HOST_POD_SWITCH(src)==HOST_POD_SWITCH(dest)){
        routeout = new Route();

        routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);
        routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);

        routeout->push_back(queues_nlp_ns[HOST_POD_SWITCH(dest)][dest]);
        routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);

        paths->push_back(routeout);

        check_non_null(routeout);
        return paths;
    }
    else if (HOST_POD(src)==HOST_POD(dest)){
        //don't go up the hierarchy, stay in the pod only.

        uint32_t pod = HOST_POD(src);
        //there are K/2 paths between the source and the destination
        for (uint32_t upper = MIN_POD_ID(pod);upper <= MAX_POD_ID(pod); upper++){
            routeout = new Route();
      
            routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);
            routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);

            routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][upper]);
            routeout->push_back(pipes_nlp_nup[HOST_POD_SWITCH(src)][upper]);

            routeout->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(dest)]);
            routeout->push_back(pipes_nup_nlp[upper][HOST_POD_SWITCH(dest)]);
      
            routeout->push_back(queues_nlp_ns[HOST_POD_SWITCH(dest)][dest]);
            routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);
      
            paths->push_back(routeout);
            check_non_null(routeout);
        }
        return paths;
    }
    else {
        uint32_t pod = HOST_POD(src);

        for (uint32_t upper = MIN_POD_ID(pod);upper <= MAX_POD_ID(pod); upper++)
            for (uint32_t core = (upper%(K/2)) * K / 2; core < ((upper % (K/2)) + 1)*K/2; core++){
                //upper is nup
        
                routeout = new Route();
        
                routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);
                routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);
        
                routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][upper]);
                routeout->push_back(pipes_nlp_nup[HOST_POD_SWITCH(src)][upper]);
        
                routeout->push_back(queues_nup_nc[upper][core]);
                routeout->push_back(pipes_nup_nc[upper][core]);
        
                //now take the only link down to the destination server!
        
                uint32_t upper2 = HOST_POD(dest)*K/2 + 2 * core / K;
                //printf("K %d HOST_POD(%d) %d core %d upper2 %d\n",K,dest,HOST_POD(dest),core, upper2);
        
                routeout->push_back(queues_nc_nup[core][upper2]);
                routeout->push_back(pipes_nc_nup[core][upper2]);
        
                routeout->push_back(queues_nup_nlp[upper2][HOST_POD_SWITCH(dest)]);
                routeout->push_back(pipes_nup_nlp[upper2][HOST_POD_SWITCH(dest)]);
        
                routeout->push_back(queues_nlp_ns[HOST_POD_SWITCH(dest)][dest]);
                routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);
        
                paths->push_back(routeout);
                check_non_null(routeout);
            }
        return paths;
    }
}

void OversubscribedFatTreeTopology::count_queue(Queue* queue){
    if (_link_usage.find(queue)==_link_usage.end()){
        _link_usage[queue] = 0;
    }

    _link_usage[queue] = _link_usage[queue] + 1;
}

int64_t OversubscribedFatTreeTopology::find_lp_switch(Queue* queue){
    //first check ns_nlp
    for (uint32_t i=0;i<NSRV;i++)
        for (uint32_t j = 0;j<NK;j++)
            if (queues_ns_nlp[i][j]==queue)
                return j;

    //only count nup to nlp
    count_queue(queue);

    for (uint32_t i=0;i<NK;i++)
        for (uint32_t j = 0;j<NK;j++)
            if (queues_nup_nlp[i][j]==queue)
                return j;

    return -1;
}

int64_t OversubscribedFatTreeTopology::find_up_switch(Queue* queue){
    count_queue(queue);
    //first check nc_nup
    for (uint32_t i=0;i<NC;i++)
        for (uint32_t j = 0;j<NK;j++)
            if (queues_nc_nup[i][j]==queue)
                return j;

    //check nlp_nup
    for (uint32_t i=0;i<NK;i++)
        for (uint32_t j = 0;j<NK;j++)
            if (queues_nlp_nup[i][j]==queue)
                return j;

    return -1;
}

int64_t OversubscribedFatTreeTopology::find_core_switch(Queue* queue){
    count_queue(queue);
    //first check nup_nc
    for (uint32_t i=0;i<NK;i++)
        for (uint32_t j = 0;j<NC;j++)
            if (queues_nup_nc[i][j]==queue)
                return j;

    return -1;
}

int64_t OversubscribedFatTreeTopology::find_destination(Queue* queue){
    //first check nlp_ns
    for (uint32_t i=0;i<NK;i++)
        for (uint32_t j = 0;j<NSRV;j++)
            if (queues_nlp_ns[i][j]==queue)
                return j;

    return -1;
}

void OversubscribedFatTreeTopology::print_path(std::ofstream &paths,uint32_t src,const Route* route){
    paths << "SRC_" << src << " ";
  
    if (route->size()/2==2){
        paths << "LS_" << find_lp_switch((RandomQueue*)route->at(1)) << " ";
        paths << "DST_" << find_destination((RandomQueue*)route->at(3)) << " ";
    } else if (route->size()/2==4){
        paths << "LS_" << find_lp_switch((RandomQueue*)route->at(1)) << " ";
        paths << "US_" << find_up_switch((RandomQueue*)route->at(3)) << " ";
        paths << "LS_" << find_lp_switch((RandomQueue*)route->at(5)) << " ";
        paths << "DST_" << find_destination((RandomQueue*)route->at(7)) << " ";
    } else if (route->size()/2==6){
        paths << "LS_" << find_lp_switch((RandomQueue*)route->at(1)) << " ";
        paths << "US_" << find_up_switch((RandomQueue*)route->at(3)) << " ";
        paths << "CS_" << find_core_switch((RandomQueue*)route->at(5)) << " ";
        paths << "US_" << find_up_switch((RandomQueue*)route->at(7)) << " ";
        paths << "LS_" << find_lp_switch((RandomQueue*)route->at(9)) << " ";
        paths << "DST_" << find_destination((RandomQueue*)route->at(11)) << " ";
    } else {
        paths << "Wrong hop count " << ntoa(route->size()/2);
    }
  
    paths << endl;
}

vector<uint32_t>* OversubscribedFatTreeTopology::get_neighbours(uint32_t src){
    vector<uint32_t>* neighbours = new vector<uint32_t>();
    uint32_t sw = HOST_POD_SWITCH(src) * 2 * K;
    for (uint32_t i=0; i < 2*K; i++){
        uint32_t dst = i + sw;
        if (dst!=src)
            neighbours->push_back(dst);
    }
  
    return neighbours;
}

