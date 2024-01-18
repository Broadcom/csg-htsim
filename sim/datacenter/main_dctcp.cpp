#include <sstream>
#include <iostream>
#include <string.h>

#include "config.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "fat_tree_topology.h"
#include "queue_lossless_input.h"
#include "connection_matrix.h"
 
uint32_t RTT = 1; // this is per link delay in us; identical RTT microseconds = 0.02 ms
int DEFAULT_NODES = 432;
#define DEFAULT_QUEUE_SIZE 15

EventList eventlist;

void exit_error(char* progr) {
    cout << "Usage " << progr << " [-o log_file]\n\t[-q queue_size] [-host_queue_type swift|prio|fair_prio]\n\t[-tm traffic_matrix_file]\n\t[-topo topology_file]\n\t[-strat route_strategy (single,rand,perm,pull,ecmp,\n\t[-log log_level]\n\t[-logtime log_time]\n\t[-pfc_thresholds low high]" << endl;
    exit(1);
}

int main(int argc, char **argv) {
    mem_b queuesize = DEFAULT_QUEUE_SIZE;
    double logtime = 0.25;
    uint32_t path_entropy_size = 10000000;
    queue_type qt = COMPOSITE;

    bool log_sink = false;
    bool log_tor_downqueue = false;
    bool log_tor_upqueue = false;
    bool log_switches = false;
    bool log_traffic = false;
    bool log_queue_usage = false;
    RouteStrategy route_strategy = NOT_SET;

    stringstream filename(ios_base::out);
    filename << "logout.dat";

    queue_type snd_type = FAIR_PRIO;

    uint64_t high_pfc = 15, low_pfc = 12;

    char* tm_file = NULL;
    char* topo_file = NULL;
    int i = 1;
    while (i < argc) {
        if (!strcmp(argv[i], "-o")) {
            filename.str(std::string());
            filename << argv[i + 1];
            i++;
        } else if (!strcmp(argv[i], "-tm")) {
            tm_file = argv[i + 1];
            cout << "traffic matrix file: " << tm_file << endl;
            i++;
        } else if (!strcmp(argv[i], "-topo")) {
            topo_file = argv[i + 1];
            cout << "FatTree topology file: " << topo_file << endl;
            i++;
        } else if (!strcmp(argv[i], "-q")) {
            queuesize = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-logtime")) {
            logtime = atof(argv[i + 1]);
            cout << "logtime " << logtime << " ms" << endl;
            i++;
        } else if (!strcmp(argv[i],"-pfc_thresholds")){
            low_pfc = atoi(argv[i+1]);
            high_pfc = atoi(argv[i+2]);
            cout << "PFC thresholds high " << high_pfc << " low " << low_pfc << endl;
            i++;
        } else if (!strcmp(argv[i], "-host_queue_type")) {
            if (!strcmp(argv[i+1], "swift")) {
                snd_type = SWIFT_SCHEDULER;
            } 
            else if (!strcmp(argv[i+1], "prio")) {
                snd_type = PRIORITY;
            }
            else if (!strcmp(argv[i+1], "fair_prio")) {
                snd_type = FAIR_PRIO;
            }
            else {
                cout << "Unknown host queue type " << argv[i+1] << " expecting one of swift|prio|fair_prio" << endl;
                exit_error(argv[0]);
            }
            cout << "host queue_type "<< snd_type << endl;
            i++;
        } else if (!strcmp(argv[i],"-log")){
            if (!strcmp(argv[i+1], "sink")) {
                cout << "logging sinks\n";
                log_sink = true;
            } else if (!strcmp(argv[i+1], "tor_downqueue")) {
                cout << "logging tor downqueues\n";
                log_tor_downqueue = true;
            } else if (!strcmp(argv[i+1], "tor_upqueue")) {
                cout << "logging tor upqueues\n";
                log_tor_upqueue = true;
            } else if (!strcmp(argv[i+1], "switch")) {
                cout << "logging total switch queues\n";
                log_switches = true;
            } else if (!strcmp(argv[i+1], "traffic")) {
                cout << "logging traffic\n";
                log_traffic = true;
            } else if (!strcmp(argv[i+1], "queue_usage")) {
                cout << "logging queue usage\n";
                log_queue_usage = true;
            } else {
                exit_error(argv[0]);
            }
            i++;
        } else if (!strcmp(argv[i],"-strat")){
            if (!strcmp(argv[i+1], "perm")) {
                route_strategy = SCATTER_PERMUTE;
            } else if (!strcmp(argv[i+1], "rand")) {
                route_strategy = SCATTER_RANDOM;
            } else if (!strcmp(argv[i+1], "ecmp")) {
                route_strategy = SCATTER_ECMP;
            } else if (!strcmp(argv[i+1], "pull")) {
                route_strategy = PULL_BASED;
            } else if (!strcmp(argv[i+1], "single")) {
                route_strategy = SINGLE_PATH;
            } else if (!strcmp(argv[i+1], "ecmp_host")) {
                route_strategy = ECMP_FIB;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
            } else if (!strcmp(argv[i+1], "rr_ecmp")) {
                //this is the host route strategy;
                route_strategy = ECMP_FIB_ECN;
                qt = COMPOSITE_ECN_LB;
                //this is the switch route strategy. 
                FatTreeSwitch::set_strategy(FatTreeSwitch::RR_ECMP);
            } else if (!strcmp(argv[i+1], "ecmp_host_ecn")) {
                route_strategy = ECMP_FIB_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                qt = COMPOSITE_ECN_LB;
            } else if (!strcmp(argv[i+1], "reactive_ecn")) {
                // Jitu's suggestion for something really simple
                // One path at a time, but switch whenever we get a trim or ecn
                //this is the host route strategy;
                route_strategy = REACTIVE_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                qt = COMPOSITE_ECN_LB;
            } else if (!strcmp(argv[i+1], "ecmp_ar")) {
                route_strategy = ECMP_FIB;
                path_entropy_size = 1;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ADAPTIVE_ROUTING);
            } else if (!strcmp(argv[i+1], "ecmp_host_ar")) {
                route_strategy = ECMP_FIB;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP_ADAPTIVE);
                //the stuff below obsolete
                //FatTreeSwitch::set_ar_fraction(atoi(argv[i+2]));
                //cout << "AR fraction: " << atoi(argv[i+2]) << endl;
                //i++;
            } else if (!strcmp(argv[i+1], "ecmp_rr")) {
                // switch round robin
                route_strategy = ECMP_FIB;
                path_entropy_size = 1;
                FatTreeSwitch::set_strategy(FatTreeSwitch::RR);
            }
            i++;
        } else  {
            cout << "Unknown parameter " << argv[i] << endl;
            exit_error(argv[0]);
        }

        i++;
    }

    LosslessInputQueue::_high_threshold = Packet::data_packet_size()*high_pfc;
    LosslessInputQueue::_low_threshold = Packet::data_packet_size()*low_pfc;

    queuesize = memFromPkt(queuesize);

    // prepare the loggers
    cout << "Logging to " << filename.str() << endl;
    Logfile logfile(filename.str(), eventlist);

    TcpSinkLoggerSampling sinkLogger = TcpSinkLoggerSampling(timeFromMs(logtime), eventlist);
    if (log_sink) {
        logfile.addLogger(sinkLogger);
    }
    TcpTrafficLogger traffic_logger = TcpTrafficLogger();
    if (log_traffic) {
        logfile.addLogger(traffic_logger);
    }

    QueueLoggerFactory* qlf;
    if (log_tor_downqueue || log_tor_upqueue) {
        qlf = new QueueLoggerFactory(&logfile, QueueLoggerFactory::LOGGER_SAMPLING, eventlist);
        qlf->set_sample_period(timeFromUs(10.0));
    } else if (log_queue_usage) {
        qlf = new QueueLoggerFactory(&logfile, QueueLoggerFactory::LOGGER_EMPTY, eventlist);
        qlf->set_sample_period(timeFromUs(10.0));
    }

    // scanner interval must be less than min RTO
    TcpRtxTimerScanner tcpRtxScanner(timeFromMs(10), eventlist);

