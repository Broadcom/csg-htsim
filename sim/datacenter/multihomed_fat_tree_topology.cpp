// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "multihomed_fat_tree_topology.h"
#include <vector>
#include "string.h"
#include <sstream>

#include <iostream>
#include "main.h"

string ntoa(double n);
string itoa(uint64_t n);

MultihomedFatTreeTopology::MultihomedFatTreeTopology(Logfile* lg, EventList* ev,FirstFit * fit,simtime_picosec rtt){
    logfile = lg;
    eventlist = ev;
    ff = fit;
    _rtt = rtt;
  
    _no_of_nodes = K * K * K/3;

    init_network();
}

void MultihomedFatTreeTopology::init_network(){
    QueueLoggerSampling* queueLogger;

    for (uint32_t j=0;j<NC;j++)
        for (uint32_t k=0;k<NK;k++){
            queues_nc_nup[j][k] = NULL;
            pipes_nc_nup[j][k] = NULL;
            queues_nup_nc[k][j] = NULL;
            pipes_nup_nc[k][j] = NULL;
        }

    for (uint32_t j=0;j<NK;j++)
        for (uint32_t k=0;k<NLP;k++){
            queues_nup_nlp[j][k] = NULL;
            pipes_nup_nlp[j][k] = NULL;
            queues_nlp_nup[k][j] = NULL;
            pipes_nlp_nup[k][j] = NULL;
        }
  
    for (uint32_t j=0;j<NLP;j++)
        for (uint32_t k=0;k<NSRV;k++){
            queues_nlp_ns[j][k] = NULL;
            pipes_nlp_ns[j][k] = NULL;
            queues_ns_nlp[k][j] = NULL;
            pipes_ns_nlp[k][j] = NULL;
        }

    // lower layer pod switch to server
    for (uint32_t j = 0; j < NLP; j++) {
        for (uint32_t l = 0; l < 2*K/3; l++) {
            uint32_t k = (j/2)*2*K/3 + l;
            // Downlink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            //queueLogger = NULL;
            logfile->addLogger(*queueLogger);

            queues_nlp_ns[j][k] = new RandomQueue(speedFromPktps(HOST_NIC), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
            queues_nlp_ns[j][k]->setName("LS_" + ntoa(j) + "-" + "DST_" +ntoa(k));
            logfile->writeName(*(queues_nlp_ns[j][k]));
          
            pipes_nlp_ns[j][k] = new Pipe(_rtt, *eventlist);
            pipes_nlp_ns[j][k]->setName("Pipe-nt-ns-" + ntoa(j) + "-" + ntoa(k));
            logfile->writeName(*(pipes_nlp_ns[j][k]));
          
            // Uplink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);
            queues_ns_nlp[k][j] = new RandomQueue(speedFromPktps(HOST_NIC), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
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

    //Lower layer in pod to upper layer in pod!
    for (uint32_t j = 0; j < NLP; j++) {
        uint32_t podid = j/K;
        //Connect the lower layer switch to the upper layer switches in the same pod
        uint32_t start = MIN_POD_ID(podid);
        uint32_t end = MAX_POD_ID(podid);

        if (j%2==0) end = (start+end)/2;
        else start = (start+end)/2+1;

        //      cout << "Init " << end-start+1 << " entries "<< endl;
        for (uint32_t k=start; k<=end;k++){
            // Downlink

            //cout << "NLP " << j << " NUP " << k << endl;
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);
            queues_nup_nlp[k][j] = new RandomQueue(speedFromPktps(HOST_NIC), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
            queues_nup_nlp[k][j]->setName("US_" + ntoa(k) + "-" + "LS_"+ntoa(j));
            logfile->writeName(*(queues_nup_nlp[k][j]));
        
            pipes_nup_nlp[k][j] = new Pipe(_rtt, *eventlist);
            pipes_nup_nlp[k][j]->setName("Pipe-na-nt-" + ntoa(k) + "-" + ntoa(j));
            logfile->writeName(*(pipes_nup_nlp[k][j]));
        
            // Uplink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);
            queues_nlp_nup[j][k] = new RandomQueue(speedFromPktps(HOST_NIC), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
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

            queues_nup_nc[j][k] = new RandomQueue(speedFromPktps(HOST_NIC), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
            queues_nup_nc[j][k]->setName("US_" + ntoa(j) + "-" + "CS_"+ ntoa(k));
            logfile->writeName(*(queues_nup_nc[j][k]));
        
            pipes_nup_nc[j][k] = new Pipe(_rtt, *eventlist);
            pipes_nup_nc[j][k]->setName("Pipe-nup-nc-" + ntoa(j) + "-" + ntoa(k));
            logfile->writeName(*(pipes_nup_nc[j][k]));
        
            // Uplink
        
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);
        
            //        if (k==0&&j==0)
            //queues_nc_nup[k][j] = new RandomQueue(speedFromPktps(HOST_NIC/10), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
            //else
            queues_nc_nup[k][j] = new RandomQueue(speedFromPktps(HOST_NIC), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
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

vector<const Route*>* MultihomedFatTreeTopology::get_paths(uint32_t src, uint32_t dest){
    vector<const Route*>* paths = new vector<const Route*>();

    Route* routeout;

    if (HOST_POD_SWITCH1(src)==HOST_POD_SWITCH1(dest)){
        Queue* pqueue = new Queue(speedFromPktps(2*HOST_NIC), memFromPkt(FEEDER_BUFFER), *eventlist, NULL);
        pqueue->setName("PQueue_" + ntoa(src) + "_" + ntoa(dest));
        //logfile->writeName(*pqueue);
  
        routeout = new Route();
        routeout->push_back(pqueue);

        routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH1(src)]);
        routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH1(src)]);

        assert(HOST_POD_SWITCH1(src)==HOST_POD_SWITCH1(dest));
        assert(HOST_POD_SWITCH2(src)==HOST_POD_SWITCH2(dest));

        routeout->push_back(queues_nlp_ns[HOST_POD_SWITCH1(dest)][dest]);
        routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH1(dest)][dest]);

        paths->push_back(routeout);
        check_non_null(routeout);

        routeout = new Route();
        pqueue = new Queue(speedFromPktps(2*HOST_NIC), memFromPkt(FEEDER_BUFFER), *eventlist, NULL);
        pqueue->setName("PQueue_" + ntoa(src) + "_2_" + ntoa(dest));
        routeout->push_back(pqueue);

        routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH2(src)]);
        routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH2(src)]);

        routeout->push_back(queues_nlp_ns[HOST_POD_SWITCH2(dest)][dest]);
        routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH2(dest)][dest]);

        paths->push_back(routeout);
        check_non_null(routeout);

        return paths;
    }
    else if (HOST_POD(src)==HOST_POD(dest)){
        //don't go up the hierarchy, stay in the pod only.

        uint32_t pod = HOST_POD(src);
        //there are K/2 paths between the source and the destination
        for (uint32_t upper = MIN_POD_ID(pod);upper <= MAX_POD_ID(pod); upper++){
            //upper is nup
            Queue* pqueue = new Queue(speedFromPktps(HOST_NIC), memFromPkt(FEEDER_BUFFER), *eventlist, NULL);
            pqueue->setName("PQueue_" + ntoa(src) + "_" + ntoa(dest));
            //logfile->writeName(*pqueue);
      
            routeout = new Route();
            routeout->push_back(pqueue);

            uint32_t sws,swd;
            if (upper <= (MIN_POD_ID(pod)+MAX_POD_ID(pod))/2){
                sws = HOST_POD_SWITCH1(src);
                swd = HOST_POD_SWITCH1(dest);
            }
            else{
                sws = HOST_POD_SWITCH2(src);
                swd = HOST_POD_SWITCH2(dest);
            }
      
            routeout->push_back(queues_ns_nlp[src][sws]);
            routeout->push_back(pipes_ns_nlp[src][sws]);

            routeout->push_back(queues_nlp_nup[sws][upper]);
            routeout->push_back(pipes_nlp_nup[sws][upper]);

            routeout->push_back(queues_nup_nlp[upper][swd]);
            routeout->push_back(pipes_nup_nlp[upper][swd]);
      
            routeout->push_back(queues_nlp_ns[swd][dest]);
            routeout->push_back(pipes_nlp_ns[swd][dest]);

            paths->push_back(routeout);
            check_non_null(routeout);
        }
        return paths;
    }
    else {
        uint32_t pod = HOST_POD(src);
        uint32_t pod_dest = HOST_POD(dest);
        uint32_t sws,swd;

        for (uint32_t upper = MIN_POD_ID(pod);upper <= MAX_POD_ID(pod); upper++)
            for (uint32_t core = (upper%(K/2)) * K / 2; core < ((upper % (K/2)) + 1)*K/2; core++){
                //upper is nup
                Queue* pqueue = new Queue(speedFromPktps(HOST_NIC), memFromPkt(FEEDER_BUFFER), *eventlist, NULL);
                pqueue->setName("PQueue_" + ntoa(src) + "_" + ntoa(dest));
                //logfile->writeName(*pqueue);
        
                routeout = new Route();
                routeout->push_back(pqueue);

                if (upper <= (MIN_POD_ID(pod)+MAX_POD_ID(pod))/2){
                    sws = HOST_POD_SWITCH1(src);
                }
                else{
                    sws = HOST_POD_SWITCH2(src);
                }
        
                routeout->push_back(queues_ns_nlp[src][sws]);
                routeout->push_back(pipes_ns_nlp[src][sws]);
        
                routeout->push_back(queues_nlp_nup[sws][upper]);
                routeout->push_back(pipes_nlp_nup[sws][upper]);
        
                routeout->push_back(queues_nup_nc[upper][core]);
                routeout->push_back(pipes_nup_nc[upper][core]);
        
                //now take the only link down to the destination server!
        
                uint32_t upper2 = HOST_POD(dest) * K/2 + 2 * core / K;

                if (upper2 <= (MIN_POD_ID(pod_dest)+MAX_POD_ID(pod_dest))/2){
                    //          cout << "Upper 2S "<<upper2 << " MIN_POD " << MIN_POD_ID(pod_dest)<< " MAX_POD " << MAX_POD_ID(pod_dest)<<endl;
                    swd = HOST_POD_SWITCH1(dest);
                }
                else{
                    //cout << "Upper 2B "<<upper2 << " MIN_POD " << MIN_POD_ID(pod_dest)<< " MAX_POD " << MAX_POD_ID(pod_dest)<<endl;
                    swd = HOST_POD_SWITCH2(dest);
                }
                //cout << "SRC " << src  << " DEST "<<dest << " Dest Switch " << swd << endl;


                //printf("K %d HOST_POD(%d) %d core %d upper2 %d\n",K,dest,HOST_POD(dest),core, upper2);
        
                routeout->push_back(queues_nc_nup[core][upper2]);
                routeout->push_back(pipes_nc_nup[core][upper2]);
        
                routeout->push_back(queues_nup_nlp[upper2][swd]);
                routeout->push_back(pipes_nup_nlp[upper2][swd]);
        
                routeout->push_back(queues_nlp_ns[swd][dest]);
                routeout->push_back(pipes_nlp_ns[swd][dest]);
        
                paths->push_back(routeout);
                check_non_null(routeout);
            }
        return paths;
    }
}

