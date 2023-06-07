#include "star_topology.h"
#include <vector>
#include "string.h"
#include <sstream>

#include <iostream>
#include "main.h"

string ntoa(double n);
string itoa(uint64_t n);

StarTopology::StarTopology(Logfile* lg, EventList* ev,FirstFit * fit,simtime_picosec rtt){
  logfile = lg;
  eventlist = ev;
  ff = fit;
  _rtt = rtt;
  
  _no_of_nodes = NSRV;

  init_network();
}

void StarTopology::init_network(){
  QueueLoggerSampling* queueLogger;
  
  for (uint32_t i=0;i<NSRV;i++){
    pipe_in_ns[i] = NULL;
    pipe_out_ns[i] = NULL;
    queue_in_ns[i] = NULL;
    queue_out_ns[i] = NULL;
  }

  for (uint32_t j = 0; j < NSRV; j++) {
    // Downlink
    queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
    //queueLogger = NULL;
    logfile->addLogger(*queueLogger);

    queue_in_ns[j] = new RandomQueue(speedFromPktps(HOST_NIC), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
    queue_in_ns[j]->setName("IN_" + ntoa(j));
    logfile->writeName(*(queue_in_ns[j]));
          
    pipe_in_ns[j] = new Pipe(_rtt, *eventlist);
    pipe_in_ns[j]->setName("Pipe-in-" + ntoa(j));
    logfile->writeName(*(pipe_in_ns[j]));

    queueLogger = new QueueLoggerSampling(timeFromMs(1000), *eventlist);
    //queueLogger = NULL;
    logfile->addLogger(*queueLogger);
    queue_out_ns[j] = new RandomQueue(speedFromPktps(HOST_NIC), memFromPkt(SWITCH_BUFFER + RANDOM_BUFFER), *eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
    queue_out_ns[j]->setName("OUT_" + ntoa(j));
    logfile->writeName(*(queue_out_ns[j]));
          
    pipe_out_ns[j] = new Pipe(_rtt, *eventlist);
    pipe_out_ns[j]->setName("Pipe-out-" + ntoa(j));
    logfile->writeName(*(pipe_out_ns[j]));
          
    if (ff){
      ff->add_queue(queue_in_ns[j]);
      ff->add_queue(queue_out_ns[j]);
    }
  }
}

void check_non_null(Route* rt);

vector<const Route*>* StarTopology::get_paths(uint32_t src, uint32_t dest){
  vector<const Route*>* paths = new vector<const Route*>();

  Route* routeout;

    Queue* pqueue = new Queue(speedFromPktps(HOST_NIC), memFromPkt(FEEDER_BUFFER), *eventlist, NULL);
    pqueue->setName("PQueue_" + ntoa(src) + "_" + ntoa(dest));
    //logfile->writeName(*pqueue);
  
    routeout = new Route();
    routeout->push_back(pqueue);

    routeout->push_back(queue_out_ns[src]);
    routeout->push_back(pipe_out_ns[src]);

    routeout->push_back(queue_in_ns[dest]);
    routeout->push_back(pipe_in_ns[dest]);

    paths->push_back(routeout);
    check_non_null(routeout);
    return paths;
}

void StarTopology::count_queue(RandomQueue* queue){
  if (_link_usage.find(queue)==_link_usage.end()){
    _link_usage[queue] = 0;
  }

  _link_usage[queue] = _link_usage[queue] + 1;
}

void StarTopology::print_path(std::ofstream &paths, uint32_t src, const Route* route){
  paths << "SRC_" << src << " ";
  paths << endl;
}

vector<uint32_t>* StarTopology::get_neighbours(uint32_t src){
  return NULL;
}

