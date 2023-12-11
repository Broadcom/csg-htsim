// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "fat_tree_topology.h"
#include <vector>
#include "string.h"
#include <sstream>

#include <iostream>
#include "main.h"
#include "queue.h"
#include "fat_tree_switch.h"
#include "compositequeue.h"
#include "aeolusqueue.h"
#include "prioqueue.h"
#include "ecnprioqueue.h"
#include "queue_lossless.h"
#include "queue_lossless_input.h"
#include "queue_lossless_output.h"
#include "swift_scheduler.h"
#include "ecnqueue.h"

// use tokenize from connection matrix
extern void tokenize(string const &str, const char delim, vector<string> &out);

// default to 3-tier topology.  Change this with set_tiers() before calling the constructor.
uint32_t FatTreeTopology::_tiers = 3;
simtime_picosec FatTreeTopology::_link_latencies[] = {0,0,0};
simtime_picosec FatTreeTopology::_switch_latencies[] = {0,0,0};
uint32_t FatTreeTopology::_hosts_per_pod = 0;
uint32_t FatTreeTopology::_radix_up[] = {0,0};
uint32_t FatTreeTopology::_radix_down[] = {0,0,0};
mem_b FatTreeTopology::_queue_up[] = {0,0};
mem_b FatTreeTopology::_queue_down[] = {0,0,0};
uint32_t FatTreeTopology::_bundlesize[] = {1,1,1};
uint32_t FatTreeTopology::_oversub[] = {1,1,1};
linkspeed_bps FatTreeTopology::_downlink_speeds[] = {0,0,0};

void
FatTreeTopology::set_tier_parameters(int tier, int radix_up, int radix_down, mem_b queue_up, mem_b queue_down, int bundlesize, linkspeed_bps linkspeed, int oversub) {
    // tier is 0 for ToR, 1 for agg switch, 2 for core switch
    if (tier < CORE_TIER) {
        // no uplinks from core switches
        _radix_up[tier] = radix_up;
        _queue_up[tier] = queue_up;
    }
    _radix_down[tier] = radix_down;
    _queue_down[tier] = queue_down;
    _bundlesize[tier] = bundlesize;
    _downlink_speeds[tier] = linkspeed; // this is the link going downwards from this tier.  up/down linkspeeds are symmetric.
    _oversub[tier] = oversub;
    // xxx what to do about queue sizes
}

// load a config file and use it to create a FatTreeTopology
FatTreeTopology* FatTreeTopology::load(const char * filename, QueueLoggerFactory* logger_factory, EventList& eventlist, mem_b queuesize, queue_type q_type, queue_type sender_q_type){
    std::ifstream file(filename);
    if (file.is_open()) {
        FatTreeTopology* ft = load(file, logger_factory, eventlist, queuesize, q_type, sender_q_type);
        file.close();
	return ft;
    } else {
        cerr << "Failed to open FatTree config file " << filename << endl;
        exit(1);
    }
}

// in-place conversion to lower case
void to_lower(string& s) {
    string::iterator i;
    for (i = s.begin(); i != s.end(); i++) {
        *i = std::tolower(*i);
    }
        //std::transform(s.begin(), s.end(), s.begin(),
        //[](unsigned char c){ return std::tolower(c); });
}