void MultihomedFatTreeTopology::count_queue(RandomQueue* queue){
    if (_link_usage.find(queue)==_link_usage.end()){
        _link_usage[queue] = 0;
    }

    _link_usage[queue] = _link_usage[queue] + 1;
}

int64_t MultihomedFatTreeTopology::find_lp_switch(RandomQueue* queue){
    //first check ns_nlp
    for (uint32_t i=0;i<NSRV;i++)
        for (uint32_t j = 0;j<NLP;j++)
            if (queues_ns_nlp[i][j]==queue)
                return j;

    //only count nup to nlp
    count_queue(queue);

    for (uint32_t i=0;i<NK;i++)
        for (uint32_t j = 0;j<NLP;j++)
            if (queues_nup_nlp[i][j]==queue)
                return j;

    return -1;
}

int64_t MultihomedFatTreeTopology::find_up_switch(RandomQueue* queue){
    count_queue(queue);
    //first check nc_nup
    for (uint32_t i=0;i<NC;i++)
        for (uint32_t j = 0;j<NK;j++)
            if (queues_nc_nup[i][j]==queue)
                return j;

    //check nlp_nup
    for (uint32_t i=0;i<NLP;i++)
        for (uint32_t j = 0;j<NK;j++)
            if (queues_nlp_nup[i][j]==queue)
                return j;

    return -1;
}