#ifdef FAT_TREE
    FatTreeTopology* top;
    if (topo_file) {
        cout << "Loading topology from " << topo_file << endl;
        top = FatTreeTopology::load(topo_file, qlf, eventlist,
            queuesize, LOSSLESS_INPUT_ECN, snd_type);
    } else {
        cout << "No topology file specified" << endl;
        exit_error(argv[0]);
    }
#endif

    uint32_t no_of_nodes = top->no_of_nodes();

    vector<const Route*>*** net_paths;
    net_paths = new vector<const Route*>**[no_of_nodes];

    int **path_refcounts;
    path_refcounts = new int*[no_of_nodes];

    int* is_dest = new int[no_of_nodes];
    
    for (size_t s = 0; s < no_of_nodes; s++) {
        is_dest[s] = 0;
        net_paths[s] = new vector<const Route*>*[no_of_nodes];
        path_refcounts[s] = new int[no_of_nodes];
        for (size_t d = 0; d < no_of_nodes; d++) {
            net_paths[s][d] = NULL;
            path_refcounts[s][d] = 0;
        }
    }

    ConnectionMatrix* conns = new ConnectionMatrix(no_of_nodes);
    if (tm_file){
        cout << "Loading connection matrix from  " << tm_file << endl;

        if (!conns->load(tm_file)){
            cout << "Failed to load connection matrix " << tm_file << endl;
            exit(-1);
        }
    }
    else {
        cout << "No connection matrix specified" << endl;
        exit_error(argv[0]);
    }

    if (conns->N != no_of_nodes){
        cout << "Connection matrix number of nodes is " << conns->N << " while I am using " << no_of_nodes << endl;
        exit(-1);
    }

    //handle link failures specified in the connection matrix.
    for (size_t c = 0; c < conns->failures.size(); c++){
        failure* crt = conns->failures.at(c);

        cout << "Adding link failure switch type" << crt->switch_type << " Switch ID " << crt->switch_id << " link ID "  << crt->link_id << endl;
        top->add_failed_link(crt->switch_type,crt->switch_id,crt->link_id);
    }

    // used just to print out stats data at the end
    list <const Route*> routes;

    vector<connection*>* all_conns = conns->getAllConnections();
    vector <TcpSrc*> tcp_srcs;

    Route* routeout, *routein;

    for (size_t c = 0; c < all_conns->size(); c++){
        connection* crt = all_conns->at(c);
        int src = crt->src;
        int dest = crt->dst;
        path_refcounts[src][dest]++;
        path_refcounts[dest][src]++;
                        
        if (!net_paths[src][dest]
            && route_strategy != ECMP_FIB
            && route_strategy != ECMP_FIB_ECN
            && route_strategy != REACTIVE_ECN ) {
            vector<const Route*>* paths = top->get_bidir_paths(src,dest,false);
            net_paths[src][dest] = paths;
            /*
            for (unsigned int i = 0; i < paths->size(); i++) {
              routes.push_back((*paths)[i]);
            }
            */
        }
        if (!net_paths[dest][src]
            && route_strategy != ECMP_FIB
            && route_strategy != ECMP_FIB_ECN
            && route_strategy != REACTIVE_ECN ) {
            vector<const Route*>* paths = top->get_bidir_paths(dest,src,false);
            net_paths[dest][src] = paths;
            /*
            for (unsigned int i = 0; i < paths->size(); i++) {
              routes.push_back((*paths)[i]);
            }
            */
        }
    }

    map <flowid_t, TriggerTarget*> flowmap;

    for (size_t c = 0; c < all_conns->size(); c++){
        connection* crt = all_conns->at(c);
        int src = crt->src;
        int dest = crt->dst;
        cout << "Connection " << crt->src << "->" << crt->dst << " starting at " << crt->start << " size " << crt->size << endl;

        TcpSrc *tcpSrc = new TcpSrc(NULL, NULL, eventlist);
        tcpSrc->set_ssthresh(10*Packet::data_packet_size());
        tcpSrc->set_flowsize(crt->size);
        tcpSrc->_rto = timeFromMs(10);
        tcp_srcs.push_back(tcpSrc);
        tcpSrc->set_dst(dest);
        if (crt->flowid) {
            tcpSrc->set_flowid(crt->flowid);
            assert(flowmap.find(crt->flowid) == flowmap.end()); // don't have dups
            flowmap[crt->flowid] = tcpSrc;
        }
                        
        if (crt->size>0){
            tcpSrc->set_flowsize(crt->size);
        }

        if (crt->trigger) {
            Trigger* trig = conns->getTrigger(crt->trigger, eventlist);
            trig->add_target(*tcpSrc);
        }
        if (crt->send_done_trigger) {
            Trigger* trig = conns->getTrigger(crt->send_done_trigger, eventlist);
            tcpSrc->set_end_trigger(*trig);
        }

        TcpSink *tcpSnk = new TcpSink();
                        
        tcpSrc->setName("tcp_" + ntoa(src) + "_" + ntoa(dest));

        cout << "tcp_" + ntoa(src) + "_" + ntoa(dest) << endl;
        logfile.writeName(*tcpSrc);

        tcpSnk->setName("tcp_sink_" + ntoa(src) + "_" + ntoa(dest));
        logfile.writeName(*tcpSnk);
        if (crt->recv_done_trigger) {
            Trigger* trig = conns->getTrigger(crt->recv_done_trigger, eventlist);
            // This makes no sense since the trigger will never be activated
            tcpSnk->set_end_trigger(*trig);
        }

        tcpRtxScanner.registerTcp(*tcpSrc);

        switch (route_strategy) {
        case SCATTER_PERMUTE:
        case SCATTER_RANDOM:
        case SCATTER_ECMP:
        case PULL_BASED:
            tcpSrc->connect(NULL, NULL, *tcpSnk, crt->start);
#ifdef PACKET_SCATTER
            tcpSrc->set_paths(net_paths[src][dest]);
            tcpSnk->set_paths(net_paths[dest][src]);
#endif
            break;
        case ECMP_FIB:
        case ECMP_FIB_ECN:
        case REACTIVE_ECN:
            {
                Route* srctotor = new Route();
                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)][0]);
                srctotor->push_back(top->pipes_ns_nlp[src][top->HOST_POD_SWITCH(src)][0]);
                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)][0]->getRemoteEndpoint());

                Route* dsttotor = new Route();
                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)][0]);
                dsttotor->push_back(top->pipes_ns_nlp[dest][top->HOST_POD_SWITCH(dest)][0]);
                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)][0]->getRemoteEndpoint());

                tcpSrc->connect(*srctotor, *dsttotor, *tcpSnk, crt->start);
