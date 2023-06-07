// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "camcubetopology.h"
#include <vector>
#include "string.h"
#include <sstream>

#include <iostream>
#include "randomqueue.h"
#include "compositequeue.h"
#include "compositeprioqueue.h"
#include "main.h"

string ntoa(double n);
string itoa(uint64_t n);

CamCubeTopology::CamCubeTopology(Logfile* lg, EventList* ev,queue_type qt,simtime_picosec rtt){
    logfile = lg;
    _rtt = rtt;
    eventlist = ev;
    this->qt = qt;
    init_network();
}

#define ADDRESS(srv,level) (level==0?srv%K:srv/(int)pow(K,level))

Queue* CamCubeTopology::alloc_src_queue(QueueLogger* queueLogger){
    return  new PriorityQueue(speedFromMbps((uint64_t)HOST_NIC), memFromPkt(FEEDER_BUFFER), *eventlist, queueLogger);
}

Queue* CamCubeTopology::alloc_queue(QueueLogger* queueLogger){
    return alloc_queue(queueLogger, HOST_NIC);
}

Queue* CamCubeTopology::alloc_queue(QueueLogger* queueLogger, uint64_t speed){
    if (qt==RANDOM) {
        return new RandomQueue(speedFromMbps(speed), 
                                                           memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), 
                                                           *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
    } else if (qt==COMPOSITE) {
        return new CompositeQueue(speedFromMbps(speed), memFromPkt(8), 
                                  *eventlist, queueLogger);
    } else if (qt==COMPOSITE_PRIO) {
        return new CompositePrioQueue(speedFromMbps(speed), memFromPkt(8), 
                                      *eventlist, queueLogger);
    } 
    
    assert(0);
}

int CamCubeTopology::srv_from_address(unsigned int* address){
    return address[0]+address[1]*K+address[2]*K*K;
}

void CamCubeTopology::address_from_srv(int srv, unsigned int* address){
    address[2] = srv/(K*K);
    address[1] = (srv-address[2]*K*K)/K;
    address[0] = srv%K;
}

void CamCubeTopology::init_network(){
    QueueLoggerSampling* queueLogger;

    assert(K>=0);
    assert(NUM_SRV==(int)pow(K,3));
    
    for (int i=0;i<NUM_SRV;i++){
        address_from_srv(i,addresses[i]);
        
        //must add 6 neighbors
        for (int k=0;k<6;k++){
            pipes[i][k] = NULL;
            queues[i][k] = NULL;

            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);
            prio_queues[i][k] = alloc_src_queue(queueLogger);
            prio_queues[i][k]->setName("PRIO_SRV_" + ntoa(i) + "(" + ntoa(k) + ")");
        }
    }

    for (int l=0;l<3;l++){
        //create links for axis l
        for (int i=0;i<NUM_SRV;i++){
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);
            
            string name = ntoa(addresses[i][0])+ntoa(addresses[i][1])+ntoa(addresses[i][2]);

            queues[i][l] = alloc_queue(queueLogger);
            queues[i][l]->setName("SRV_" + name + "(axis_" + ntoa(l)+")_pos");
            logfile->writeName(*(queues[i][l]));
            
            pipes[i][l] = new Pipe(_rtt, *eventlist);
            pipes[i][l]->setName("Pipe-SRV_" + name + "(axis_" + ntoa(l)+")_pos");
            logfile->writeName(*(pipes[i][l]));

            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
            logfile->addLogger(*queueLogger);
            
            queues[i][l+3] = alloc_queue(queueLogger);
            queues[i][l+3]->setName("SRV_" + name + "(axis_" + ntoa(l)+")_neg");
            logfile->writeName(*(queues[i][l+3]));
            
            pipes[i][l+3] = new Pipe(_rtt, *eventlist);
            pipes[i][l+3]->setName("Pipe-SRV_" + name + "(axis_" + ntoa(l)+")_neg");
            logfile->writeName(*(pipes[i][l+3]));
        }
    }
}

int CamCubeTopology::get_distance(int src,int dest,int dimension,int* iface){
    int a,b;
    
    a = (addresses[src][dimension]+K-addresses[dest][dimension])%K;
    b = (addresses[dest][dimension]+K-addresses[src][dimension])%K;
    
    if (a<b){
        if (iface!=NULL)
            *iface = dimension+3;
        return a;
    }
    else {
        if (iface!=NULL)
            *iface = dimension;
        return b;
    }
}