int64_t MultihomedFatTreeTopology::find_core_switch(RandomQueue* queue){
    count_queue(queue);
    //first check nup_nc
    for (uint32_t i=0;i<NK;i++)
        for (uint32_t j = 0;j<NC;j++)
            if (queues_nup_nc[i][j]==queue)
                return j;

    return -1;
}

int64_t MultihomedFatTreeTopology::find_destination(RandomQueue* queue){
    //first check nlp_ns
    for (uint32_t i=0;i<NLP;i++)
        for (uint32_t j = 0;j<NSRV;j++)
            if (queues_nlp_ns[i][j]==queue)
                return j;

    return -1;
}

void MultihomedFatTreeTopology::print_path(std::ofstream &paths,uint32_t src,const Route* route){
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

vector<uint32_t>* MultihomedFatTreeTopology::get_neighbours(uint32_t src){
    vector<uint32_t>* neighbours = new vector<uint32_t>();
    uint32_t sw = HOST_POD_SWITCH1(src)*K/3;
    for (uint32_t i=0;i<2*K/3;i++){
        uint32_t dst = i+sw;
        if (dst!=src){
            neighbours->push_back(dst);
        }
    }
    /*  cout << "Neighbours of "<<src << " are ";
        for (uint32_t i=0;i<neighbours->size;i++)
        cout << " " << neighbours->at(i);
        cout << endl;*/
  
    return neighbours;
}