FatTreeTopology* FatTreeTopology::load(istream& file, QueueLoggerFactory* logger_factory, EventList& eventlist, mem_b queuesize, queue_type q_type, queue_type sender_q_type){
    //cout << "topo load start\n";
    std::string line;
    int linecount = 0;
    int no_of_nodes = 0;
    _tiers = 0;
    _hosts_per_pod = 0;
    for (int tier = 0; tier < 3; tier++) {
        _queue_down[tier] = queuesize;
        if (tier != 2)
            _queue_up[tier] = queuesize;
    }
    while (std::getline(file, line)) {
        linecount++;
        vector<string> tokens;
        tokenize(line, ' ', tokens);
        if (tokens.size() == 0)
            continue;
        if (tokens[0][0] == '#') {
            continue;
        }
        to_lower(tokens[0]);
        if (tokens[0] == "nodes") {
            no_of_nodes = stoi(tokens[1]);
        } else if (tokens[0] == "tiers") {
            _tiers = stoi(tokens[1]);
        } else if (tokens[0] == "podsize") {
            _hosts_per_pod = stoi(tokens[1]);
        } else if (tokens[0] == "tier") {
            // we're done with the header
            break;
        }
    }
    if (no_of_nodes == 0) {
        cerr << "Missing number of nodes in header" << endl;
        exit(1);
    }
    if (_tiers == 0) {
        cerr << "Missing number of tiers in header" << endl;
        exit(1);
    }
    if (_tiers < 2 || _tiers > 3) {
        cerr << "Invalid number of tiers: " << _tiers << endl;
        exit(1);
    }
    if (_hosts_per_pod == 0) {
        cerr << "Missing pod size in header" << endl;
        exit(1);
    }
    linecount--;
    bool tiers_done[3] = {false, false, false};
    int current_tier = -1;
    do {
        linecount++;
        vector<string> tokens;
	tokenize(line, ' ', tokens);
        if (tokens.size() < 1) {
            continue;
    	}
        to_lower(tokens[0]);
        if (tokens.size() == 0 || tokens[0][0] == '#') {
            continue;
        } else if (tokens[0] == "tier") {
            current_tier = stoi(tokens[1]);
            if (current_tier < 0 || current_tier > 2) {
                cerr << "Invalid tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            tiers_done[current_tier] = true;
        } else if (tokens[0] == "downlink_speed_gbps") {
            if (_downlink_speeds[current_tier] != 0) {
                cerr << "Duplicate linkspeed setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            _downlink_speeds[current_tier] = ((linkspeed_bps)stoi(tokens[1])) * 1000000000;
        } else if (tokens[0] == "radix_up") {
            if (_radix_up[current_tier] != 0) {
                cerr << "Duplicate radix_up setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            if (current_tier == 2) {
                cerr << "Can't specific radix_up for tier " << current_tier << " at line " << linecount << " (no uplinks from top tier!)" << endl;
                exit(1);
            }
            _radix_up[current_tier] = stoi(tokens[1]);
        } else if (tokens[0] == "radix_down") {
            if (_radix_down[current_tier] != 0) {
                cerr << "Duplicate radix_down setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            _radix_down[current_tier] = stoi(tokens[1]);
        } else if (tokens[0] == "queue_up") {
            if (_queue_up[current_tier] != 0) {
                cerr << "Duplicate queue_up setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            if (current_tier == 2) {
                cerr << "Can't specific queue_up for tier " << current_tier << " at line " << linecount << " (no uplinks from top tier!)" << endl;
                exit(1);
            }
            _queue_up[current_tier] = stoi(tokens[1]);
        } else if (tokens[0] == "queue_down") {
            if (_queue_down[current_tier] != 0) {
                cerr << "Duplicate queue_down setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            _queue_down[current_tier] = stoi(tokens[1]);
        } else if (tokens[0] == "oversubscribed") {
            if (_oversub[current_tier] != 1) {
                cerr << "Duplicate oversubscribed setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            _oversub[current_tier] = stoi(tokens[1]); 
        } else if (tokens[0] == "bundle") {
            if (_bundlesize[current_tier] != 1) {
                cerr << "Duplicate bundle size setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            _bundlesize[current_tier] = stoi(tokens[1]); 
        } else if (tokens[0] == "switch_latency_ns") {
            if (_switch_latencies[current_tier] != 0) {
                cerr << "Duplicate switch_latency setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            _switch_latencies[current_tier] = timeFromNs(stoi(tokens[1])); 
        } else if (tokens[0] == "downlink_latency_ns") {
            if (_link_latencies[current_tier] != 0) {
                cerr << "Duplicate link latency setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            _link_latencies[current_tier] = timeFromNs(stoi(tokens[1])); 
        }
    } while (std::getline(file, line));
    for (uint32_t tier = 0; tier < _tiers; tier++) {
        if (tiers_done[tier] == false) {
            cerr << "No configuration found for tier " << tier << endl;
            exit(1);
        }
        if (_downlink_speeds[tier] == 0) {
            cerr << "Missing downlink_speed_gbps for tier " << tier << endl;
            exit(1);
        }
        if (_link_latencies[tier] == 0) {
            cerr << "Missing downlink_latency_ns for tier " << tier << endl;
            exit(1);
        }
        if (tier < (_tiers - 1) && _radix_up[tier] == 0) {
            cerr << "Missing radix_up for tier " << tier << endl;
            exit(1);
        }
        if (_radix_down[tier] == 0) {
            cerr << "Missing radix_down for tier " << tier << endl;
            exit(1);
        }
        if (tier < (_tiers - 1) && _queue_up[tier] == 0) {
            cerr << "Missing queue_up for tier " << tier << endl;
            exit(1);
        }
        if (_queue_down[tier] == 0) {
            cerr << "Missing queue_down for tier " << tier << endl;
            exit(1);
        }
    }

    cout << "Topology load done\n";
    FatTreeTopology* ft = new FatTreeTopology(no_of_nodes, 0, 0, logger_factory, &eventlist, NULL, q_type, 0, 0, sender_q_type);
    cout << "FatTree constructor done, " << ft->no_of_nodes() << " nodes created\n";
    return ft;
}

FatTreeTopology::FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
                                 QueueLoggerFactory* logger_factory,
                                 EventList* ev,FirstFit * fit,queue_type q, simtime_picosec latency, simtime_picosec switch_latency, queue_type snd){
    
    set_linkspeeds(linkspeed);
    set_queue_sizes(queuesize);
    _logger_factory = logger_factory;
    _eventlist = ev;
    ff = fit;
    _qt = q;
    _sender_qt = snd;
    failed_links = 0;
    if ((latency != 0 || switch_latency != 0) && _link_latencies[TOR_TIER] != 0) {
        cerr << "Don't set latencies using both the constructor and set_latencies - use only one of the two\n";
        exit(1);
    }
    _hop_latency = latency;
    _switch_latency = switch_latency;

    if (_link_latencies[TOR_TIER] == 0) {
        cout << "Fat Tree topology with " << timeAsUs(_hop_latency) << "us links and " << timeAsUs(_switch_latency) <<"us switching latency." <<endl;
    } else {
        cout << "Fat Tree topology with "
             << timeAsUs(_link_latencies[TOR_TIER]) << "us Src-ToR links, "
             << timeAsUs(_link_latencies[AGG_TIER]) << "us ToR-Agg links, ";
        if (_tiers == 3) {
            cout << timeAsUs(_link_latencies[CORE_TIER]) << "us Agg-Core links, ";
        }
        cout << timeAsUs(_switch_latencies[TOR_TIER]) << "us ToR switch latency, "
             << timeAsUs(_switch_latencies[AGG_TIER]) << "us Agg switch latency";
        if (_tiers == 3) {
            cout << ", " << timeAsUs(_switch_latencies[CORE_TIER]) << "us Core switch latency." << endl;
        } else {
            cout << "." << endl;
        }
    }
    set_params(no_of_nodes);

    init_network();
}

FatTreeTopology::FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
                                 QueueLoggerFactory* logger_factory,
                                 EventList* ev,FirstFit * fit,queue_type q){
    set_linkspeeds(linkspeed);
    set_queue_sizes(queuesize);
    _logger_factory = logger_factory;
    _eventlist = ev;
    ff = fit;
    _qt = q;
    _sender_qt = FAIR_PRIO;
    failed_links = 0;
    if (_link_latencies[TOR_TIER] == 0) {
        _hop_latency = timeFromUs((uint32_t)1);
    } else {
        _hop_latency = timeFromUs((uint32_t)0); 
    }
    _switch_latency = timeFromUs((uint32_t)0); 
 
    cout << "Fat tree topology (1) with " << no_of_nodes << " nodes" << endl;
    set_params(no_of_nodes);

    init_network();
}

FatTreeTopology::FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
                                 QueueLoggerFactory* logger_factory,
                                 EventList* ev,FirstFit * fit, queue_type q, uint32_t num_failed){
    set_linkspeeds(linkspeed);
    set_queue_sizes(queuesize);
    if (_link_latencies[TOR_TIER] == 0) {
        _hop_latency = timeFromUs((uint32_t)1);
    } else {
        _hop_latency = timeFromUs((uint32_t)0); 
    }
    _switch_latency = timeFromUs((uint32_t)0); 
    _logger_factory = logger_factory;
    _qt = q;
    _sender_qt = FAIR_PRIO;

    _eventlist = ev;
    ff = fit;

    failed_links = num_failed;
  
    cout << "Fat tree topology (2) with " << no_of_nodes << " nodes" << endl;
    set_params(no_of_nodes);

    init_network();
}

FatTreeTopology::FatTreeTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
                                 QueueLoggerFactory* logger_factory,
                                 EventList* ev,FirstFit * fit, queue_type qtype,
                                 queue_type sender_qtype, uint32_t num_failed){
    set_linkspeeds(linkspeed);
    set_queue_sizes(queuesize);
    if (_link_latencies[TOR_TIER] == 0) {
        _hop_latency = timeFromUs((uint32_t)1);
    } else {
        _hop_latency = timeFromUs((uint32_t)0); 
    }
    _switch_latency = timeFromUs((uint32_t)0); 
    _logger_factory = logger_factory;
    _qt = qtype;
    _sender_qt = sender_qtype;

    _eventlist = ev;
    ff = fit;

    failed_links = num_failed;

    cout << "Fat tree topology (3) with " << no_of_nodes << " nodes" << endl;
    set_params(no_of_nodes);

    init_network();
}

void FatTreeTopology::set_linkspeeds(linkspeed_bps linkspeed) {
    if (linkspeed != 0 && _downlink_speeds[TOR_TIER] != 0) {
        cerr << "Don't set linkspeeds using both the constructor and set_tier_parameters - use only one of the two\n";
        exit(1);
    }
    if (linkspeed == 0 && _downlink_speeds[TOR_TIER] == 0) {
        cerr << "Linkspeed is not set, either as a default or by constructor\n";
        exit(1);
    }
    // set tier linkspeeds if no defaults are specified
    if (_downlink_speeds[TOR_TIER] == 0) {_downlink_speeds[TOR_TIER] = linkspeed;}
    if (_downlink_speeds[AGG_TIER] == 0) {_downlink_speeds[AGG_TIER] = linkspeed;}
    if (_downlink_speeds[CORE_TIER] == 0) {_downlink_speeds[CORE_TIER] = linkspeed;}
}

void FatTreeTopology::set_queue_sizes(mem_b queuesize) {
    if (queuesize != 0) {
        // all tiers use the same queuesize
        for (int tier = TOR_TIER; tier <= CORE_TIER; tier++) {
            _queue_down[tier] = queuesize;
            if (tier != CORE_TIER)
                _queue_up[tier] = queuesize;
        }
    } else {
        // the tier queue sizes must have already been set
        assert(_queue_down[TOR_TIER] != 0);
    }
}

void FatTreeTopology::set_custom_params(uint32_t no_of_nodes) {
    //cout << "set_custom_params" << endl;
    // do some sanity checking before we proceed
    assert(_hosts_per_pod > 0);

    // check bundlesizes are feasible with switch radix
    for (uint32_t tier = TOR_TIER; tier < _tiers; tier++) {
        if (_radix_down[tier] == 0) {
            cerr << "Custom topology, but radix_down not set for tier " << tier << endl;
            exit(1);
        }
        if (_radix_down[tier] % _bundlesize[tier] != 0) {
            cerr << "Mismatch between tier " << tier << " down radix of " << _radix_down[tier] << " and bundlesize " << _bundlesize[tier] << "\n";
            cerr << "Radix must be a multiple of bundlesize\n";
            exit(1);
        }
        if (tier < (_tiers - 1) && _radix_up[tier] == 0) {
            cerr << "Custom topology, but radix_up not set for tier " << tier << endl;
            exit(1);
        }
        if (tier < (_tiers - 1) && _radix_up[tier] % _bundlesize[tier+1] != 0) {
            cerr << "Mismatch between tier " << tier << " up radix of " << _radix_up[tier] << " and tier " << tier+1 << " down bundlesize " << _bundlesize[tier+1] << "\n";
            cerr << "Radix must be a multiple of bundlesize\n";
            exit(1);
        }
    }

    int no_of_pods = 0;
    _no_of_nodes = no_of_nodes;
    _tor_switches_per_pod = 0;
    _agg_switches_per_pod = 0;
    int no_of_tor_uplinks = 0;
    int no_of_agg_uplinks = 0;
    int no_of_core_switches = 0;
    if (no_of_nodes % _hosts_per_pod != 0) {
        cerr << "No_of_nodes is not a multiple of hosts_per_pod\n";
        exit(1);
    }

    no_of_pods = no_of_nodes / _hosts_per_pod; // we don't allow multi-port hosts yet
    assert(_bundlesize[TOR_TIER] == 1);
    if (_hosts_per_pod % _radix_down[TOR_TIER] != 0) {
        cerr << "Mismatch between TOR radix " << _radix_down[TOR_TIER] << " and podsize " << _hosts_per_pod << endl;
        exit(1);
    }
    _tor_switches_per_pod = _hosts_per_pod / _radix_down[TOR_TIER];

    assert((no_of_nodes * _downlink_speeds[TOR_TIER]) % (_downlink_speeds[AGG_TIER] * _oversub[TOR_TIER]) == 0);
    no_of_tor_uplinks = (no_of_nodes * _downlink_speeds[TOR_TIER]) / (_downlink_speeds[AGG_TIER] *  _oversub[TOR_TIER]);
    cout << "no_of_tor_uplinks: " << no_of_tor_uplinks << endl;

    if (_radix_down[TOR_TIER]/_radix_up[TOR_TIER] != _oversub[TOR_TIER]) {
        cerr << "Mismatch between TOR linkspeeds (" << speedAsGbps(_downlink_speeds[TOR_TIER]) << "Gbps down, "
             << speedAsGbps(_downlink_speeds[AGG_TIER]) << "Gbps up) and TOR radix (" << _radix_down[TOR_TIER] << " down, "
             << _radix_up[TOR_TIER] << " up) and oversubscription ratio of " << _oversub[TOR_TIER] << endl;
        exit(1);
    }

    assert(no_of_tor_uplinks % (no_of_pods * _radix_down[AGG_TIER]) == 0);
    _agg_switches_per_pod = no_of_tor_uplinks / (no_of_pods * _radix_down[AGG_TIER]);
    if (_agg_switches_per_pod * _bundlesize[AGG_TIER] != _radix_up[TOR_TIER]) {
        cerr << "Mismatch between TOR up radix " << _radix_up[TOR_TIER] << " and " << _agg_switches_per_pod
             << " aggregation switches per pod required by " << no_of_tor_uplinks << " TOR uplinks in "
             << no_of_pods << " pods " << " with an aggregation switch down radix of " << _radix_down[AGG_TIER] << endl;
        if (_bundlesize[AGG_TIER] == 1 && _radix_up[TOR_TIER] % _agg_switches_per_pod  == 0 && _radix_up[TOR_TIER]/_agg_switches_per_pod > 1) {
            cerr << "Did you miss specifying a Tier 1 bundle size of " << _radix_up[TOR_TIER]/_agg_switches_per_pod << "?" << endl;
        } else if (_radix_up[TOR_TIER] % _agg_switches_per_pod  == 0
                   && _radix_up[TOR_TIER]/_agg_switches_per_pod != _bundlesize[AGG_TIER]) {
            cerr << "Tier 1 bundle size is " << _bundlesize[AGG_TIER] << ". Did you mean it to be "
                 << _radix_up[TOR_TIER]/_agg_switches_per_pod << "?" << endl;
        }
        exit(1);
    }

    if (_tiers == 3) {
        assert((no_of_tor_uplinks * _downlink_speeds[AGG_TIER]) % (_downlink_speeds[CORE_TIER] * _oversub[AGG_TIER]) == 0);
        no_of_agg_uplinks = (no_of_tor_uplinks * _downlink_speeds[AGG_TIER]) / (_downlink_speeds[CORE_TIER] * _oversub[AGG_TIER]);
        cout << "no_of_agg_uplinks: " << no_of_agg_uplinks << endl;

        assert(no_of_agg_uplinks % _radix_down[CORE_TIER] == 0);
        no_of_core_switches = no_of_agg_uplinks / _radix_down[CORE_TIER];

        if (no_of_core_switches % _agg_switches_per_pod != 0) {
            cerr << "Topology results in " << no_of_core_switches << " core switches, which isn't an integer multiple of "
                 << _agg_switches_per_pod << " aggregation switches per pod, computed from Tier 0 and 1 values\n";
            exit(1);
        }

        if ((no_of_core_switches * _bundlesize[CORE_TIER])/ _agg_switches_per_pod  != _radix_up[AGG_TIER]) {
            cerr << "Mismatch between the AGG switch up-radix of " << _radix_up[AGG_TIER] << " and calculated "
                 << _agg_switches_per_pod << " aggregation switched per pod with " << no_of_core_switches << " core switches" << endl;
            if (_bundlesize[CORE_TIER] == 1
                && _radix_up[AGG_TIER] % (no_of_core_switches/_agg_switches_per_pod) == 0
                && _radix_up[AGG_TIER] / (no_of_core_switches/_agg_switches_per_pod) > 1) {
                cerr << "Did you miss specifying a Tier 2 bundle size of "
                     << _radix_up[AGG_TIER] / (no_of_core_switches/_agg_switches_per_pod) << "?" << endl;
            } else if (_radix_up[AGG_TIER] % (no_of_core_switches/_agg_switches_per_pod) == 0
                       && _radix_up[AGG_TIER] / (no_of_core_switches/_agg_switches_per_pod) != _bundlesize[CORE_TIER]) {
                cerr << "Tier 2 bundle size is " << _bundlesize[CORE_TIER] << ". Did you mean it to be "
                     << _radix_up[AGG_TIER] /	(no_of_core_switches/_agg_switches_per_pod) << "?" << endl;
            }
            exit(1);
        }
    }

    cout << "No of nodes: " << no_of_nodes << endl;
    cout << "No of pods: " << no_of_pods << endl;
    cout << "Hosts per pod: " << _hosts_per_pod << endl;
    cout << "Hosts per pod: " << _hosts_per_pod << endl;
    cout << "ToR switches per pod: " << _tor_switches_per_pod << endl;
    cout << "Agg switches per pod: " << _agg_switches_per_pod << endl;
    cout << "No of core switches: " << no_of_core_switches << endl;
    for (uint32_t tier = TOR_TIER; tier < _tiers; tier++) {
        cout << "Tier " << tier << " QueueSize Down " << _queue_down[tier] << " bytes" << endl;
        if (tier < CORE_TIER)
            cout << "Tier " << tier << " QueueSize Up " << _queue_up[tier] << " bytes" << endl;
    }

    // looks like we're OK, lets build it
    NSRV = no_of_nodes;
    NTOR = _tor_switches_per_pod * no_of_pods;
    NAGG = _agg_switches_per_pod * no_of_pods;
    NPOD = no_of_pods;
    NCORE = no_of_core_switches;
    alloc_vectors();
}


void FatTreeTopology::set_params(uint32_t no_of_nodes) {
    if (_hosts_per_pod > 0) {
        // if we've set all the detailed parameters, we'll use them, otherwise fall through to defaults
        set_custom_params(no_of_nodes);
        return;
    }
    
    cout << "Set params " << no_of_nodes << endl;
    for (int tier = TOR_TIER; tier <= CORE_TIER; tier++) {
        cout << "Tier " << tier << " QueueSize Down " << _queue_down[tier] << " bytes" << endl;
        if (tier < CORE_TIER)
            cout << "Tier " << tier << " QueueSize Up " << _queue_up[tier] << " bytes" << endl;
    }
    _no_of_nodes = 0;
    int K = 0;
    if (_tiers == 3) {
        while (_no_of_nodes < no_of_nodes) {
            K++;
            _no_of_nodes = K * K * K /4;
        }
        if (_no_of_nodes > no_of_nodes) {
            cerr << "Topology Error: can't have a 3-Tier FatTree with " << no_of_nodes
                 << " nodes\n";
            exit(1);
        }
        int NK = (K*K/2);
        NSRV = (K*K*K/4);
        NTOR = NK;
        NAGG = NK;
        NPOD = K;
        NCORE = (K*K/4);
    } else if (_tiers == 2) {
        // We want a leaf-spine topology
        while (_no_of_nodes < no_of_nodes) {
            K++;
            _no_of_nodes = K * K /2;
        }
        if (_no_of_nodes > no_of_nodes) {
            cerr << "Topology Error: can't have a 2-Tier FatTree with " << no_of_nodes
                 << " nodes\n";
            exit(1);
        }
        int NK = K;
        NSRV = K * K /2;
        NTOR = NK;
        NAGG = NK/2;
        NPOD = 1;
        NCORE = 0;
    } else {
        cerr << "Topology Error: " << _tiers << " tier FatTree not supported\n";
        exit(1);
    }
    
    
    cout << "_no_of_nodes " << _no_of_nodes << endl;
    cout << "K " << K << endl;
    cout << "Queue type " << _qt << endl;

    // if these are set, we should be in the custom code, not here
    assert(_radix_down[TOR_TIER] == 0); 
    assert(_radix_up[TOR_TIER] == 0);
    
    _radix_down[TOR_TIER] = K/2;
    _radix_up[TOR_TIER] = K/2;
    _radix_down[AGG_TIER] = K/2;
    _radix_up[AGG_TIER] = K/2;
    _radix_down[CORE_TIER] = K;
    assert(_hosts_per_pod == 0);
    _tor_switches_per_pod = K/2;
    _agg_switches_per_pod = K/2;
    _hosts_per_pod = _no_of_nodes / NPOD;

    alloc_vectors();
}

void FatTreeTopology::alloc_vectors() {

    switches_lp.resize(NTOR,NULL);
    switches_up.resize(NAGG,NULL);
    switches_c.resize(NCORE,NULL);


    // These vectors are sparse - we won't use all the entries
    if (_tiers == 3) {
        // resizing 3d vectors is scary magic
        pipes_nc_nup.resize(NCORE, vector< vector<Pipe*> >(NAGG, vector<Pipe*>(_bundlesize[CORE_TIER])));
        queues_nc_nup.resize(NCORE, vector< vector<BaseQueue*> >(NAGG, vector<BaseQueue*>(_bundlesize[CORE_TIER])));
    }

    pipes_nup_nlp.resize(NAGG, vector< vector<Pipe*> >(NTOR, vector<Pipe*>(_bundlesize[AGG_TIER])));
    queues_nup_nlp.resize(NAGG, vector< vector<BaseQueue*> >(NTOR, vector<BaseQueue*>(_bundlesize[AGG_TIER])));

    pipes_nlp_ns.resize(NTOR, vector< vector<Pipe*> >(NSRV, vector<Pipe*>(_bundlesize[TOR_TIER])));
    queues_nlp_ns.resize(NTOR, vector< vector<BaseQueue*> >(NSRV, vector<BaseQueue*>(_bundlesize[TOR_TIER])));


    if (_tiers == 3) {
        pipes_nup_nc.resize(NAGG, vector< vector<Pipe*> >(NCORE, vector<Pipe*>(_bundlesize[CORE_TIER])));
        queues_nup_nc.resize(NAGG, vector< vector<BaseQueue*> >(NCORE, vector<BaseQueue*>(_bundlesize[CORE_TIER])));
    }
    
    pipes_nlp_nup.resize(NTOR, vector< vector<Pipe*> >(NAGG, vector<Pipe*>(_bundlesize[AGG_TIER])));
    pipes_ns_nlp.resize(NSRV, vector< vector<Pipe*> >(NTOR, vector<Pipe*>(_bundlesize[TOR_TIER])));
    queues_nlp_nup.resize(NTOR, vector< vector<BaseQueue*> >(NAGG, vector<BaseQueue*>(_bundlesize[AGG_TIER])));
    queues_ns_nlp.resize(NSRV, vector< vector<BaseQueue*> >(NTOR, vector<BaseQueue*>(_bundlesize[TOR_TIER])));
}

BaseQueue* FatTreeTopology::alloc_src_queue(QueueLogger* queueLogger){
    linkspeed_bps linkspeed = _downlink_speeds[TOR_TIER]; // linkspeeds are symmetric
    switch (_sender_qt) {
    case SWIFT_SCHEDULER:
        return new FairScheduler(linkspeed, *_eventlist, queueLogger);
    case PRIORITY:
        return new PriorityQueue(linkspeed,
                                 memFromPkt(FEEDER_BUFFER), *_eventlist, queueLogger);
    case FAIR_PRIO:
        return new FairPriorityQueue(linkspeed,
                                     memFromPkt(FEEDER_BUFFER), *_eventlist, queueLogger);
    default:
        abort();
    }
}

BaseQueue* FatTreeTopology::alloc_queue(QueueLogger* queueLogger, mem_b queuesize,
                                        link_direction dir, int switch_tier, bool tor = false){
    if (dir == UPLINK) {
        switch_tier++; // _downlink_speeds is set for the downlinks, so uplinks need to use the tier above's linkspeed
    }
    return alloc_queue(queueLogger, _downlink_speeds[switch_tier], queuesize, dir, switch_tier, tor);
}

BaseQueue*
FatTreeTopology::alloc_queue(QueueLogger* queueLogger, linkspeed_bps speed, mem_b queuesize,
                             link_direction dir, int switch_tier, bool tor){
    switch (_qt) {
    case RANDOM:
        return new RandomQueue(speed, queuesize, *_eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
    case COMPOSITE:
        return new CompositeQueue(speed, queuesize, *_eventlist, queueLogger);
    case CTRL_PRIO:
        return new CtrlPrioQueue(speed, queuesize, *_eventlist, queueLogger);
    case AEOLUS:
        return new AeolusQueue(speed, queuesize, FatTreeSwitch::_speculative_threshold_fraction * queuesize,  *_eventlist, queueLogger);
    case AEOLUS_ECN:
        {
            AeolusQueue* q = new AeolusQueue(speed, queuesize, FatTreeSwitch::_speculative_threshold_fraction * queuesize ,  *_eventlist, queueLogger);
            if (!tor || dir == UPLINK) {
                // don't use ECN on ToR downlinks
                q->set_ecn_threshold(FatTreeSwitch::_ecn_threshold_fraction * queuesize);
            }
            return q;
        }
    case ECN:
        return new ECNQueue(speed, queuesize, *_eventlist, queueLogger, memFromPkt(15));
    case ECN_PRIO:
        return new ECNPrioQueue(speed, queuesize, queuesize,
                                FatTreeSwitch::_ecn_threshold_fraction * queuesize,
                                FatTreeSwitch::_ecn_threshold_fraction * queuesize,
                                *_eventlist, queueLogger);
    case LOSSLESS:
        return new LosslessQueue(speed, queuesize, *_eventlist, queueLogger, NULL);
    case LOSSLESS_INPUT:
        return new LosslessOutputQueue(speed, queuesize, *_eventlist, queueLogger);
    case LOSSLESS_INPUT_ECN: 
        return new LosslessOutputQueue(speed, memFromPkt(10000), *_eventlist, queueLogger,1,memFromPkt(16));
    case COMPOSITE_ECN:
        if (tor && dir == DOWNLINK) 
            return new CompositeQueue(speed, queuesize, *_eventlist, queueLogger);
        else
            return new ECNQueue(speed, memFromPkt(2*SWITCH_BUFFER), *_eventlist, queueLogger, memFromPkt(15));
    case COMPOSITE_ECN_LB:
        {
            CompositeQueue* q = new CompositeQueue(speed, queuesize, *_eventlist, queueLogger);
            if (!tor || dir == UPLINK) {
                // don't use ECN on ToR downlinks
                q->set_ecn_threshold(FatTreeSwitch::_ecn_threshold_fraction * queuesize);
            }
            return q;
        }
    default:
        abort();
    }
}

void FatTreeTopology::init_network(){
    QueueLogger* queueLogger;
    if (_tiers == 3) {
        for (uint32_t j=0;j<NCORE;j++) {
            for (uint32_t k=0;k<NAGG;k++) {
                for (uint32_t b = 0; b < _bundlesize[CORE_TIER]; b++) {
                    queues_nc_nup[j][k][b] = NULL;
                    pipes_nc_nup[j][k][b] = NULL;
                    queues_nup_nc[k][j][b] = NULL;
                    pipes_nup_nc[k][j][b] = NULL;
                }
            }
        }
    }
    
    for (uint32_t j=0;j<NAGG;j++) {
        for (uint32_t k=0;k<NTOR;k++) {
            for (uint32_t b = 0; b < _bundlesize[AGG_TIER]; b++) {
                queues_nup_nlp[j][k][b] = NULL;
                pipes_nup_nlp[j][k][b] = NULL;
                queues_nlp_nup[k][j][b] = NULL;
                pipes_nlp_nup[k][j][b] = NULL;
            }
        }
    }
    
    for (uint32_t j=0;j<NTOR;j++) {
        for (uint32_t k=0;k<NSRV;k++) {
            for (uint32_t b = 0; b < _bundlesize[TOR_TIER]; b++) { 
                queues_nlp_ns[j][k][b] = NULL;
                pipes_nlp_ns[j][k][b] = NULL;
                queues_ns_nlp[k][j][b] = NULL;
                pipes_ns_nlp[k][j][b] = NULL;
            }
        }
    }

    //create switches if we have lossless operation
    //if (_qt==LOSSLESS)
    // changed to always create switches
    for (uint32_t j=0;j<NTOR;j++){
        simtime_picosec switch_latency = (_switch_latencies[TOR_TIER] > 0) ? _switch_latencies[TOR_TIER] : _switch_latency;
        switches_lp[j] = new FatTreeSwitch(*_eventlist, "Switch_LowerPod_"+ntoa(j),FatTreeSwitch::TOR,j,switch_latency,this);
    }
    for (uint32_t j=0;j<NAGG;j++){
        simtime_picosec switch_latency = (_switch_latencies[AGG_TIER] > 0) ? _switch_latencies[AGG_TIER] : _switch_latency;
        switches_up[j] = new FatTreeSwitch(*_eventlist, "Switch_UpperPod_"+ntoa(j), FatTreeSwitch::AGG,j,switch_latency,this);
    }
    for (uint32_t j=0;j<NCORE;j++){
        simtime_picosec switch_latency = (_switch_latencies[CORE_TIER] > 0) ? _switch_latencies[CORE_TIER] : _switch_latency;
        switches_c[j] = new FatTreeSwitch(*_eventlist, "Switch_Core_"+ntoa(j), FatTreeSwitch::CORE,j,switch_latency,this);
    }
      
    // links from lower layer pod switch to server
    for (uint32_t tor = 0; tor < NTOR; tor++) {
        uint32_t link_bundles = _radix_down[TOR_TIER]/_bundlesize[TOR_TIER];
        for (uint32_t l = 0; l < link_bundles; l++) {
            uint32_t srv = tor * link_bundles + l;
            for (uint32_t b = 0; b < _bundlesize[TOR_TIER]; b++) {
                // Downlink
                if (_logger_factory) {
                    queueLogger = _logger_factory->createQueueLogger();
                } else {
                    queueLogger = NULL;
                }
            
                queues_nlp_ns[tor][srv][b] = alloc_queue(queueLogger, _queue_down[TOR_TIER], DOWNLINK, TOR_TIER, true);
                queues_nlp_ns[tor][srv][b]->setName("LS" + ntoa(tor) + "->DST" +ntoa(srv) + "(" + ntoa(b) + ")");
                //if (logfile) logfile->writeName(*(queues_nlp_ns[tor][srv]));
                simtime_picosec hop_latency = (_hop_latency == 0) ? _link_latencies[TOR_TIER] : _hop_latency;
                pipes_nlp_ns[tor][srv][b] = new Pipe(hop_latency, *_eventlist);
                pipes_nlp_ns[tor][srv][b]->setName("Pipe-LS" + ntoa(tor)  + "->DST" + ntoa(srv) + "(" + ntoa(b) + ")");
                //if (logfile) logfile->writeName(*(pipes_nlp_ns[tor][srv]));
            
                // Uplink
                if (_logger_factory) {
                    queueLogger = _logger_factory->createQueueLogger();
                } else {
                    queueLogger = NULL;
                }
                queues_ns_nlp[srv][tor][b] = alloc_src_queue(queueLogger);   
                queues_ns_nlp[srv][tor][b]->setName("SRC" + ntoa(srv) + "->LS" +ntoa(tor) + "(" + ntoa(b) + ")");
                //cout << queues_ns_nlp[srv][tor][b]->str() << endl;
                //if (logfile) logfile->writeName(*(queues_ns_nlp[srv][tor]));

                queues_ns_nlp[srv][tor][b]->setRemoteEndpoint(switches_lp[tor]);

                assert(switches_lp[tor]->addPort(queues_nlp_ns[tor][srv][b]) < 96);

                if (_qt==LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN){
                    //no virtual queue needed at server
                    new LosslessInputQueue(*_eventlist, queues_ns_nlp[srv][tor][b], switches_lp[tor], _hop_latency);
                }
        
                pipes_ns_nlp[srv][tor][b] = new Pipe(hop_latency, *_eventlist);
                pipes_ns_nlp[srv][tor][b]->setName("Pipe-SRC" + ntoa(srv) + "->LS" + ntoa(tor) + "(" + ntoa(b) + ")");
                //if (logfile) logfile->writeName(*(pipes_ns_nlp[srv][tor]));
            
                if (ff){
                    ff->add_queue(queues_nlp_ns[tor][srv][b]);
                    ff->add_queue(queues_ns_nlp[srv][tor][b]);
                }
            }
        }
    }

    //Lower layer in pod to upper layer in pod!
    for (uint32_t tor = 0; tor < NTOR; tor++) {
        uint32_t podid = tor/_tor_switches_per_pod;
        uint32_t agg_min, agg_max;
        if (_tiers == 3) {
            //Connect the lower layer switch to the upper layer switches in the same pod
            agg_min = MIN_POD_AGG_SWITCH(podid);
            agg_max = MAX_POD_AGG_SWITCH(podid);
        } else {
            //Connect the lower layer switch to all upper layer switches
            assert(_tiers == 2);
            agg_min = 0;
            agg_max = NAGG-1;
        }
        for (uint32_t agg=agg_min; agg<=agg_max; agg++){
            for (uint32_t b = 0; b < _bundlesize[AGG_TIER]; b++) {
                // Downlink
                if (_logger_factory) {
                    queueLogger = _logger_factory->createQueueLogger();
                } else {
                    queueLogger = NULL;
                }
                queues_nup_nlp[agg][tor][b] = alloc_queue(queueLogger, _queue_down[AGG_TIER], DOWNLINK, AGG_TIER);
                queues_nup_nlp[agg][tor][b]->setName("US" + ntoa(agg) + "->LS_" + ntoa(tor) + "(" + ntoa(b) + ")");
                //if (logfile) logfile->writeName(*(queues_nup_nlp[agg][tor]));
            
                simtime_picosec hop_latency = (_hop_latency == 0) ? _link_latencies[AGG_TIER] : _hop_latency;
                pipes_nup_nlp[agg][tor][b] = new Pipe(hop_latency, *_eventlist);
                pipes_nup_nlp[agg][tor][b]->setName("Pipe-US" + ntoa(agg) + "->LS" + ntoa(tor) + "(" + ntoa(b) + ")");
                //if (logfile) logfile->writeName(*(pipes_nup_nlp[agg][tor]));
            
                // Uplink
                if (_logger_factory) {
                    queueLogger = _logger_factory->createQueueLogger();
                } else {
                    queueLogger = NULL;
                }
                queues_nlp_nup[tor][agg][b] = alloc_queue(queueLogger, _queue_up[TOR_TIER], UPLINK, TOR_TIER, true);
                queues_nlp_nup[tor][agg][b]->setName("LS" + ntoa(tor) + "->US" + ntoa(agg) + "(" + ntoa(b) + ")");
                //cout << queues_nlp_nup[tor][agg][b]->str() << endl;
                //if (logfile) logfile->writeName(*(queues_nlp_nup[tor][agg]));

                assert(switches_lp[tor]->addPort(queues_nlp_nup[tor][agg][b]) < 96);
                assert(switches_up[agg]->addPort(queues_nup_nlp[agg][tor][b]) < 64);
                queues_nlp_nup[tor][agg][b]->setRemoteEndpoint(switches_up[agg]);
                queues_nup_nlp[agg][tor][b]->setRemoteEndpoint(switches_lp[tor]);

                /*if (_qt==LOSSLESS){
                  ((LosslessQueue*)queues_nlp_nup[tor][agg])->setRemoteEndpoint(queues_nup_nlp[agg][tor]);
                  ((LosslessQueue*)queues_nup_nlp[agg][tor])->setRemoteEndpoint(queues_nlp_nup[tor][agg]);
                  }else */
                if (_qt==LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN){            
                    new LosslessInputQueue(*_eventlist, queues_nlp_nup[tor][agg][b],switches_up[agg],_hop_latency);
                    new LosslessInputQueue(*_eventlist, queues_nup_nlp[agg][tor][b],switches_lp[tor],_hop_latency);
                }
        
                pipes_nlp_nup[tor][agg][b] = new Pipe(hop_latency, *_eventlist);
                pipes_nlp_nup[tor][agg][b]->setName("Pipe-LS" + ntoa(tor) + "->US" + ntoa(agg) + "(" + ntoa(b) + ")");
                //if (logfile) logfile->writeName(*(pipes_nlp_nup[tor][agg]));
        
                if (ff){
                    ff->add_queue(queues_nlp_nup[tor][agg][b]);
                    ff->add_queue(queues_nup_nlp[agg][tor][b]);
                }
            }
        }
    }

    /*for (int32_t i = 0;i<NK;i++){
      for (uint32_t j = 0;j<NK;j++){
      printf("%p/%p ",queues_nlp_nup[i][j], queues_nup_nlp[j][i]);
      }
      printf("\n");
      }*/
    
    // Upper layer in pod to core
    if (_tiers == 3) {
        for (uint32_t agg = 0; agg < NAGG; agg++) {
            uint32_t podpos = agg%(_agg_switches_per_pod);
            for (uint32_t l = 0; l < _radix_up[AGG_TIER]/_bundlesize[CORE_TIER]; l++) {
                uint32_t core = podpos +  _agg_switches_per_pod * l;
                assert(core < NCORE);
                for (uint32_t b = 0; b < _bundlesize[CORE_TIER]; b++) {
                
                    // Downlink
                    if (_logger_factory) {
                        queueLogger = _logger_factory->createQueueLogger();
                    } else {
                        queueLogger = NULL;
                    }
                    assert(queues_nup_nc[agg][core][b] == NULL);
                    queues_nup_nc[agg][core][b] = alloc_queue(queueLogger, _queue_up[AGG_TIER], UPLINK, AGG_TIER);
                    queues_nup_nc[agg][core][b]->setName("US" + ntoa(agg) + "->CS" + ntoa(core) + "(" + ntoa(b) + ")");
                    //cout << queues_nup_nc[agg][core][b]->str() << endl;
                    //if (logfile) logfile->writeName(*(queues_nup_nc[agg][core]));
        
                    simtime_picosec hop_latency = (_hop_latency == 0) ? _link_latencies[CORE_TIER] : _hop_latency;
                    pipes_nup_nc[agg][core][b] = new Pipe(hop_latency, *_eventlist);
                    pipes_nup_nc[agg][core][b]->setName("Pipe-US" + ntoa(agg) + "->CS" + ntoa(core) + "(" + ntoa(b) + ")");
                    //if (logfile) logfile->writeName(*(pipes_nup_nc[agg][core]));
        
                    // Uplink
                    if (_logger_factory) {
                        queueLogger = _logger_factory->createQueueLogger();
                    } else {
                        queueLogger = NULL;
                    }
        
                    if ((l+agg*_agg_switches_per_pod)<failed_links){
                        queues_nc_nup[core][agg][b] = alloc_queue(queueLogger, _downlink_speeds[CORE_TIER]/10, _queue_down[CORE_TIER],
                                                               DOWNLINK, CORE_TIER, false);
                        cout << "Adding link failure for agg_sw " << ntoa(agg) << " l " << ntoa(l) << " b " << ntoa(b) << endl;
                    } else {
                        queues_nc_nup[core][agg][b] = alloc_queue(queueLogger, _queue_down[CORE_TIER], DOWNLINK, CORE_TIER);
                    }
        
                    queues_nc_nup[core][agg][b]->setName("CS" + ntoa(core) + "->US" + ntoa(agg) + "(" + ntoa(b) + ")");

                    assert(switches_up[agg]->addPort(queues_nup_nc[agg][core][b]) < 64);
                    assert(switches_c[core]->addPort(queues_nc_nup[core][agg][b]) < 64);
                    queues_nup_nc[agg][core][b]->setRemoteEndpoint(switches_c[core]);
                    queues_nc_nup[core][agg][b]->setRemoteEndpoint(switches_up[agg]);

                    /*if (_qt==LOSSLESS){
                      ((LosslessQueue*)queues_nup_nc[agg][core])->setRemoteEndpoint(queues_nc_nup[core][agg]);
                      ((LosslessQueue*)queues_nc_nup[core][agg])->setRemoteEndpoint(queues_nup_nc[agg][core]);
                      }
                      else*/
                    if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN){
                        new LosslessInputQueue(*_eventlist, queues_nup_nc[agg][core][b], switches_c[core], _hop_latency);
                        new LosslessInputQueue(*_eventlist, queues_nc_nup[core][agg][b], switches_up[agg], _hop_latency);
                    }
                    //if (logfile) logfile->writeName(*(queues_nc_nup[core][agg]));
            
                    pipes_nc_nup[core][agg][b] = new Pipe(hop_latency, *_eventlist);
                    pipes_nc_nup[core][agg][b]->setName("Pipe-CS" + ntoa(core) + "->US" + ntoa(agg) + "(" + ntoa(b) + ")");
                    //if (logfile) logfile->writeName(*(pipes_nc_nup[core][agg]));
            
                    if (ff){
                        ff->add_queue(queues_nup_nc[agg][core][b]);
                        ff->add_queue(queues_nc_nup[core][agg][b]);
                    }
                }
            }
        }
    }

    /*    for (uint32_t i = 0;i<NK;i++){
          for (uint32_t j = 0;j<NC;j++){
          printf("%p/%p ",queues_nup_nc[i][agg], queues_nc_nup[agg][i]);
          }
          printf("\n");
          }*/
    
    //init thresholds for lossless operation
    if (_qt==LOSSLESS) {
        for (uint32_t j=0;j<NTOR;j++){
            switches_lp[j]->configureLossless();
        }
        for (uint32_t j=0;j<NAGG;j++){
            switches_up[j]->configureLossless();
        }
        for (uint32_t j=0;j<NCORE;j++){
            switches_c[j]->configureLossless();
        }
    }
}

void FatTreeTopology::add_failed_link(uint32_t type, uint32_t switch_id, uint32_t link_id){
    assert(type == FatTreeSwitch::AGG);
    assert(link_id < _radix_up[AGG_TIER]);
    assert(switch_id < NAGG);
    
    uint32_t podpos = switch_id%(_agg_switches_per_pod);
    uint32_t k = podpos * _agg_switches_per_pod + link_id;

    // note: if bundlesize > 1, we only fail the first link in a bundle.
    
    assert(queues_nup_nc[switch_id][k][0]!=NULL && queues_nc_nup[k][switch_id][0]!=NULL );
    queues_nup_nc[switch_id][k][0] = NULL;
    queues_nc_nup[k][switch_id][0] = NULL;

    assert(pipes_nup_nc[switch_id][k][0]!=NULL && pipes_nc_nup[k][switch_id][0]);
    pipes_nup_nc[switch_id][k][0] = NULL;
    pipes_nc_nup[k][switch_id][0] = NULL;
}


vector<const Route*>* FatTreeTopology::get_bidir_paths(uint32_t src, uint32_t dest, bool reverse){
    vector<const Route*>* paths = new vector<const Route*>();

    route_t *routeout, *routeback;
  
    //QueueLoggerSimple *simplequeuelogger = new QueueLoggerSimple();
    //QueueLoggerSimple *simplequeuelogger = 0;
    //logfile->addLogger(*simplequeuelogger);
    //Queue* pqueue = new Queue(_linkspeed, memFromPkt(FEEDER_BUFFER), *_eventlist, simplequeuelogger);
    //pqueue->setName("PQueue_" + ntoa(src) + "_" + ntoa(dest));
    //logfile->writeName(*pqueue);
    if (HOST_POD_SWITCH(src)==HOST_POD_SWITCH(dest)){
  
        // forward path
        routeout = new Route();
        //routeout->push_back(pqueue);
        routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)][0]);
        routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)][0]);

        if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
            routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)][0]->getRemoteEndpoint());

        routeout->push_back(queues_nlp_ns[HOST_POD_SWITCH(dest)][dest][0]);
        routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest][0]);

        if (reverse) {
            // reverse path for RTS packets
            routeback = new Route();
            routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)][0]);
            routeback->push_back(pipes_ns_nlp[dest][HOST_POD_SWITCH(dest)][0]);

            if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)][0]->getRemoteEndpoint());

            routeback->push_back(queues_nlp_ns[HOST_POD_SWITCH(src)][src][0]);
            routeback->push_back(pipes_nlp_ns[HOST_POD_SWITCH(src)][src][0]);

            routeout->set_reverse(routeback);
            routeback->set_reverse(routeout);
        }

        //print_route(*routeout);
        paths->push_back(routeout);

        check_non_null(routeout);
        //cout << "pathcount " << paths->size() << endl;
        return paths;
    }
    else if (HOST_POD(src)==HOST_POD(dest)){
        //don't go up the hierarchy, stay in the pod only.

        uint32_t pod = HOST_POD(src);
        //there are K/2 paths between the source and the destination  <- this is no longer true for bundles
        if (_tiers == 2) {
            // xxx sanity check for debugging, remove later.
            assert(MIN_POD_AGG_SWITCH(pod) == 0);
            assert(MAX_POD_AGG_SWITCH(pod) == NAGG - 1);
        }
        for (uint32_t upper = MIN_POD_AGG_SWITCH(pod);upper <= MAX_POD_AGG_SWITCH(pod); upper++){
            for (uint32_t b_up = 0; b_up < _bundlesize[AGG_TIER]; b_up++) {
                for (uint32_t b_down = 0; b_down < _bundlesize[AGG_TIER]; b_down++) {
                    // b_up is link number in upgoing bundle, b_down is link number in downgoing bundle
                    // note: no bundling supported between host and tor - just use link number 0
                
                    //upper is nup
      
                    routeout = new Route();
      
                    routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)][0]);
                    routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)][0]);

                    if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                        routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)][0]->getRemoteEndpoint());

                    routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][upper][b_up]);
                    routeout->push_back(pipes_nlp_nup[HOST_POD_SWITCH(src)][upper][b_up]);

                    if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                        routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][upper][b_up]->getRemoteEndpoint());

                    routeout->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(dest)][b_down]);
                    routeout->push_back(pipes_nup_nlp[upper][HOST_POD_SWITCH(dest)][b_down]);

                    if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                        routeout->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(dest)][b_down]->getRemoteEndpoint());

                    routeout->push_back(queues_nlp_ns[HOST_POD_SWITCH(dest)][dest][0]);
                    routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest][0]);

                    if (reverse) {
                        // reverse path for RTS packets
                        routeback = new Route();
      
                        routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)][0]);
                        routeback->push_back(pipes_ns_nlp[dest][HOST_POD_SWITCH(dest)][0]);

                        if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                            routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)][0]->getRemoteEndpoint());

                        routeback->push_back(queues_nlp_nup[HOST_POD_SWITCH(dest)][upper][b_down]);
                        routeback->push_back(pipes_nlp_nup[HOST_POD_SWITCH(dest)][upper][b_down]);

                        if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                            routeback->push_back(queues_nlp_nup[HOST_POD_SWITCH(dest)][upper][b_down]->getRemoteEndpoint());

                        routeback->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(src)][b_up]);
                        routeback->push_back(pipes_nup_nlp[upper][HOST_POD_SWITCH(src)][b_up]);

                        if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                            routeback->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(src)][b_up]->getRemoteEndpoint());
      
                        routeback->push_back(queues_nlp_ns[HOST_POD_SWITCH(src)][src][0]);
                        routeback->push_back(pipes_nlp_ns[HOST_POD_SWITCH(src)][src][0]);

                        routeout->set_reverse(routeback);
                        routeback->set_reverse(routeout);
                    }
      
                    //print_route(*routeout);
                    paths->push_back(routeout);
                    check_non_null(routeout);
                }
            }
        }
        cout << "pathcount " << paths->size() << endl;
        return paths;
    } else {
        assert(_tiers == 3);
        uint32_t pod = HOST_POD(src);

        for (uint32_t upper = MIN_POD_AGG_SWITCH(pod); upper <= MAX_POD_AGG_SWITCH(pod); upper++) {
            uint32_t podpos = upper % _agg_switches_per_pod;

            for (uint32_t l = 0; l < _radix_up[AGG_TIER]/_bundlesize[CORE_TIER]; l++) {
                uint32_t core = podpos +  _agg_switches_per_pod * l;

                for (uint32_t b1_up = 0; b1_up < _bundlesize[AGG_TIER]; b1_up++) {
                    for (uint32_t b1_down = 0; b1_down < _bundlesize[AGG_TIER]; b1_down++) {
                        // b1_up is link number in upgoing bundle from tor to agg, b1_down is link number in downgoing bundle

                        for (uint32_t b2_up = 0; b2_up < _bundlesize[CORE_TIER]; b2_up++) {
                            for (uint32_t b2_down = 0; b2_down < _bundlesize[CORE_TIER]; b2_down++) {
                                // b2_up is link number in upgoing bundle from agg to core, b2_down is link number in downgoing bundle
                                // note: no bundling supported between host and tor - just use link number 0
                                //upper is nup
        
                                routeout = new Route();
                                //routeout->push_back(pqueue);
        
                                routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)][0]);
                                routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)][0]);

                                if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                                    routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)][0]->getRemoteEndpoint());
        
                                routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][upper][b1_up]);
                                routeout->push_back(pipes_nlp_nup[HOST_POD_SWITCH(src)][upper][b1_up]);

                                if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                                    routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][upper][b1_up]->getRemoteEndpoint());
        
                                routeout->push_back(queues_nup_nc[upper][core][b2_up]);
                                routeout->push_back(pipes_nup_nc[upper][core][b2_up]);

                                if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                                    routeout->push_back(queues_nup_nc[upper][core][b2_up]->getRemoteEndpoint());
        
                                //now take the only link down to the destination server!
        
                                uint32_t upper2 = MIN_POD_AGG_SWITCH(HOST_POD(dest)) + core % _agg_switches_per_pod;
                                //printf("K %d HOST_POD(%d) %d core %d upper2 %d\n",K,dest,HOST_POD(dest),core, upper2);
        
                                routeout->push_back(queues_nc_nup[core][upper2][b2_down]);
                                routeout->push_back(pipes_nc_nup[core][upper2][b2_down]);

                                if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                                    routeout->push_back(queues_nc_nup[core][upper2][b2_down]->getRemoteEndpoint());        

                                routeout->push_back(queues_nup_nlp[upper2][HOST_POD_SWITCH(dest)][b1_down]);
                                routeout->push_back(pipes_nup_nlp[upper2][HOST_POD_SWITCH(dest)][b1_down]);

                                if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                                    routeout->push_back(queues_nup_nlp[upper2][HOST_POD_SWITCH(dest)][b1_down]->getRemoteEndpoint());
        
                                routeout->push_back(queues_nlp_ns[HOST_POD_SWITCH(dest)][dest][0]);
                                routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest][0]);

                                if (reverse) {
                                    // reverse path for RTS packets
                                    routeback = new Route();
        
                                    routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)][0]);
                                    routeback->push_back(pipes_ns_nlp[dest][HOST_POD_SWITCH(dest)][0]);

                                    if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                                        routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)][0]->getRemoteEndpoint());
        
                                    routeback->push_back(queues_nlp_nup[HOST_POD_SWITCH(dest)][upper2][b1_down]);
                                    routeback->push_back(pipes_nlp_nup[HOST_POD_SWITCH(dest)][upper2][b1_down]);

                                    if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                                        routeback->push_back(queues_nlp_nup[HOST_POD_SWITCH(dest)][upper2][b1_down]->getRemoteEndpoint());
        
                                    routeback->push_back(queues_nup_nc[upper2][core][b2_down]);
                                    routeback->push_back(pipes_nup_nc[upper2][core][b2_down]);

                                    if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                                        routeback->push_back(queues_nup_nc[upper2][core][b2_down]->getRemoteEndpoint());
        
                                    //now take the only link back down to the src server!
        
                                    routeback->push_back(queues_nc_nup[core][upper][b2_up]);
                                    routeback->push_back(pipes_nc_nup[core][upper][b2_up]);

                                    if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                                        routeback->push_back(queues_nc_nup[core][upper][b2_up]->getRemoteEndpoint());
        
                                    routeback->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(src)][b1_up]);
                                    routeback->push_back(pipes_nup_nlp[upper][HOST_POD_SWITCH(src)][b1_up]);

                                    if (_qt==LOSSLESS_INPUT || _qt==LOSSLESS_INPUT_ECN)
                                        routeback->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(src)][b1_up]->getRemoteEndpoint());
        
                                    routeback->push_back(queues_nlp_ns[HOST_POD_SWITCH(src)][src][0]);
                                    routeback->push_back(pipes_nlp_ns[HOST_POD_SWITCH(src)][src][0]);


                                    routeout->set_reverse(routeback);
                                    routeback->set_reverse(routeout);
                                }
        
                                //print_route(*routeout);
                                paths->push_back(routeout);
                                check_non_null(routeout);
                            }
                        }
                    }
                }
            }
        }
        cout << "pathcount " << paths->size() << endl;
        return paths;
    }
}

