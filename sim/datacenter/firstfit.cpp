#include "firstfit.h"
#include <iostream>

FirstFit::FirstFit(simtime_picosec scanPeriod, EventList& eventlist, vector<const Route*>*** n) : EventSource(eventlist,"FirstFit"), _scanPeriod(scanPeriod) /*, _init(0)*/
{
  eventlist.sourceIsPendingRel(*this, _scanPeriod);
  net_paths = n;

  threshold = (int)(timeAsSec(_scanPeriod) * HOST_NIC * 100);

  printf("Threshold is %d\n",threshold);
}

void FirstFit::doNextEvent() {
  run();
  eventlist().sourceIsPendingRel(*this, _scanPeriod);
}

void FirstFit::add_flow(int src, int dest,TcpSrc* flow){
  //  connections[src][dest] = flow;
  assert(flow->_route);
  flow_counters[flow] = new flow_entry(0,0,src,dest,flow->_route);
}

void FirstFit::add_queue(BaseQueue* queue){
  assert(queue!=NULL);
  path_allocations[queue] = 0;
}

void FirstFit::run(){
  int crt_counter = 0,delta = 0;
  TcpSrc* tcp;
  flow_entry* f;

  //update stats for all the flows
  map<TcpSrc*,flow_entry*>::iterator it;

  //try to remove flows first
  for (it = flow_counters.begin();it!=flow_counters.end();it++){
    tcp = (TcpSrc*)(*it).first;
    f = (flow_entry*)(*it).second;

    crt_counter = tcp->_last_acked;
    delta = crt_counter-f->byte_counter;

    if (f->allocated&&delta<threshold){
      //say this link is free for others
      f->allocated = 0;
      
      //
      //printf("Removing flow %d %d from path because delta is %d\n",f->src,f->dest,delta);

      cout << "R";
      
      for (unsigned int i=1;i<f->route->size()-1;i+=2){
        path_allocations[(BaseQueue*)f->route->at(i)] -= 1;
      }
    }
  }

  //increase detection speed!
  if (delta<0){
    f->byte_counter = 0;
    delta = crt_counter;
  }

  for (it = flow_counters.begin();it!=flow_counters.end();it++){
    tcp = (TcpSrc*)(*it).first;
    f = (flow_entry*)(*it).second;

    crt_counter = tcp->_last_acked;
    delta = crt_counter-f->byte_counter;
    f->byte_counter = crt_counter;

    if (!f->allocated && delta > threshold){
      int best_route = -1, best_cost = 10000000;
      int crt_cost;

      for (unsigned int p = 0;p<net_paths[f->src][f->dest]->size();p++){
        const Route* crt_route = net_paths[f->src][f->dest]->at(p);
        crt_cost = 0;

        for (unsigned int i=1;i<crt_route->size()-1;i+=2)
          if (path_allocations[(BaseQueue*)crt_route->at(i)]>crt_cost)
            crt_cost = path_allocations[(BaseQueue*)crt_route->at(i)];
                                        
        if (crt_cost<best_cost){
          best_cost = crt_cost;
          best_route = p;
        }
      }
      assert(best_route>-1);
      //set route!
      f->allocated = 1;
      //printf("Switching flow %d %d to path %d\n",f->src,f->dest,best_route);
      cout << "S";

      Route* new_route = new Route(*(net_paths[f->src][f->dest]->at(best_route)));
      new_route->push_back(tcp->_sink);

      tcp->replace_route(new_route);
      f->route = new_route;

      for (unsigned int i=1;i<tcp->_route->size()-1;i+=2){
        path_allocations[(BaseQueue*)new_route->at(i)] += 1;
      }
    }
  }
}
