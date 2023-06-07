// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "dragon_fly_topology.h"
#include <vector>
#include "string.h"
#include <sstream>
#include <iostream>
#include "main.h"
#include "queue.h"
#include "switch.h"
#include "compositequeue.h"
#include "prioqueue.h"
#include "queue_lossless.h"
#include "queue_lossless_input.h"
#include "queue_lossless_output.h"
#include "ecnqueue.h"
#include "main.h"

string ntoa(double n);
string itoa(uint64_t n);

DragonFlyTopology::DragonFlyTopology(uint32_t p, uint32_t a, uint32_t h, mem_b queuesize, Logfile* lg,EventList* ev,queue_type q,simtime_picosec rtt){
    _queuesize = queuesize;
    logfile = lg;
    _eventlist = ev;
    qt = q;
    _rtt = rtt;
 
    _p = p;
    _a = a;
    _h = h;

    _no_of_nodes = _a*_p*(_a*_h+1);

    cout << "DragonFly topology with " << _p << " hosts per router, " << _a << " routers per group and " << (_a * _h +1) << " groups, total nodes " << _no_of_nodes << endl;
    cout << "Queue type " << qt << endl;

    set_params();
    init_network();
}

DragonFlyTopology::DragonFlyTopology(uint32_t no_of_nodes, mem_b queuesize, Logfile* lg,EventList* ev,queue_type q,simtime_picosec rtt){
    _queuesize = queuesize;
    logfile = lg;
    _eventlist = ev;
    qt = q;
    _rtt = rtt;
  
    set_params(no_of_nodes);

    init_network();
}

void DragonFlyTopology::set_params(uint32_t no_of_nodes) {
    cout << "Set params " << no_of_nodes << endl;
    cout << "QueueSize " << _queuesize << endl;
    _no_of_nodes = 0;
    _h = 0;

    while (_no_of_nodes < no_of_nodes) {
        _h++;
        _p = _h;
        _a = 2 * _h;
        _no_of_nodes =  _a*_p*(_a*_h+1);
    }

    if (_no_of_nodes > no_of_nodes) {
        cerr << "Topology Error: can't have a DragonFly with " << no_of_nodes
             << " nodes\n";
        exit(1);
    }

    //now that we know the parameters, setup the topology.
    set_params();
}
    

void DragonFlyTopology::set_params() {
    _no_of_groups = _a*_h+1;
    _no_of_switches = _no_of_groups * _a;

    cout << "DragonFly topology with " << _p << " hosts per router, " << _a << " routers per group and " << (_a * _h +1) << " groups, total nodes " << _no_of_nodes << endl;
    cout << "Queue type " << qt << endl;

    switches.resize(_no_of_switches,NULL);

    pipes_host_switch.resize(_no_of_nodes, vector<Pipe*>(_no_of_switches));
    queues_host_switch.resize(_no_of_nodes, vector<Queue*>(_no_of_switches));

    pipes_switch_host.resize(_no_of_switches, vector<Pipe*>(_no_of_nodes));
    queues_switch_host.resize(_no_of_switches, vector<Queue*>(_no_of_nodes));

    pipes_switch_switch.resize(_no_of_switches, vector<Pipe*>(_no_of_switches));
    queues_switch_switch.resize(_no_of_switches, vector<Queue*>(_no_of_switches));

}

Queue* DragonFlyTopology::alloc_src_queue(QueueLogger* queueLogger){
    return new FairPriorityQueue(speedFromMbps((uint64_t)HOST_NIC), memFromPkt(FEEDER_BUFFER), *_eventlist, queueLogger);
    //return new PriorityQueue(speedFromMbps((uint64_t)HOST_NIC), memFromPkt(FEEDER_BUFFER), *_eventlist, queueLogger);
    
}

Queue* DragonFlyTopology::alloc_queue(QueueLogger* queueLogger, mem_b queuesize, bool tor = false){
    return alloc_queue(queueLogger, HOST_NIC, queuesize, tor);
}