void FatTreeTopology::count_queue(Queue* queue){
    if (_link_usage.find(queue)==_link_usage.end()){
        _link_usage[queue] = 0;
    }

    _link_usage[queue] = _link_usage[queue] + 1;
}

int64_t FatTreeTopology::find_lp_switch(Queue* queue){
    //first check ns_nlp
    for (uint32_t srv=0;srv<NSRV;srv++)
        for (uint32_t tor = 0; tor < NTOR; tor++)
            if (queues_ns_nlp[srv][tor][0] == queue)
                return tor;

    //only count nup to nlp
    count_queue(queue);

    for (uint32_t agg = 0; agg < NAGG; agg++)
        for (uint32_t tor = 0; tor < NTOR; tor++)
            for (uint32_t b = 0; b < _bundlesize[AGG_TIER]; b++) {
                if (queues_nup_nlp[agg][tor][b] == queue)
                    return tor;
            }

    return -1;
}

int64_t FatTreeTopology::find_up_switch(Queue* queue){
    count_queue(queue);
    //first check nc_nup
    for (uint32_t core=0; core < NCORE; core++)
        for (uint32_t agg = 0; agg < NAGG; agg++)
            for (uint32_t b = 0; b < _bundlesize[CORE_TIER]; b++) {
                if (queues_nc_nup[core][agg][b] == queue)
                    return agg;
            }

    //check nlp_nup
    for (uint32_t tor=0; tor < NTOR; tor++)
        for (uint32_t agg = 0; agg < NAGG; agg++)
            for (uint32_t b = 0; b < _bundlesize[AGG_TIER]; b++) {
                if (queues_nlp_nup[tor][agg][b] == queue)
                    return agg;
            }

    return -1;
}

