// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include "generic_topology.h"
#include "config.h"
#include "compositequeue.h"
#include <string>
#include <algorithm>
#include <stdlib.h>
#include <fstream>

// A topology that is loadable from a file

GenericTopology::GenericTopology(Logfile* lg, EventList* ev){
  _logfile = lg;
  _eventlist = ev;
}


bool GenericTopology::load(const char* filename){
    FILE* f = fopen(filename,"r");
    if (!f)
        return false;

    // we need to do two passes, one to build the ID tables, and one to fill in all the cross-references
    bool result = load(f, 0);
    if (!result) {
        return false;
    }
    f = fopen(filename,"r");
    return load(f, 1);
}

char* skip_whitespace(char *s) {
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

bool trim_comma(string& str) {
    uint32_t len = str.size();
    if (str[len-1] == ',') {
        str.erase(len-1);
        return true;
    }
    return false;
}

void tokenize(string str, std::vector<std::string>& tokens) {
    std::string buf;                 // Have a buffer string
    std::stringstream ss(str);       // Insert the string into a stream
    while (ss >> buf) {
        if (buf != "") {
            bool has_comma = trim_comma(buf);
            tokens.push_back(buf);
            if (has_comma) {
                tokens.push_back(",");
            }
        }
    }
}

bool get_attribute(std::vector<std::string>& tokens, uint32_t &ix, std::vector<std::string>& attribute) {
    attribute.clear();
    if (tokens.size() <= ix + 1) {
        return false;
    }
    while (tokens.size() > ix) {
        if (tokens[ix] == ",") {
            ix++;
            return true;
        }
        attribute.push_back(tokens[ix]);
        ix++;
    }
    return true;
}

//
//  Lowercases string
//
template <typename T>
std::basic_string<T> lowercase(const std::basic_string<T>& s)
{
    std::basic_string<T> s2 = s;
    std::transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
    return s2;
}

Host* GenericTopology::find_host(const string& id) {
    // linear scan though vector is slow, but we only do this at startup so probably OK.  Otherwise add a map.
    for (uint32_t i = 0; i < _hosts.size(); i++) {
        if (_hosts[i]->nodename() == id) {
            return _hosts[i];
        }
    }
    return NULL;
}

void GenericTopology::parse_host(std::vector<std::string>& tokens, int pass, std::fstream& gv) {
    assert(tokens.size() >= 2);
    string id = tokens[1];
    assert(tokens[2] == ",");
    Host *host = 0;
    if (pass == 0) {
        if (find_host(id)) {
            cerr << "Duplicate host id " << id << " found - terminating" << endl;
            abort();
        } else {
            host = new Host(id);
            _hosts.push_back(host);
            return;
        }
    } else {
        host = find_host(id);
    }
    vector<string> attribute;
    uint32_t ix = 3;
    while (get_attribute(tokens, ix, attribute)) {
        if (attribute[0] == "queue") {
            assert(attribute.size() == 2);
            string queueid = attribute[1];
            BaseQueue *q = find_queue(queueid);
            host->setQueue(q);
            gv << id << " -> " << queueid << endl;
        } else if (attribute[0] == "pos") {
            assert(attribute.size() == 3);
            host->setPos(stoi(attribute[1]), stoi(attribute[2]));
        }
    }
}

Switch* GenericTopology::find_switch(const string& id) {
    // linear scan though vector is slow, but we only do this at startup so probably OK.  Otherwise add a map.
    for (uint32_t i = 0; i < _switches.size(); i++) {
        if (_switches[i]->nodename() == id) {
            return _switches[i];
        }
    }
    return NULL;
}

void GenericTopology::parse_switch(std::vector<std::string>& tokens, int pass, std::fstream& gv) {
    assert(tokens.size() >= 2);
    string id = tokens[1];
    assert(tokens[2] == ",");
    Switch *sw = 0;
    if (pass == 0) {
        if (find_switch(id)) {
            cerr << "Duplicate switch id " << id << " found - terminating" << endl;
            abort();
        } else {
            sw = new Switch(*_eventlist, id);
            _switches.push_back(sw);
            return;
        }
    } else {
        sw = find_switch(id);
    }
    vector<string> attribute;
    uint32_t ix = 3;
    while (get_attribute(tokens, ix, attribute)) {
        if (attribute[0] == "queue") {
            assert(attribute.size() == 2);
            string queueid = attribute[1];
            BaseQueue *q = find_queue(queueid);
            sw->addPort(q);
            gv << id << " -> " << queueid << endl;
        } else if (attribute[0] == "pos") {
            assert(attribute.size() == 3);
            sw->setPos(stoi(attribute[1]), stoi(attribute[2]));
        }
    }
}

BaseQueue* GenericTopology::find_queue(const string& id) {
    // linear scan though vector is slow, but we only do this at startup so probably OK.  Otherwise add a map.
    for (uint32_t i = 0; i < _queues.size(); i++) {
        if (_queues[i]->nodename() == id) {
            return _queues[i];
        }
    }
    return NULL;
}

void GenericTopology::parse_queue(std::vector<std::string>& tokens, int pass, std::fstream& gv) {
    assert(tokens.size() >= 2);
    string id = tokens[1];
    assert(tokens[2] == ",");
    BaseQueue *q = 0;
    if (pass == 0) {
        if (find_queue(id)) {
            cerr << "Duplicate switch id " << id << " found - terminating" << endl;
            abort();
        } 
    } else {
        q = find_queue(id);
        if (!q) cerr << "failed to find queue " << id <<endl;
        assert(q);
    }
    // On pass 0, we will create the queue, once we know what type of queue to create, and the relevant params
    // On pass 1, we will link it into the network
    vector<string> attribute;
    string queuetype;
    linkspeed_bps linkspeed = 0;
    mem_b queuesize = 0;
    QueueLogger* queuelogger = 0;
    uint32_t ix = 3;
    while (get_attribute(tokens, ix, attribute)) {
        if (pass == 0 && attribute[0] == "type") {
            assert(attribute.size() == 2);
            queuetype = lowercase(attribute[1]);
        } else if (pass == 0 && attribute[0] == "speed") {
            assert(attribute.size() == 3);
            double speed = stof(attribute[1]);
            string units = lowercase(attribute[2]);
            if (units == "gbps") {
                linkspeed = speedFromGbps(speed);
            } else if (units == "mbps") {
                linkspeed = speedFromMbps(speed);
            }
        } else if (pass == 0 && attribute[0] == "size") {
            assert(attribute.size() == 2);
            queuesize = stoi(attribute[1]);
        } else if (pass == 0 && attribute[0] == "log") {
            string logtype = lowercase(attribute[1]);
            if (logtype == "sampling") {
                if (attribute.size() != 3) {
                    cerr << "Missing log sample rate for id " << id << endl;
                    exit(1);
                }
                simtime_picosec log_period = timeFromUs(stof(attribute[2]));
                queuelogger = new QueueLoggerSampling(log_period,  *_eventlist);
                _logfile->addLogger(*queuelogger);
            }
        } else if (pass == 1 && attribute[0] == "dst") {
            if (attribute.size() != 2) {
                cerr << "Bad dst for id " << id << endl;
                exit(1);
            }
            string next_item = attribute[1];
            // a queue can feed an pipe or another queue; rarely a host directly
            PacketSink* next = find_pipe(next_item);
            if (!next) {
                next = find_queue(next_item);
            } 
            if (!next) {
                next = find_host(next_item);
            } 
            if (!next) {
                cerr << "Next item ID " << next_item << " not found for queue ID " << id << endl;
                exit(1);
            }
            q->setNext(next);
            gv << id << " -> " << next_item << endl;
        }
    }
    if (pass == 0) {
        if (linkspeed == 0) {
            cerr << "No valid linkspeed specified for Queue " << id << endl;
            exit(1);
        }
        if (queuesize == 0) {
            cerr << "No valid size specified for Queue " << id << endl;
            exit(1);
        }
        if (queuetype == "queue") {
            q = new Queue(linkspeed, queuesize, *_eventlist, queuelogger);
        } else if (queuetype == "random") {
            q = new RandomQueue(linkspeed, queuesize, *_eventlist, queuelogger, memFromPkt(RANDOM_BUFFER));
        } else if (queuetype == "composite") {
            q = new CompositeQueue(linkspeed, queuesize, *_eventlist, queuelogger);
        } else if (queuetype == "fairscheduler") {
            q = new FairScheduler(linkspeed, *_eventlist, queuelogger);
        } else {
             cerr << "No valid type specified for Queue " << id << endl;
        }
        assert(q);
        q->forceName(id);
        assert(id == q->nodename());
        _queues.push_back(q);
    }
}

Pipe* GenericTopology::find_pipe(const string& id) {
    // linear scan though vector is slow, but we only do this at startup so probably OK.  Otherwise add a map.
    for (uint32_t i = 0; i < _pipes.size(); i++) {
        if (_pipes[i]->nodename() == id) {
            return _pipes[i];
        }
    }
    return NULL;
}

void GenericTopology::parse_pipe(std::vector<std::string>& tokens, int pass, std::fstream& gv) {
    assert(tokens.size() >= 2);
    string id = tokens[1];
    Pipe *pipe = 0;
    if (pass == 0) {
        if (find_queue(id)) {
            cerr << "Duplicate pipe id " << id << " found - terminating" << endl;
            abort();
        } 
    } else {
        pipe = find_pipe(id);
        assert(pipe);
    }
    // On pass 0, we will create the pipe, once we know its latency
    // On pass 1, we will link it into the network
    vector<string> attribute;
    simtime_picosec latency = 0;
    string reverse_id = "";
    PacketSink* prev = 0;
    string prev_item = "";
    uint32_t ix = 3;
    while (get_attribute(tokens, ix, attribute)) {
        if (pass == 0 && attribute[0] == "latency") {
            assert(attribute.size() == 3);
            double latency_raw = stof(attribute[1]);
            string units = lowercase(attribute[2]);
            if (units == "s") {
                latency = timeFromSec(latency_raw);
            } else if (units == "ms") {
                latency = timeFromMs(latency_raw);
            } else if (units == "us") {
                latency = timeFromUs(latency_raw);
            } else if (units == "ns") {
                latency = timeFromNs(latency_raw);
            } else if (units == "ps") {
                latency = latency_raw;
            } else {
                cerr << "Invalid latency units for ID " << id << endl;
            }
        } else if (attribute[0] == "reverse") { // we need this on both passes
            assert(attribute.size() == 2);
            reverse_id = attribute[1];
        } else if (pass == 1 && attribute[0] == "dst") {
            if (attribute.size() != 2) {
                cerr << "Bad dst for id " << id << endl;
                exit(1);
            }
            string next_item = attribute[1];
            // a pipe can feed a switch, host, or queue
            PacketSink* next = find_switch(next_item);
            if (!next) {
                next = find_queue(next_item);
            } 
            if (!next) {
                next = find_host(next_item);
            } 
            if (!next) {
                cerr << "Next item not found for pipe ID " << id << endl;
                exit(1);
            }
            pipe->setNext(next);
            gv << id << " -> " << next_item << endl;
        } else if (pass == 1 && attribute[0] == "src") {
            if (attribute.size() != 2) {
                cerr << "Bad src for id " << id << endl;
                exit(1);
            }
            prev_item = attribute[1];
            // a pipe can feed a switch, host, or queue
            prev = find_switch(prev_item);
            if (!prev) {
                prev = find_queue(prev_item);
            } 
            if (!prev) {
                prev = find_host(prev_item);
            } 
            if (!prev) {
                cerr << "Prev item " << prev_item << " not found for pipe ID " << id << endl;
                exit(1);
            }
        }
    }
    if (pass == 0) {
        if (latency == 0) {
            cerr << "No valid linkspeed specified for Queue " << id << endl;
            exit(1);
        }
        pipe = new Pipe(latency, *_eventlist);
        pipe->forceName(id);
        assert(id == pipe->nodename());
        _pipes.push_back(pipe);
        if (reverse_id != "") {
            pipe = new Pipe(latency, *_eventlist);
            pipe->forceName(reverse_id);
            _pipes.push_back(pipe);
        }
    } else {
        if (reverse_id != "") {
            if (!prev) {
                cerr << "Src not specified for reverse pipe with ID " << reverse_id << endl;
                exit(1);
            }
            Pipe* reverse_pipe = find_pipe(reverse_id);
            assert(reverse_pipe);
            reverse_pipe->setNext(prev);
            gv << reverse_id << " -> " << prev_item << endl;
        }
    }
}

bool GenericTopology::load(FILE* f, int pass){
    cout << "Load, pass " << pass << "**************************************************************\n";
    if (fscanf(f,"Hosts %d\n",&_no_of_hosts) != 1) {
        cerr << "Parse error: failed to find number of hosts\n";
        return false;
    }
    
    if (fscanf(f,"Switches %d\n",&_no_of_switches) != 1) {
        cerr << "Parse error: Failed to find number of switches\n";
        return false;
    }
    if (fscanf(f,"Links %d\n",&_no_of_links) != 1) {
        cerr << "Parse error: Failed to find number of switches\n";
        return false;
    }

    std::fstream gv;
    gv.open ("topo.gv", std::fstream::out);
    gv << "digraph {" << endl;
     
    

    char line[256];
    std::vector<std::string> tokens;
    while(fgets(line, 255, f)) {
        tokens.clear();
        char* s = skip_whitespace(line);
        if (*s == '#' || *s == '\n') {
            // nothing to parse on this line
            continue;
        }
        if (*s == '\0') {
            break;
        }
        tokenize(string(s), tokens);
        string cmd = lowercase(tokens[0]);
        if (cmd == string("host")) {
            parse_host(tokens, pass, gv);
        } else if (cmd == string("switch")) {
            parse_switch(tokens, pass, gv);
        } else if (cmd == string("queue")) {
            parse_queue(tokens, pass, gv);
        } else if (cmd == string("pipe")) {
            parse_pipe(tokens, pass, gv);
        }
    }
    
    fclose(f);
    gv << "}\n";
    gv.close();
    return true;
}

vector<const Route*>* GenericTopology::get_bidir_paths(uint32_t src, uint32_t dest, bool reverse) {
    vector<const Route*>* paths = new vector<const Route*>();
    // TBD
    return paths;
}

vector<uint32_t>* GenericTopology::get_neighbours(uint32_t src){
  return NULL;
}

void GenericTopology::draw() {
}