#ifdef PACKET_SCATTER
                tcpSrc->set_paths(path_entropy_size);
                tcpSnk->set_paths(path_entropy_size);
#endif

                //register src and snk to receive packets from their respective TORs. 
                assert(top->switches_lp[top->HOST_POD_SWITCH(src)]);
                assert(top->switches_lp[top->HOST_POD_SWITCH(src)]);
                top->switches_lp[top->HOST_POD_SWITCH(src)]->addHostPort(src, tcpSrc->flow_id(), tcpSrc);
                top->switches_lp[top->HOST_POD_SWITCH(dest)]->addHostPort(dest,tcpSrc->flow_id(), tcpSnk);
                break;
            }
        case SINGLE_PATH:
            {
                assert(route_strategy==SINGLE_PATH);
                int choice = rand()%net_paths[src][dest]->size();
                routeout = new Route(*(net_paths[src][dest]->at(choice)));
                routeout->add_endpoints(tcpSrc, tcpSnk);
                                
                routein = new Route(*top->get_bidir_paths(dest,src,false)->at(choice));
                routein->add_endpoints(tcpSnk, tcpSrc);
                tcpSrc->connect(*routeout, *routein, *tcpSnk, crt->start);
                break;
            }
        case NOT_SET:
            abort();
        }

        path_refcounts[src][dest]--;
        path_refcounts[dest][src]--;


        // set up the triggers
        // xxx

        // free up the routes if no other connection needs them 
        if (path_refcounts[src][dest] == 0 && net_paths[src][dest]) {
            vector<const Route*>::iterator i;
            for (i = net_paths[src][dest]->begin(); i != net_paths[src][dest]->end(); i++) {
                if ((*i)->reverse())
                    delete (*i)->reverse();
                delete *i;
            }
            delete net_paths[src][dest];
        }
        if (path_refcounts[dest][src] == 0 && net_paths[dest][src]) {
            vector<const Route*>::iterator i;
            for (i = net_paths[dest][src]->begin(); i != net_paths[dest][src]->end(); i++) {
                if ((*i)->reverse())
                    delete (*i)->reverse();
                delete *i;
            }
            delete net_paths[dest][src];
        }

        if (log_sink) {
            sinkLogger.monitorSink(tcpSnk);
        }
    }

    for (size_t ix = 0; ix < no_of_nodes; ix++) {
        delete path_refcounts[ix];
    }

    Logged::dump_idmap();

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    //logfile.write("# hostnicrate = " + ntoa(linkspeed/1000000) + " Mbps");
    //logfile.write("# corelinkrate = " + ntoa(HOST_NIC*CORE_TO_HOST) + " pkt/sec");
    //logfile.write("# buffer = " + ntoa((double) (queues_na_ni[0][1]->_maxsize) / ((double) pktsize)) + " pkt");
    double rtt = timeAsSec(timeFromUs(RTT));
    logfile.write("# rtt =" + ntoa(rtt));
    
    // GO!
    cout << "Starting simulation" << endl;
    while (eventlist.doNextEvent()) {
    }

    cout << "Done" << endl;

}