#include "incast.h"
#include <iostream>
#include <vector>
using namespace std;

Incast::Incast(uint64_t b, EventList& eventlist) : EventSource(eventlist,"Incast"),  _finished(0), _bytes(b) 
{}

void Incast::doNextEvent() {
  if (_flows.size()==0)
    return;

  _finished ++;

  //  cout << "FLOWS FINISHED " << _finished << " out of " << _flows.size()<<endl;
  
  if (_finished>=_flows.size()){
    vector<TcpSrcTransfer*>::iterator it;
    
    //try to remove flows first
    for (it = _flows.begin();it!=_flows.end();it++){
      TcpSrcTransfer* tcp = *it;    
      tcp->reset(_bytes,1);
    }
    cout << "Transfer Finished at " << timeAsMs(eventlist().now()) << endl;
    _finished = 0;
  }
}

void Incast::addFlow(TcpSrcTransfer* src){
  _flows.push_back(src);
}
