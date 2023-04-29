// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef GENERIC_TOPO
#define GENERIC_TOPO
#include "main.h"
#include "randomqueue.h"
#include "pipe.h"
#include "config.h"
#include "loggers.h"
#include "network.h"
#include "firstfit.h"
#include "topology.h"
#include "logfile.h"
#include "eventlist.h"
#include "switch.h"
#include <ostream>
#include <fstream>


class Host : public PacketSink, public Drawable {
public:
    Host(string s) :_queue(0) { _nodename= s;}
    void setQueue(BaseQueue *q) {_queue=q;}
    // inherited from PacketSink - we shouldn't normally be receiving
    // a packet on a host - the route already makes the switching
    // decision direct to the receiving protocol.  May revisit this later.
    virtual void receivePacket(Packet& pkt) {abort();} 
    const string& nodename() {return _nodename;}
private:
    string _nodename;
    BaseQueue* _queue;
};

class GenericTopology: public Topology {
public:
    GenericTopology(Logfile* lg, EventList* ev);
    bool load(const char *filename);
    void draw();
    virtual vector<const Route*>* get_bidir_paths(uint32_t src, uint32_t dest, bool reverse);

    //void count_queue(RandomQueue*);
    //void print_path(std::ofstream& paths, uint32_t src, const Route* route);
    vector<uint32_t>* get_neighbours(uint32_t src);
    uint32_t no_of_nodes() const {return _no_of_hosts;}
private:
    void parse_host(std::vector<std::string>& tokens, int pass, std::fstream& gv);
    void parse_switch(std::vector<std::string>& tokens, int pass, std::fstream& gv);
    void parse_queue(std::vector<std::string>& tokens, int pass, std::fstream& gv);
    void parse_pipe(std::vector<std::string>& tokens, int pass, std::fstream& gv);
    Host* find_host(const string& id);
    Switch* find_switch(const string& id);
    BaseQueue* find_queue(const string &id);
    Pipe* find_pipe(const string &id);
    bool load(FILE* f, int pass);
    vector <Host*> _hosts;
    vector <Switch*> _switches;
    vector <Pipe*> _pipes;
    vector <BaseQueue*> _queues;
    uint32_t _no_of_hosts;
    uint32_t _no_of_links;
    uint32_t _no_of_switches;
    Logfile* _logfile;
    EventList* _eventlist;
};

#endif
