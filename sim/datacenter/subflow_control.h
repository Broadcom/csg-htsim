#ifndef subflowcontrol
#define subflowcontrol

#include "main.h"
#include "mtcp.h"
#include "randomqueue.h"
#include "eventlist.h"
#include "loggers.h"
#include <list>
#include <iostream>
#include <map>
#include <string>

string ntoa(double i);

struct multipath_flow_entry {
  int byte_counter;
  vector<int>* subflows;
  vector<int>* structure;
  int src;
  int dest;

  multipath_flow_entry(int bc, int s, int d){
    byte_counter = bc;
    src = s;
    dest = d;
    subflows = new vector<int>();
    structure = new vector<int>();
  }
};

class SubflowControl: public EventSource{
 public:
  SubflowControl(simtime_picosec scanPeriod, Logfile* lg, SinkLoggerSampling* sl,
                 EventList& eventlist, TcpRtxTimerScanner* rtx, vector<const Route*>*** np, 
                 std::ofstream* p,int max_subflows);
  void doNextEvent();

  void init();
  void run();
  void add_flow(int src,int dest,MultipathTcpSrc* flow);
  void add_subflow(MultipathTcpSrc* m, int subflow, int structure = -1);

  vector<const Route*>*** net_paths;
  std::ofstream* paths;

  void print_stats();
 private:
  map<MultipathTcpSrc*,multipath_flow_entry*> flow_counters;

  simtime_picosec _scanPeriod;  
  int threshold;
  int _max_subflows;
  TcpRtxTimerScanner* _rtx;
  SinkLoggerSampling* sinkLogger;
  Logfile * logfile;
};

#endif