Queue* DragonFlyTopology::alloc_queue(QueueLogger* queueLogger, uint64_t speed, mem_b queuesize, bool tor){
    if (qt==RANDOM)
        return new RandomQueue(speedFromMbps(speed), queuesize, *_eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
    else if (qt==COMPOSITE)
        return new CompositeQueue(speedFromMbps(speed), queuesize, *_eventlist, queueLogger);
    else if (qt==CTRL_PRIO)
        return new CtrlPrioQueue(speedFromMbps(speed), queuesize, *_eventlist, queueLogger);
    else if (qt==ECN)
        return new ECNQueue(speedFromMbps(speed), memFromPkt(queuesize), *_eventlist, queueLogger, memFromPkt(15));
    else if (qt==LOSSLESS)
        return new LosslessQueue(speedFromMbps(speed), memFromPkt(50), *_eventlist, queueLogger, NULL);
    else if (qt==LOSSLESS_INPUT)
        return new LosslessOutputQueue(speedFromMbps(speed), memFromPkt(200), *_eventlist, queueLogger);    
    else if (qt==LOSSLESS_INPUT_ECN)
        return new LosslessOutputQueue(speedFromMbps(speed), memFromPkt(10000), *_eventlist, queueLogger,1,memFromPkt(16));
    else if (qt==COMPOSITE_ECN){
        if (tor) 
            return new CompositeQueue(speedFromMbps(speed), queuesize, *_eventlist, queueLogger);
        else
            return new ECNQueue(speedFromMbps(speed), memFromPkt(2*SWITCH_BUFFER), *_eventlist, queueLogger, memFromPkt(15));
    }
    assert(0);
}

void DragonFlyTopology::init_network(){
    QueueLoggerSampling* queueLogger;

    for (uint32_t j=0;j<_no_of_switches;j++){
        for (uint32_t k=0;k<_no_of_switches;k++){
            queues_switch_switch[j][k] = NULL;
            pipes_switch_switch[j][k] = NULL;
        }
      
        for (uint32_t k=0;k<_no_of_nodes;k++){
            queues_switch_host[j][k] = NULL;
            pipes_switch_host[j][k] = NULL;
            queues_host_switch[k][j] = NULL;
            pipes_host_switch[k][j] = NULL;
        }
    }
  
    //create switches if we have lossless operation
    if (qt==LOSSLESS)
        for (uint32_t j=0;j<_no_of_switches;j++){
            switches[j] = new Switch(*_eventlist, "Switch_"+ntoa(j));
        }
      
    // links from switches to server
    for (uint32_t j = 0; j < _no_of_switches; j++) {
        for (uint32_t l = 0; l < _p; l++) {
            uint32_t k = j * _p + l;
            // Downlink
            queueLogger = new QueueLoggerSampling(timeFromUs((uint32_t)10), *_eventlist);
            //queueLogger = NULL;
            logfile->addLogger(*queueLogger);
          
            queues_switch_host[j][k] = alloc_queue(queueLogger, _queuesize,true);
            queues_switch_host[j][k]->setName("SW" + ntoa(j) + "->DST" +ntoa(k));
            logfile->writeName(*(queues_switch_host[j][k]));
          
            pipes_switch_host[j][k] = new Pipe(_rtt, *_eventlist);
            pipes_switch_host[j][k]->setName("Pipe-SW" + ntoa(j)  + "->DST" + ntoa(k));
            logfile->writeName(*(pipes_switch_host[j][k]));
          
            // Uplink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *_eventlist);
            logfile->addLogger(*queueLogger);
            queues_host_switch[k][j] = alloc_src_queue(queueLogger);
            queues_host_switch[k][j]->setName("SRC" + ntoa(k) + "->SW" +ntoa(j));
            logfile->writeName(*(queues_host_switch[k][j]));

            if (qt==LOSSLESS){
                switches[j]->addPort(queues_switch_host[j][k]);
                ((LosslessQueue*)queues_switch_host[j][k])->setRemoteEndpoint(queues_host_switch[k][j]);
            }else if (qt==LOSSLESS_INPUT || qt == LOSSLESS_INPUT_ECN){
                //no virtual queue needed at server
                new LosslessInputQueue(*_eventlist,queues_host_switch[k][j]);
            }
          
            pipes_host_switch[k][j] = new Pipe(_rtt, *_eventlist);
            pipes_host_switch[k][j]->setName("Pipe-SRC" + ntoa(k) + "->SW" + ntoa(j));
            logfile->writeName(*(pipes_host_switch[k][j]));
        }
    }


    //Switch to switch links
    for (uint32_t j = 0; j < _no_of_switches; j++) {
        uint32_t groupid = j/_a;

        //Connect the switch to other switches in the same group, with higher IDs (full mesh within group).
        for (uint32_t k=j+1; k<(groupid+1)*_a;k++){
            //Downlink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *_eventlist);
            logfile->addLogger(*queueLogger);
            queues_switch_switch[k][j] = alloc_queue(queueLogger, _queuesize);
            queues_switch_switch[k][j]->setName("SW" + ntoa(k) + "-I->SW" + ntoa(j));
            logfile->writeName(*(queues_switch_switch[k][j]));
        
            pipes_switch_switch[k][j] = new Pipe(_rtt, *_eventlist);
            pipes_switch_switch[k][j]->setName("Pipe-SW" + ntoa(k) + "-I->SW" + ntoa(j));
            logfile->writeName(*(pipes_switch_switch[k][j]));
        
            // Uplink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *_eventlist);
            logfile->addLogger(*queueLogger);
            queues_switch_switch[j][k] = alloc_queue(queueLogger, _queuesize,true);
            queues_switch_switch[j][k]->setName("SW" + ntoa(j) + "-I->SW" + ntoa(k));
            logfile->writeName(*(queues_switch_switch[j][k]));

            if (qt==LOSSLESS){
                switches[j]->addPort(queues_switch_switch[j][k]);
                ((LosslessQueue*)queues_switch_switch[j][k])->setRemoteEndpoint(queues_switch_switch[k][j]);
                switches[k]->addPort(queues_switch_switch[k][j]);
                ((LosslessQueue*)queues_switch_switch[k][j])->setRemoteEndpoint(queues_switch_switch[j][k]);
            }else if (qt==LOSSLESS_INPUT || qt == LOSSLESS_INPUT_ECN){            
                new LosslessInputQueue(*_eventlist, queues_switch_switch[j][k]);
                new LosslessInputQueue(*_eventlist, queues_switch_switch[k][j]);
            }
        
            pipes_switch_switch[j][k] = new Pipe(_rtt, *_eventlist);
            pipes_switch_switch[j][k]->setName("Pipe-SW" + ntoa(j) + "-I->SW" + ntoa(k));
            logfile->writeName(*(pipes_switch_switch[j][k]));
        }

        //Connect the switch to switches from other groups. Global links.
        for (uint32_t l = 0; l < _h; l++){
            uint32_t targetgroupid = (j%_a)*_h + l;
            uint32_t larger = targetgroupid>=groupid;

            //Compute target switch ID; only create links to groups with ID larger than ours.             
            //if the ID is larger than ours, effective target group is +1 (we skip our own group for global links). 
            if (larger)
                targetgroupid++;
            else
                continue;
            
            uint32_t k  = targetgroupid * _a + groupid/_h;

            //Downlink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *_eventlist);
            logfile->addLogger(*queueLogger);
            queues_switch_switch[k][j] = alloc_queue(queueLogger, _queuesize);
            queues_switch_switch[k][j]->setName("SW" + ntoa(k) + "-G->SW" + ntoa(j));
            logfile->writeName(*(queues_switch_switch[k][j]));
        
            pipes_switch_switch[k][j] = new Pipe(_rtt, *_eventlist);
            pipes_switch_switch[k][j]->setName("Pipe-SW" + ntoa(k) + "-G->SW" + ntoa(j));
            logfile->writeName(*(pipes_switch_switch[k][j]));
        
            // Uplink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *_eventlist);
            logfile->addLogger(*queueLogger);
            queues_switch_switch[j][k] = alloc_queue(queueLogger, _queuesize,true);
            queues_switch_switch[j][k]->setName("SW" + ntoa(j) + "-G->SW" + ntoa(k));
            logfile->writeName(*(queues_switch_switch[j][k]));

            if (qt==LOSSLESS){
                switches[j]->addPort(queues_switch_switch[j][k]);
                ((LosslessQueue*)queues_switch_switch[j][k])->setRemoteEndpoint(queues_switch_switch[k][j]);
                switches[k]->addPort(queues_switch_switch[k][j]);
                ((LosslessQueue*)queues_switch_switch[k][j])->setRemoteEndpoint(queues_switch_switch[j][k]);
            }else if (qt==LOSSLESS_INPUT || qt == LOSSLESS_INPUT_ECN){            
                new LosslessInputQueue(*_eventlist, queues_switch_switch[j][k]);
                new LosslessInputQueue(*_eventlist, queues_switch_switch[k][j]);
            }
        
            pipes_switch_switch[j][k] = new Pipe(_rtt, *_eventlist);
            pipes_switch_switch[j][k]->setName("Pipe-SW" + ntoa(j) + "-G->SW" + ntoa(k));
            logfile->writeName(*(pipes_switch_switch[j][k]));
        }        
    }

    //init thresholds for lossless operation
    if (qt==LOSSLESS)
        for (uint32_t j=0;j<_no_of_switches;j++){
            switches[j]->configureLossless();
        }
}

vector<const Route*>* DragonFlyTopology::get_bidir_paths(uint32_t src, uint32_t dest, bool reverse){
    vector<const Route*>* paths = new vector<const Route*>();

    route_t *routeout, *routeback;
  
    if (HOST_TOR(src)==HOST_TOR(dest)){
        // forward path
        routeout = new Route();
        routeout->push_back(queues_host_switch[src][HOST_TOR(src)]);
        routeout->push_back(pipes_host_switch[src][HOST_TOR(src)]);

        if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
            routeout->push_back(queues_host_switch[src][HOST_TOR(src)]->getRemoteEndpoint());

        routeout->push_back(queues_switch_host[HOST_TOR(dest)][dest]);
        routeout->push_back(pipes_switch_host[HOST_TOR(dest)][dest]);

        // reverse path for RTS packets
        routeback = new Route();
        routeback->push_back(queues_host_switch[dest][HOST_TOR(dest)]);
        routeback->push_back(pipes_host_switch[dest][HOST_TOR(dest)]);

        if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
            routeback->push_back(queues_host_switch[dest][HOST_TOR(dest)]->getRemoteEndpoint());

        routeback->push_back(queues_switch_host[HOST_TOR(src)][src]);
        routeback->push_back(pipes_switch_host[HOST_TOR(src)][src]);

        routeout->set_reverse(routeback);
        routeback->set_reverse(routeout);

        //print_route(*routeout);
        paths->push_back(routeout);

        check_non_null(routeout);
        return paths;
    }
    else if (HOST_GROUP(src)==HOST_GROUP(dest)){
        //don't go up the hierarchy, stay in the group only.

        //there is 1 direct path between the source and the destination.
        //TBD also do indirect paths?

        routeout = new Route();
    
        routeout->push_back(queues_host_switch[src][HOST_TOR(src)]);
        routeout->push_back(pipes_host_switch[src][HOST_TOR(src)]);
    
        if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
            routeout->push_back(queues_host_switch[src][HOST_TOR(src)]->getRemoteEndpoint());
    
        routeout->push_back(queues_switch_switch[HOST_TOR(src)][HOST_TOR(dest)]);
        routeout->push_back(pipes_switch_switch[HOST_TOR(src)][HOST_TOR(dest)]);
    
        if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
            routeout->push_back(queues_switch_switch[HOST_TOR(src)][HOST_TOR(dest)]->getRemoteEndpoint());
    
        routeout->push_back(queues_switch_host[HOST_TOR(dest)][dest]);
        routeout->push_back(pipes_switch_host[HOST_TOR(dest)][dest]);
    
        // reverse path for RTS packets
        routeback = new Route();
    
        routeback->push_back(queues_host_switch[dest][HOST_TOR(dest)]);
        routeback->push_back(pipes_host_switch[dest][HOST_TOR(dest)]);
    
        if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
            routeback->push_back(queues_host_switch[dest][HOST_TOR(dest)]->getRemoteEndpoint());
    
        routeback->push_back(queues_switch_switch[HOST_TOR(dest)][HOST_TOR(src)]);
        routeback->push_back(pipes_switch_switch[HOST_TOR(dest)][HOST_TOR(src)]);
    
        if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
            routeback->push_back(queues_switch_switch[HOST_TOR(dest)][HOST_TOR(src)]->getRemoteEndpoint());
    
        routeback->push_back(queues_switch_host[HOST_TOR(src)][src]);
        routeback->push_back(pipes_switch_host[HOST_TOR(src)][src]);
    
        routeout->set_reverse(routeback);
        routeback->set_reverse(routeout);
    
        //print_route(*routeout);
        paths->push_back(routeout);
        check_non_null(routeout);

        return paths;
    }
    else {
        uint32_t srcgroup = HOST_GROUP(src);
        uint32_t dstgroup = HOST_GROUP(dest);

        //add lowest cost path first. add others if needed later. 
        routeout = new Route();

        assert(queues_host_switch[src][HOST_TOR(src)]);
    
        routeout->push_back(queues_host_switch[src][HOST_TOR(src)]);
        routeout->push_back(pipes_host_switch[src][HOST_TOR(src)]);

        //cout << "SRC " << src << " SW " << HOST_TOR(src) << " ";
    
        if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
            routeout->push_back(queues_host_switch[src][HOST_TOR(src)]->getRemoteEndpoint());

        uint32_t srcswitch,dstswitch;
        //find srcswitch from srcgroup which has a path to dstgroup and  dstswitch from dstgroup which has an incoming path from srcgroup.

        if (srcgroup<dstgroup){
            srcswitch = srcgroup * _a + (dstgroup-1)/_h;
            dstswitch =  dstgroup * _a + srcgroup/_h;
        }
        else {
            srcswitch = srcgroup * _a + dstgroup/_h;
            dstswitch =  dstgroup * _a + (srcgroup-1)/_h;
        }

        if (HOST_TOR(src)!=srcswitch){
            /*When SRC HOST TOR does not have a direct path to destination group, take local path to the appropriate switch*/

            assert(queues_switch_switch[HOST_TOR(src)][srcswitch]);
            routeout->push_back(queues_switch_switch[HOST_TOR(src)][srcswitch]);
            routeout->push_back(pipes_switch_switch[HOST_TOR(src)][srcswitch]);

            //cout << "SW " << srcswitch <<        " " ;
        
            if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
                routeout->push_back(queues_switch_switch[HOST_TOR(src)][srcswitch]->getRemoteEndpoint());
        }

        /* path from source group to destination group*/
        assert(queues_switch_switch[srcswitch][dstswitch]);
        routeout->push_back(queues_switch_switch[srcswitch][dstswitch]);
        routeout->push_back(pipes_switch_switch[srcswitch][dstswitch]);

        //cout << "SW " << dstswitch <<        " ";    

        if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
            routeout->push_back(queues_host_switch[srcswitch][dstswitch]->getRemoteEndpoint());

        if (dstswitch!=HOST_TOR(dest)){
            /*When dstswitch does not have a direct path to dest, take local path to the appropriate TOR switch*/
            //cout << "SW " << HOST_TOR(dest) <<        " ";
            assert(queues_switch_switch[dstswitch][HOST_TOR(dest)]);

            routeout->push_back(queues_switch_switch[dstswitch][HOST_TOR(dest)]);
            routeout->push_back(pipes_switch_switch[dstswitch][HOST_TOR(dest)]);

            if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
                routeout->push_back(queues_switch_switch[dstswitch][HOST_TOR(dest)]->getRemoteEndpoint());
        }
        //cout << "DEST " << dest <<        " " << endl;
        assert(queues_switch_host[HOST_TOR(dest)][dest]);

        routeout->push_back(queues_switch_host[HOST_TOR(dest)][dest]);
        routeout->push_back(pipes_switch_host[HOST_TOR(dest)][dest]);

        // reverse path for RTS packets                                                                                        /*
        //routeback = new Route();

        /*routeback->push_back(queues_host_switch[dest][HOST_TOR(dest)]);
          routeback->push_back(pipes_host_switch[dest][HOST_TOR(dest)]);

          if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
          routeback->push_back(queues_host_switch[dest][HOST_TOR(dest)]->getRemoteEndpoint());


          if (dstswitch!=HOST_TOR(dest)){
          routeback->push_back(queues_switch_host[HOST_TOR(dest)][dstswitch]);
          routeback->push_back(pipes_switch_host[HOST_TOR(dest)][dstswitch]);

          if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
          routeback->push_back(queues_host_switch[HOST_TOR(dest)][dstswitch]->getRemoteEndpoint());
          }
    
          routeback->push_back(queues_switch_host[dstswitch][srcswitch]);
          routeback->push_back(pipes_switch_host[dstswitch][srcswitch]);

          if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
          routeback->push_back(queues_host_switch[dstswitch][srcswitch]->getRemoteEndpoint());

          if (HOST_TOR(src)!=srcswitch){
          //When SRC HOST TOR does not have a direct path to destination group, take local path to the appropriate switch

          routeback->push_back(queues_switch_host[srcswitch][HOST_TOR(src)]);
          routeback->push_back(pipes_switch_host[srcswitch][HOST_TOR(src)]);

          if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
          routeback->push_back(queues_host_switch[srcswitch][HOST_TOR(src)]->getRemoteEndpoint());
          }
    
          routeback->push_back(queues_switch_host[HOST_TOR(src)][src]);
          routeback->push_back(pipes_switch_host[HOST_TOR(src)][src]);

          routeout->set_reverse(routeback);
          routeback->set_reverse(routeout);
        */

        //print_route(*routeout);                                                                                            
        paths->push_back(routeout);
        check_non_null(routeout);

        for (uint32_t p = 0;p < _no_of_groups; p++){
            if (p==srcgroup || p==dstgroup)
                continue;
        
            //add indirect paths via random group;
            routeout = new Route();

            assert(queues_host_switch[src][HOST_TOR(src)]);
        
            routeout->push_back(queues_host_switch[src][HOST_TOR(src)]);
            routeout->push_back(pipes_host_switch[src][HOST_TOR(src)]);
        
            //cout << "DPSRC " << src << " SW " << HOST_TOR(src) << " " << queues_host_switch[src][HOST_TOR(src)]  << " ";
        
            if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
                routeout->push_back(queues_host_switch[src][HOST_TOR(src)]->getRemoteEndpoint());
        
            uint32_t intergroup = p;
        
            while (intergroup==srcgroup || intergroup==dstgroup)
                intergroup = rand()%_no_of_groups;

            //cout << "Groups " << srcgroup  << "  "  << intergroup << " " << dstgroup << endl;
        
            uint32_t srcswitch,dstswitch,interswitch1,interswitch2;
            //find srcswitch from srcgroup which has a path to intergroup and dstswitch from intergroup which has an incoming path from srcgroup.

            if (srcgroup<intergroup){
                srcswitch = srcgroup * _a + (intergroup-1)/_h;
                interswitch1 =  intergroup * _a + srcgroup/_h;
            }
            else {
                srcswitch = srcgroup * _a + intergroup/_h;
                interswitch1 =  intergroup * _a + (srcgroup-1)/_h;
            }
        
            if (HOST_TOR(src)!=srcswitch){
                /*When SRC HOST TOR does not have a direct path to destination group, take local path to the appropriate switch*/
            
                assert(queues_switch_switch[HOST_TOR(src)][srcswitch]);
                routeout->push_back(queues_switch_switch[HOST_TOR(src)][srcswitch]);
                routeout->push_back(pipes_switch_switch[HOST_TOR(src)][srcswitch]);
            
                //cout << "SW " << srcswitch <<        " " << queues_switch_switch[HOST_TOR(src)][srcswitch] << " ";
            
                if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
                    routeout->push_back(queues_switch_switch[HOST_TOR(src)][srcswitch]->getRemoteEndpoint());
            }
        
            /* path from source group to inter group*/
            assert(queues_switch_switch[srcswitch][interswitch1]);
            routeout->push_back(queues_switch_switch[srcswitch][interswitch1]);
            routeout->push_back(pipes_switch_switch[srcswitch][interswitch1]);
        
            //cout << "SW " << interswitch1 <<        " ";    
        
            if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
                routeout->push_back(queues_host_switch[srcswitch][interswitch1]->getRemoteEndpoint());
        
            //route from inter group to destination group.
            if (intergroup<dstgroup){
                interswitch2 = intergroup * _a + (dstgroup-1)/_h;
                dstswitch =  dstgroup * _a + intergroup/_h;
            }
            else {
                interswitch2 = intergroup * _a + dstgroup/_h;
                dstswitch =  dstgroup * _a + (intergroup-1)/_h;
            }

            if (interswitch1 != interswitch2){
                /*When interswitch1 does not have a direct path to destination group, take local path to the appropriate switch*/
                //cout << "SW " << interswitch2 <<      " ";
                assert(queues_switch_switch[interswitch1][interswitch2]);
            
                routeout->push_back(queues_switch_switch[interswitch1][interswitch2]);
                routeout->push_back(pipes_switch_switch[interswitch1][interswitch2]);
            
                if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
                    routeout->push_back(queues_switch_switch[interswitch1][interswitch2]->getRemoteEndpoint());
            }

            /* path from inter group to destgroup*/
            assert(queues_switch_switch[interswitch2][dstswitch]);
            routeout->push_back(queues_switch_switch[interswitch2][dstswitch]);
            routeout->push_back(pipes_switch_switch[interswitch2][dstswitch]);
        
            //cout << "SW " << dstswitch <<        " ";
        
            if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
                routeout->push_back(queues_host_switch[interswitch2][dstswitch]->getRemoteEndpoint());
        
            if (dstswitch!=HOST_TOR(dest)){
                /*When dstswitch does not have a direct path to dest, take local path to the appropriate TOR switch*/
                //cout << "SW " << HOST_TOR(dest) <<        " ";
                assert(queues_switch_switch[dstswitch][HOST_TOR(dest)]);
            
                routeout->push_back(queues_switch_switch[dstswitch][HOST_TOR(dest)]);
                routeout->push_back(pipes_switch_switch[dstswitch][HOST_TOR(dest)]);
            
                if (qt==LOSSLESS_INPUT || qt==LOSSLESS_INPUT_ECN)
                    routeout->push_back(queues_switch_switch[dstswitch][HOST_TOR(dest)]->getRemoteEndpoint());
            }
    
            //cout << "DEST " << dest <<        " " << endl;
            assert(queues_switch_host[HOST_TOR(dest)][dest]);
        
            routeout->push_back(queues_switch_host[HOST_TOR(dest)][dest]);
            routeout->push_back(pipes_switch_host[HOST_TOR(dest)][dest]);

            // reverse path for RTS packets                                                                                      
            //routeback = new Route();
            /*TODO*/
            //routeout->set_reverse(routeback);
            //routeback->set_reverse(routeout);
        
            //print_route(*routeout);                                                                                            
            paths->push_back(routeout);
            check_non_null(routeout);
        }
    
        return paths;
    }
}

int64_t DragonFlyTopology::find_switch(Queue* queue){
    //first check host to switch
    for (uint32_t i=0;i<_no_of_nodes;i++)
        for (uint32_t j = 0;j<_no_of_switches;j++)
            if (queues_host_switch[i][j]==queue)
                return j;

    for (uint32_t i=0;i<_no_of_switches;i++)
        for (uint32_t j = 0;j<_no_of_switches;j++)
            if (queues_switch_switch[i][j]==queue)
                return j;

    return -1;
}

int64_t DragonFlyTopology::find_destination(Queue* queue){
    for (uint32_t i=0;i<_no_of_switches;i++)
        for (uint32_t j = 0;j<_no_of_nodes;j++)
            if (queues_switch_host[i][j]==queue)
                return j;

    return -1;
}



void DragonFlyTopology::print_path(std::ofstream &paths, uint32_t src, const Route* route){
    paths << "SRC_" << src << " ";
  
    if (route->size()/2==2){
        paths << "SW_" << find_switch((Queue*)route->at(1)) << " ";
        paths << "DST_" << find_destination((Queue*)route->at(3)) << " ";
    } else if (route->size()/2==3){
        paths << "SW_" << find_switch((Queue*)route->at(1)) << " ";
        paths << "SW_" << find_switch((Queue*)route->at(3)) << " ";
        paths << "SW_" << find_switch((Queue*)route->at(5)) << " ";
        paths << "DST_" << find_destination((Queue*)route->at(7)) << " ";
    } else if (route->size()/2==6){
        paths << "SW_" << find_switch((Queue*)route->at(1)) << " ";
        paths << "SW_" << find_switch((Queue*)route->at(3)) << " ";
        paths << "SW_" << find_switch((Queue*)route->at(5)) << " ";
        paths << "SW_" << find_switch((Queue*)route->at(7)) << " ";
        paths << "SW_" << find_switch((Queue*)route->at(9)) << " ";
        paths << "DST_" << find_destination((Queue*)route->at(11)) << " ";
    } else {
        paths << "Wrong hop count " << ntoa(route->size()/2);
    }
  
    paths << endl;
}