int64_t FatTreeTopology::find_core_switch(Queue* queue){
    count_queue(queue);
    //first check nup_nc
    for (uint32_t agg=0;agg<NAGG;agg++)
        for (uint32_t core = 0;core<NCORE;core++)
            for (uint32_t b = 0; b < _bundlesize[CORE_TIER]; b++) {
                if (queues_nup_nc[agg][core][b] == queue)
                    return core;
            }

    return -1;
}

int64_t FatTreeTopology::find_destination(Queue* queue){
    //first check nlp_ns
    for (uint32_t tor=0; tor<NTOR; tor++)
        for (uint32_t srv = 0; srv<NSRV; srv++)
            if (queues_nlp_ns[tor][srv][0]==queue)
                return srv;

    return -1;
}

void FatTreeTopology::print_path(std::ofstream &paths,uint32_t src,const Route* route){
    paths << "SRC_" << src << " ";
  
    if (route->size()/2==2){
        paths << "LS_" << find_lp_switch((Queue*)route->at(1)) << " ";
        paths << "DST_" << find_destination((Queue*)route->at(3)) << " ";
    } else if (route->size()/2==4){
        paths << "LS_" << find_lp_switch((Queue*)route->at(1)) << " ";
        paths << "US_" << find_up_switch((Queue*)route->at(3)) << " ";
        paths << "LS_" << find_lp_switch((Queue*)route->at(5)) << " ";
        paths << "DST_" << find_destination((Queue*)route->at(7)) << " ";
    } else if (route->size()/2==6){
        paths << "LS_" << find_lp_switch((Queue*)route->at(1)) << " ";
        paths << "US_" << find_up_switch((Queue*)route->at(3)) << " ";
        paths << "CS_" << find_core_switch((Queue*)route->at(5)) << " ";
        paths << "US_" << find_up_switch((Queue*)route->at(7)) << " ";
        paths << "LS_" << find_lp_switch((Queue*)route->at(9)) << " ";
        paths << "DST_" << find_destination((Queue*)route->at(11)) << " ";
    } else {
        paths << "Wrong hop count " << ntoa(route->size()/2);
    }
  
    paths << endl;
}

void FatTreeTopology::add_switch_loggers(Logfile& log, simtime_picosec sample_period) {
    for (uint32_t i = 0; i < NTOR; i++) {
        switches_lp[i]->add_logger(log, sample_period);
    }
    for (uint32_t i = 0; i < NAGG; i++) {
        switches_up[i]->add_logger(log, sample_period);
    }
    for (uint32_t i = 0; i < NCORE
             ; i++) {
        switches_c[i]->add_logger(log, sample_period);
    }
}