vector<const Route*>* CamCubeTopology::get_paths(int src, int dest){
    vector<const Route*>* paths = new vector<const Route*>(); 
    vector<Route*> *p = get_paths_camcube(src,dest,1);
    for (unsigned int i = 0;i<p->size();i++){
        paths->push_back(p->at(i));
    };

    delete p;
    return paths;
}

vector<Route*>* CamCubeTopology::get_paths_camcube(int src, int dest, int first){
    vector<Route*>* paths = new vector<Route*>(), *ret_paths; 
    unsigned int crt[3],n;

    assert(src!=dest);

    cout << "Getting path from " <<ntoa(src) << " to " << ntoa(dest) << endl;

    int ifx,x = get_distance(src,dest,0,&ifx);
    int ify,y = get_distance(src,dest,1,&ify);
    int ifz,z = get_distance(src,dest,2,&ifz);
    
    if (x>=1){
        memcpy(crt,addresses[src],3*sizeof(unsigned int));
    
        if (ifx<3)
            crt[0] = (crt[0]+1)%K;
        else
            crt[0] = (crt[0]+K-1)%K;            
        
        n = srv_from_address(crt);
        
        if (n==dest){
            Route* t = new Route();

            t->push_back(queues[src][ifx]);
            t->push_back(pipes[src][ifx]);

            if (first)
                t->push_front(prio_queues[src][ifx]);

            paths->push_back(t);
            return paths;
        }

        ret_paths = get_paths_camcube(n,dest,0);
        //prepend the current hop to all returned paths;
        for (unsigned int i = 0;i<ret_paths->size();i++){
            ret_paths->at(i)->push_front(pipes[src][ifx]);
            ret_paths->at(i)->push_front(queues[src][ifx]);
            if (first)
                ret_paths->at(i)->push_front(prio_queues[src][ifx]);

            paths->push_back(ret_paths->at(i));
        }
        delete ret_paths;
    };

    if (y>=1){
        memcpy(crt,addresses[src],3*sizeof(unsigned int));
    
        if (ify<3)
            crt[1] = (crt[1]+1)%K;
        else
            crt[1] = (crt[1]+K-1)%K;            
        
        n = srv_from_address(crt);
        
        if (n==dest){
            Route* t = new Route();
            t->push_back(queues[src][ify]);
            t->push_back(pipes[src][ify]);
            if (first)
                t->push_front(prio_queues[src][ify]);

            paths->push_back(t);
            return paths;
        }

        ret_paths = get_paths_camcube(n,dest,0);
        //prepend the current hop to all returned paths;
        for (unsigned int i = 0;i<ret_paths->size();i++){
            ret_paths->at(i)->push_front(pipes[src][ify]);
            ret_paths->at(i)->push_front(queues[src][ify]);
            if (first)
                ret_paths->at(i)->push_front(prio_queues[src][ify]);

            paths->push_back(ret_paths->at(i));
        }
        delete ret_paths;
    };


    if (z>=1){
        memcpy(crt,addresses[src],3*sizeof(unsigned int));
    
        if (ifz<3)
            crt[2] = (crt[2]+1)%K;
        else
            crt[2] = (crt[2]+K-1)%K;            
        
        n = srv_from_address(crt);
        
        if (n==dest){
            Route* t = new Route();
            t->push_back(queues[src][ifz]);
            t->push_back(pipes[src][ifz]);

            if (first)
                t->push_front(prio_queues[src][ifz]);

            paths->push_back(t);
            return paths;
        }

        ret_paths = get_paths_camcube(n,dest,0);
        //prepend the current hop to all returned paths;
        for (unsigned int i = 0;i<ret_paths->size();i++){
            ret_paths->at(i)->push_front(pipes[src][ifz]);
            ret_paths->at(i)->push_front(queues[src][ifz]);
            if (first)
                ret_paths->at(i)->push_front(prio_queues[src][ifz]);

            paths->push_back(ret_paths->at(i));
        }
        delete ret_paths;
    };

    return paths;
}

void CamCubeTopology::print_paths(std::ofstream & p,int src,vector<const Route*>* paths){
  for (unsigned int i=0;i<paths->size();i++)
    print_path(p, src, paths->at(i));
}

void CamCubeTopology::print_path(std::ofstream &paths,int src,const Route* route){
  paths << "SRC_" << src << " ";

  for (unsigned int i=0;i<route->size()-1;i++){
    if (route->at(i)!=NULL)
        paths << (route->at(i))->nodename() << " ";
    else 
      paths << "NULL ";
  }
  paths << endl;
}

