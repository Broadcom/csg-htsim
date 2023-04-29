#include "shortflows.h"
#include <iostream>

string ntoa(double n);
string itoa(uint64_t n);

ShortFlows::ShortFlows(double lambda, EventList& eventlist, vector<const Route*>*** n,
                       ConnectionMatrix* conns,Logfile* logfile,TcpRtxTimerScanner * rtx)
  : EventSource(eventlist,"ShortFlows")
{
  eventlist.sourceIsPendingRel(*this, timeFromMs(1000));
  net_paths = n;
  _traffic_matrix = conns->getAllConnections();
  this->logfile = logfile;
  this->_lambda = lambda;

  tcpRtxScanner = rtx;
}

ShortFlow* ShortFlows::createConnection(int src, int dst, simtime_picosec starttime){
    ShortFlow* f = new ShortFlow();
    f->src = new TcpSrcTransfer(NULL,NULL,eventlist(),70000,net_paths[src][dst]);
    f->snk = new TcpSinkTransfer();

    int pos = connections[src][dst].size();

    f->src->setName("sf_" + ntoa(src) + "_" + ntoa(dst)+"("+ntoa(pos)+")");
    logfile->writeName(*(f->src));
              
    f->snk->setName("sf_sink_" + ntoa(src) + "_" + ntoa(dst)+ "("+ntoa(pos)+")");
    logfile->writeName(*(f->snk));
    
    tcpRtxScanner->registerTcp(*(f->src));

    int choice = rand()%net_paths[src][dst]->size();

    Route* routeout = new Route(*(net_paths[src][dst]->at(choice)));
    routeout->push_back(f->snk);
    
    Route* routein = new Route();
    routein->push_back(f->src);
    
    f->src->connect(*routeout, *routein, *(f->snk), starttime);
    return f;
}

void ShortFlows::doNextEvent() {
  run();
  
  simtime_picosec nextArrival = (simtime_picosec)(exponential(_lambda)*timeFromSec(1));
  eventlist().sourceIsPendingRel(*this, nextArrival);
}

void ShortFlows::run(){
  //randomly choose connection to activate.
  int pos = rand()%_traffic_matrix->size();

  //look for inactive connection
  connection* c = _traffic_matrix->at(pos);
  
  ShortFlow* f = NULL;
  for (unsigned int i=0;i<connections[c->src][c->dst].size();i++){
    f = connections[c->src][c->dst][i];
    if (!f->src->_is_active)
      break;
  }

  if (!f||f->src->_is_active){
    f = createConnection(c->src,c->dst,eventlist().now());
    connections[c->src][c->dst].push_back(f);
  }
  else {
    f->src->reset(70000, true);
  }
}
