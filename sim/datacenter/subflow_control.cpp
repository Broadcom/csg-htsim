#include "subflow_control.h"
#include <iostream>
#include "fat_tree_topology.h"

SubflowControl::SubflowControl(simtime_picosec scanPeriod, Logfile* lg, SinkLoggerSampling* sl,
                               EventList& eventlist, TcpRtxTimerScanner* rtx,  
                               vector<const Route*>*** n, std::ofstream* p, int ms) 
  : EventSource(eventlist,"SubflowControl"), _scanPeriod(scanPeriod)
{
  eventlist.sourceIsPendingRel(*this, _scanPeriod);
  net_paths = n;
  _max_subflows = ms;

  paths = p;

  sinkLogger = sl;

  logfile = lg;

  _rtx = rtx;

  threshold = (int)(timeAsSec(_scanPeriod) * HOST_NIC * 100 * 8);

  printf("Threshold is %d\n",threshold);
}

void SubflowControl::doNextEvent() {
  run();
  eventlist().sourceIsPendingRel(*this, _scanPeriod);
}

void SubflowControl::add_flow(int src, int dest,MultipathTcpSrc* flow){
  flow_counters[flow] = new multipath_flow_entry(0,src,dest);
}

void SubflowControl::add_subflow(MultipathTcpSrc* flow, int choice, int structure){
  assert(flow_counters[flow]!=NULL);

  flow_counters[flow]->subflows->push_back(choice);
  if (structure!=-1)
    flow_counters[flow]->structure->push_back(structure);
}

void SubflowControl::run(){
  int crt_counter = 0,delta = 0;
  MultipathTcpSrc* mtcp;
  multipath_flow_entry* f;

  //update stats for all the flows
  map<MultipathTcpSrc*,multipath_flow_entry*>::iterator it;

  for (it = flow_counters.begin();it!=flow_counters.end();it++){
    mtcp = (MultipathTcpSrc*)(*it).first;
    f = (multipath_flow_entry*)(*it).second;

    crt_counter = mtcp->compute_total_bytes();
    delta = crt_counter-f->byte_counter;

    int counts = f->byte_counter!=0;

    f->byte_counter = crt_counter;

    //    cout << "Delta " << delta << "subs " <<f->subflows->size() << "max " << _max_subflows << "counts " << counts << endl ;
    if (counts && delta<threshold && f->subflows->size()<_max_subflows && f->subflows->size()<net_paths[f->src][f->dest]->size()){
      //add new subflow!
      TcpSrc* tcpSrc = new TcpSrc(NULL, NULL, eventlist());
      TcpSink* tcpSnk = new TcpSink();
              
      tcpSrc->setName("mtcp_" + ntoa(f->src) + "_" + ntoa(f->subflows->size()) + "_" + ntoa(f->dest));
      logfile->writeName(*tcpSrc);
              
      tcpSnk->setName("mtcp_sink_" + ntoa(f->src) + "_" + ntoa(f->subflows->size()) + "_" + ntoa(f->dest));
      logfile->writeName(*tcpSnk);
      
      _rtx->registerTcp(*tcpSrc);
      
      int found;
      int choice;
      do {
        found = 0;
                
        if (net_paths[f->src][f->dest]->size()==K*K/4 && f->subflows->size() < K/2){
          assert(f->structure->size()==f->subflows->size());
          choice = rand()%(K/2);

          for (unsigned int cnt = 0;cnt<f->structure->size();cnt++){
            if (f->structure->at(cnt)==choice){
              found = 1;
              break;
            }
          }
        }
        else {
          choice = rand()%net_paths[f->src][f->dest]->size();
        
          for (unsigned int cnt = 0;cnt<f->subflows->size();cnt++){
            if (f->subflows->at(cnt)==choice){
              found = 1;
              break;
            }
          }
        }
      }while(found);

      if (net_paths[f->src][f->dest]->size()==K*K/4 && f->subflows->size() < K/2){
        f->structure->push_back(choice);
        choice = choice * K/2 + rand()%2;
      }

      f->subflows->push_back(choice);

      //cout << endl << ntoa(f->subflows->size()) << " subflows between " << ntoa(f->src) << " and " << ntoa(f->dest) << " at " << timeAsMs(eventlist().now()) << endl ;

      Route* routeout = new Route(*(net_paths[f->src][f->dest]->at(choice)));
      routeout->push_back(tcpSnk);
              
      Route* routein = new Route();
      routein->push_back(tcpSrc);
      double extrastarttime = timeAsMs(eventlist().now()) + timeAsMs(_scanPeriod) * drand()+1;
      
      //join multipath connection

      mtcp->addSubflow(tcpSrc);
      tcpSrc->connect(*routeout, *routein, *tcpSnk, timeFromMs(extrastarttime));
            
      sinkLogger->monitorSink(tcpSnk);

      if (paths){
        *paths << "Route from "<< ntoa(f->src) << " to " << ntoa(f->dest) << " -> " ;
        print_path(*paths, routeout);
      }
    }
  }
}

void SubflowControl::print_stats(){
  MultipathTcpSrc* mtcp;
  multipath_flow_entry* f;

  //update stats for all the flows
  map<MultipathTcpSrc*,multipath_flow_entry*>::iterator it;

  for (it = flow_counters.begin();it!=flow_counters.end();it++){
    mtcp = (MultipathTcpSrc*)(*it).first;
    f = (multipath_flow_entry*)(*it).second;

    cout << endl << ntoa(f->subflows->size()) << " subflows between " << ntoa(f->src) << " and " << ntoa(f->dest) << endl ;
  }
}
