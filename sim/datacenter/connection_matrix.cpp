// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "connection_matrix.h"
#include <string.h>
#include <stdio.h>
#include <iostream>
#include "math.h"

ConnectionMatrix::ConnectionMatrix(uint32_t n)
{
  N = n;
  conns = NULL;
}

void ConnectionMatrix::setPermutation(uint32_t conn){
  setPermutation(conn,1);
}

void ConnectionMatrix::addConnection(uint32_t src, uint32_t dest){
  if (connections.find(src)==connections.end())
      connections[src] = new vector<uint32_t>();
  
  connections[src]->push_back(dest);
}

void ConnectionMatrix::setPermutation(uint32_t conn, uint32_t rack_size){
    //int is_dest[N];
    uint32_t dest,pos;
    int64_t to[N]; // enough to hold 32-bit ID and -1
    uint32_t *rack_load;
    uint32_t rack_count;
    vector<uint32_t> perm_tmp;

    rack_count = N/rack_size;
    rack_load = (uint32_t*)calloc(rack_count,sizeof(uint32_t));
  
    for (uint32_t i=0; i<N; i++){
        //is_dest[i] = 0;
        to[i] = -1;
        perm_tmp.push_back(i);
    }

    for (uint32_t src = 0; src < N; src++) {
        do {
            pos = rand()%perm_tmp.size();
        } while(src==perm_tmp[pos]&&perm_tmp.size()>1);

        dest = perm_tmp[pos];
        assert(src!=dest);

        perm_tmp.erase(perm_tmp.begin()+pos);
        to[src] = dest;
    }

    for (uint32_t i = 0; i<conn; i++){
        if (!perm_tmp.size())
            for (uint32_t q=0; q<N; q++){
                perm_tmp.push_back(q);
            }

        pos = rand()%perm_tmp.size();

        if (rack_count<N){
            //int pos2 = rand()%perm_tmp.size();

            //if (rack_load[perm_tmp[pos]/rack_size]>rack_load[perm_tmp[pos2]/rack_size])
            //pos = pos2;
        }

        uint32_t src = perm_tmp[pos];
        printf("src=%d rack_size=%d\n", src, rack_size);
        rack_load[src/rack_size]++;
        perm_tmp.erase(perm_tmp.begin()+pos);

        if (connections.find(src)==connections.end()){
            connections[src] = new vector<uint32_t>();
        }
        connections[src]->push_back(to[src]);
    }

    cout << "Rack load: ";
    for (uint32_t i=0; i<rack_count; i++)
        cout << rack_load[i] << " ";

    cout <<endl;

    free(rack_load);
}

void ConnectionMatrix::setPermutation(){
    int is_dest[N];
    uint32_t dest;
  
    for (uint32_t i=0; i<N; i++)
        is_dest[i] = 0;

    for (uint32_t src = 0; src < N; src++) {
        vector<uint32_t>* destinations = new vector<uint32_t>();
      
        uint32_t r = rand()%(N-src);
        for (dest = 0; dest<N; dest++){
            if (r==0 && !is_dest[dest])
                break;
            if (!is_dest[dest]) {
                assert(r>0);
                r--;
            }
        }

        if (r!=0 || is_dest[dest]){
            cout << "Wrong connections r " <<  r << "is_dest "<<is_dest[dest]<<endl;
            exit(1);
        }
      
        if (src == dest){
            //find first other destination that is different!
            do {
                dest = (dest+1)%N;
            }
            while (is_dest[dest]);
        
            if (src == dest){
                printf("Wrong connections 2!\n");
                exit(1);
            }
        }
        is_dest[dest] = 1;
        destinations->push_back(dest);

        connections[src] = destinations;    
    }
}

void ConnectionMatrix::setStride(uint32_t conns){
    for (uint32_t src = 0; src<conns; src++) {
        uint32_t dest = (src+N/2)%N;

        connections[src] = new vector<uint32_t>();
        connections[src]->push_back(dest);
    }
}

void ConnectionMatrix::setPermutationShuffle(uint32_t conns){
    // we want random sources and destinations
    // we'll shuffle all the destinations for all possible sources, then pick the right number of connection afterwards
  
    vector<uint32_t> srcs(N,0); // list of srcs (used to choose which sources actually send)
    vector<uint32_t> dsts(N,0); // list of dests (used to choose which dst each src sends to)
    // initialize everyone sending to themselves
    for (uint32_t src = 0; src<N; src++) {
        srcs[src] = src;
        dsts[src] = src;
    }

    // Fisher-Yates shuffle of boths srcs and dsts
    for (uint32_t src = 0; src<N-1; src++) {
        // target between src+1 and N - 1 inclusive
        uint32_t target = rand()%(N - (src + 1)) + src + 1;
        uint32_t tmp = dsts[src];
        dsts[src] = dsts[target];
        dsts[target] = tmp;

        target = rand()%(N - (src + 1)) + src + 1;
        tmp = srcs[src];
        srcs[src] = srcs[target];
        srcs[target] = tmp;
    }

    // sanity check
    vector<uint32_t> counts(N,0);
    for (uint32_t src = 0; src<N; src++) {
        counts[dsts[src]]++;
        assert(counts[dsts[src]] <= 1);
        assert(dsts[src] != src);
        assert(dsts[src] < N);
    }

    // OK, we should have a shuffled set of N destinations, where no src sends to itself
    // we now use the first "conns" sources from our shuffled "srcs" vector
  
    for (uint32_t i = 0; i<conns; i++) {
        connections[srcs[i]] = new vector<uint32_t>();
        // ensure we don't send to ourselves
        assert(srcs[i] != dsts[srcs[i]]);
        connections[srcs[i]]->push_back(dsts[srcs[i]]);
        cout << "Src " << srcs[i] << " Dst " << dsts[srcs[i]] << endl;
    }
}

void ConnectionMatrix::setPermutationShuffleIncast(uint32_t conns){
    // we want random sources and destinations
    // we'll shuffle all the destinations for all possible sources, then pick the right number of connection afterwards
  
    vector<uint32_t> srcs(N,0); // list of srcs (used to choose which sources actually send)
    vector<uint32_t> dsts(N,0); // list of dests (used to choose which dst each src sends to)
    // initialize everyone sending to themselves
    for (uint32_t src = 0;src<N; src++) {
        srcs[src] = src;
        dsts[src] = src;
    }

    // Fisher-Yates shuffle of boths srcs and dsts
    for (uint32_t src = 0;src<N-1; src++) {
        // target between src+1 and N - 1 inclusive
        uint32_t target = rand()%(N - (src + 1)) + src + 1;
        uint32_t tmp = dsts[src];
        dsts[src] = dsts[target];
        dsts[target] = tmp;

        target = rand()%(N - (src + 1)) + src + 1;
        tmp = srcs[src];
        srcs[src] = srcs[target];
        srcs[target] = tmp;
    }

    // sanity check
    vector<uint32_t> counts(N,0);
    for (uint32_t src = 0;src<N; src++) {
        counts[dsts[src]]++;
        assert(counts[dsts[src]] <= 1);
        assert(dsts[src] != src);
        assert(dsts[src] < N);
    }

    // OK, we should have a shuffled set of N destinations, where no src sends to itself
    // we now use the first "conns" sources from our shuffled "srcs" vector
  
    for (uint32_t i = 0;i<conns; i++) {
        connections[srcs[i]] = new vector<uint32_t>();
        // ensure we don't send to ourselves
        assert(srcs[i] != dsts[srcs[i]]);
        connections[srcs[i]]->push_back(dsts[srcs[i]]);
        cout << "Src " << srcs[i] << " Dst " << dsts[srcs[i]] << endl;
    }

    //do an incast from the remainder nodes to one idle destination
    for (uint32_t i = conns;i<N; i++) {
        connections[srcs[i]] = new vector<uint32_t>();
        // ensure we don't send to ourselves
        if (srcs[i] == dsts[srcs[conns]])
            continue;

        connections[srcs[i]]->push_back(dsts[srcs[conns]]);
        cout << "Incast: Src " << srcs[i] << " Dst " << dsts[srcs[conns]] << endl;
    }  
}

void ConnectionMatrix::setLocalTraffic(Topology* top){
    for (uint32_t src = 0;src<N;src++){
        connections[src] = new vector<uint32_t>();
        vector<uint32_t>* neighbours = top->get_neighbours(src);
        for (uint32_t n=0; n<neighbours->size(); n++)
            connections[src]->push_back(neighbours->at(n));
    }
}

void ConnectionMatrix::setRandom(uint32_t cnx){
    for (uint32_t conn = 0;conn<cnx; conn++) {
        uint32_t src = rand()%N;
        uint32_t dest = rand()%N;

        if (src==0||dest==N-1){
            conn--;
            continue;
        }      

        if (connections.find(src)==connections.end()){
            connections[src] = new vector<uint32_t>();
        }

        connections[src]->push_back(dest);
    }
}

void ConnectionMatrix::setVL2(){
    for (uint32_t src = 0;src<N; src++) {
        connections[src] = new vector<uint32_t>();

        //need to draw a number from VL2 distribution
        uint32_t crt = -1;
        double coin = drand();
        if (coin<0.3)
            crt = 1;
        else if (coin<0.35)
            crt = 1+rand()%10;
        else if (coin<0.85)
            crt = 10;
        else if (coin<0.95)
            crt = 10+rand()%70;
        else 
            crt = 80;

        for (uint32_t i = 0;i<crt;i++){
            uint32_t dest = rand()%N;
            if (src==dest){
                i--;
                continue;
            }
            connections[src]->push_back(dest);
        }
    }
}

vector<connection*>* ConnectionMatrix::getAllConnections(){
    if (conns!=NULL){
        //conns is initialized from file or from the old connections vector. 
        return conns;
    }

    //builds conns from the other old connections vector
    conns = new vector<connection*>();

    vector<uint32_t>* destinations;
    map<uint32_t, vector<uint32_t>*>::iterator it;

    for (it = connections.begin(); it!=connections.end();it++){
        uint32_t src = (*it).first;
        destinations = (vector<uint32_t>*)(*it).second;
    
        vector<uint32_t> subflows_chosen;
    
        for (uint32_t dst_id = 0;dst_id<destinations->size();dst_id++){
            connection* tmp = new connection();
            tmp->src = src;
            tmp->dst = destinations->at(dst_id);
            tmp->start = 0;
            tmp->size = 0;
            conns->push_back(tmp);
        }
    }
    return conns;
}

void ConnectionMatrix::setStaggeredRandom(Topology* top,uint32_t conns,double local){
    for (uint32_t conn = 0;conn<conns; conn++) {
        uint32_t src = rand()%N;

        if (connections.find(src)==connections.end()){
            connections[src] = new vector<uint32_t>();
        }

        vector<uint32_t>* neighbours = top->get_neighbours(src);

        uint32_t dest;
        if (drand()<local){
            dest = neighbours->at(rand()%neighbours->size());
        }
        else {
            dest = rand()%N;
        }
        connections[src]->push_back(dest);
    }
}

void ConnectionMatrix::setStaggeredPermutation(Topology* top,double local){
    int is_dest[N];
    uint32_t dest = -1,i,found = 0;

    memset(is_dest,0,N*sizeof(int));

    for (uint32_t src = 0;src<N; src++) {
        connections[src] = new vector<uint32_t>();
        vector<uint32_t>* neighbours = top->get_neighbours(src);

        double v = drand();
        if (v<local){
            i = 0;
            do {
                found = 0;
                dest = neighbours->at(rand()%neighbours->size());
                if (is_dest[dest])
                    found = 1;
            }
            while (found && i++<15);
        }
    
        if (v>=local || (v<local&&found)){
            dest = rand()%N;
            while (is_dest[dest])
                dest = (dest+1)%N;
        }

        assert(dest>=0&&dest<N);
        assert(is_dest[dest]==0);
     
        connections[src]->push_back(dest);
        is_dest[dest] = 1;
    }
}

void ConnectionMatrix::setManytoMany(uint32_t c){
    vector<uint32_t> hosts;
    uint32_t t,f;

    if (c<N)
        for (uint32_t i=0; i<c; i++){
            do {
                t = rand()%N;
                f = 0;
                for (uint32_t j=0; j<hosts.size(); j++)
                    if (hosts[j]==t){
                        f = 1;
                        break;
                    }
            } while(f);
      
            hosts.push_back(t);
        }
    else {
        for (uint32_t i=0; i<c; i++){
            hosts.push_back(i);
        }    
    }

    for (uint32_t i=0; i<hosts.size(); i++){
        connections[hosts[i]] = new vector<uint32_t>();
        for (uint32_t j=0; j<hosts.size(); j++){
            if (i==j)
                continue;
            connections[hosts[i]]->push_back(hosts[j]);
        }
    }
}

void ConnectionMatrix::setHotspot(uint32_t hosts_per_hotspot, uint32_t count){
    int is_dest[N],is_done[N];
    for (uint32_t i=0;i<N;i++){
        is_dest[i] = 0;
        is_done[i] = 0;
    }

    uint32_t first, src;
    for (uint32_t k=0;k<count;k++){
        do {
            first = rand()%N;
        }
        while (is_dest[first]);
    
        is_dest[first] = 1;
    
        for (uint32_t i=0;i<hosts_per_hotspot;i++){
            do{
                if (hosts_per_hotspot==N)
                    src = i;
                else
                    src = rand()%N;
            }
            while(is_done[src]);
            is_done[src]=1;

            if (connections.find(src)==connections.end())      
                connections[src] = new vector<uint32_t>();

            connections[src]->push_back(first);
            is_done[src] = 1;
        }  
    }
}

void ConnectionMatrix::setIncastLocalPerm(uint32_t hosts_per_hotspot){
    //int is_dest[N];
    int is_done[N];

    for (uint32_t i=0;i<N;i++){
        //is_dest[i] = 0;
        is_done[i] = 0;
    }

    uint32_t first, src;
    first = rand()%N;
  
    //is_dest[first] = 1;
    
    for (uint32_t i=0;i<hosts_per_hotspot;i++){
        do{
            if (hosts_per_hotspot==N)
                src = i;
            else
                src = rand()%N;
        }
        while(is_done[src]);
        is_done[src]=1;
    
        if (connections.find(src)==connections.end())      
            connections[src] = new vector<uint32_t>();
    
        connections[src]->push_back(first);
        is_done[src] = 1;
    }

    //need to add permutation connections in the same rack as the destination.
    uint32_t K = 0, crt = 0;
    while (crt < N) {
        K++;
        crt = K * K * K /4;
    }
    if (crt > N) {
        cerr << "Topology Error: can't have a FatTree with " << N
             << " nodes\n";
        exit(1);
    }
  
    printf ("K is %d\n",K);
    uint32_t per_rack = K/2;
  
    for (uint32_t dst = (first / per_rack) * per_rack; dst < ((first / per_rack)+1)*per_rack;dst++){
        if (dst==first)
            continue;
    
        do{
            src = rand()%N;
        }
        while(is_done[src]);
        is_done[src]=1;
    
        if (connections.find(src)==connections.end())
            connections[src] = new vector<uint32_t>();

        connections[src]->push_back(dst);
    }
}

void ConnectionMatrix::setHotspotOutcast(uint32_t hosts_per_hotspot, uint32_t count){
    int is_dest[N],is_done[N];
    for (uint32_t i=0;i<N;i++){
        is_dest[i] = 0;
        is_done[i] = 0;
    }

    uint32_t first, src;
    for (uint32_t k=0;k<count;k++){
        do {
            first = rand()%N;
        }
        while (is_dest[first]);
    
        is_dest[first] = 1;
    
        for (uint32_t i=0;i<hosts_per_hotspot;i++){
            do{
                if (hosts_per_hotspot==N)
                    src = i;
                else
                    src = rand()%N;
            }
            while(is_done[src]);
            is_done[src]=1;

            if (connections.find(src)==connections.end())      
                connections[src] = new vector<uint32_t>();

            connections[src]->push_back(first);
            is_done[src] = 1;
        }  
    }

    //do outcast now.
    //find outcast source first.
    //outcast to all destinations + 1.
    uint32_t outcast_source;
    for (outcast_source = 0;outcast_source<N;outcast_source++){
        if (!is_done[outcast_source])
            break;
    }
    if (outcast_source==N) {
        cout << "Can't find free outcast source" << endl;
        abort();
    }
    uint32_t outcast_dst;
    for (outcast_dst = 0;outcast_dst<N;outcast_dst++){
        if (!is_dest[outcast_dst])
            break;
    }
    if (outcast_dst==N) {
        cout << "Can't find free outcast destination" << endl;
        abort();
    }

    connections[outcast_source] = new vector<uint32_t>();

    //outcast to all destinations.
    for (uint32_t i = 0;i<N;i++){
        if (is_dest[i]){
            connections[outcast_source]->push_back(i);
        }
    }

    connections[outcast_source]->push_back(outcast_dst);  
}


void ConnectionMatrix::setIncast(uint32_t hosts_per_hotspot, uint32_t center){
    // creates an incast to destination 0
    uint32_t first = 0;

    for (uint32_t i=center;i<hosts_per_hotspot+center;i++){
        if (connections.find(i)==connections.end())      
            connections[i] = new vector<uint32_t>();
    
        connections[i]->push_back(first);
    }  
}

void ConnectionMatrix::setOutcast(uint32_t src,uint32_t hosts_per_hotspot, uint32_t center){
    // creates an outcast from source 0 to destinations center to centre+hosts_per_hotspot
    connections[src] = new vector<uint32_t>();

    for (uint32_t i=center;i<hosts_per_hotspot+center;i++){
        connections[src]->push_back(i%N);
    }  
}

bool ConnectionMatrix::save(const char * filename){
    //save conns to disk.
    FILE* f = fopen(filename,"w+");

    if (!f)
        return false;
    else
        return save(f);
}

bool ConnectionMatrix::save(FILE* f){
    if (!conns)
        getAllConnections();

    if (fprintf(f,"Nodes %d\n",N)<0)
        return false;

    if (fprintf(f,"Connections %lu\n",conns->size())<0)
        return false;

    for (uint32_t i = 0; i < conns->size(); i++){
        if (fprintf(f,"%u->%u start %f size %u\n", conns->at(i)->src, conns->at(i)->dst,
                    timeAsUs(conns->at(i)->start), conns->at(i)->size) < 0)
            return false;
    }
    fclose(f);

    return true;
}

/*
bool ConnectionMatrix::load(const char * filename){
    //init conns.
    FILE* f = fopen(filename,"r");

    if (!f)
        return false;
    else
        return load(f);  
}
*/

bool ConnectionMatrix::load(const char * filename){
    //init conns.
    std::ifstream file(filename);
    if (file.is_open()) {
        bool success = load(file);
        file.close();
        return success; 
    } else {
        return false;
    }
}

void tokenize(string const &str, const char delim, vector<string> &out)
{
    stringstream ss(str);
    string s;
    while (getline(ss, s, delim)) {
        out.push_back(s);
    }
}

bool ConnectionMatrix::load(istream& file){
    uint32_t conns_size = 0, triggers_size = 0, failures_size = 0;
  
    assert(!conns);
    conns = new vector<connection*>();

    /*
    fscanf(f,"Nodes %d\n",&N);
    fscanf(f,"Connections %d\n",&conns_size);

    for (uint32_t i = 0;i < conns_size;i++){
        connection * c = new connection;
        double start;  // note start in the file can be a double in units of us
        if (fscanf(f,"%u->%u start %lf size %u\n", &c->src, &c->dst, &start, &c->size) < 4) {
            perror("Connection matrix load error");
            fprintf(stderr, "Incorrect syntax for connection %d\n", i);
            return false;
        }
        c->start = timeFromUs(start);
        conns->push_back(c);
    }

    
    fclose(f);
    */
    
    std::string line;
    int linecount = 0;
    uint32_t conn_count = 0;
    uint32_t trig_count = 0;
    // parse header
    while (std::getline(file, line)) {
                linecount++;
                vector<string> tokens;
                tokenize(line, ' ', tokens);
                
                if (tokens.size() == 0 || tokens[0][0] == '#') {
                        continue;
                } else         if (tokens[0] == "Nodes") {
                        N = stoi(tokens[1]);
                } else if (tokens[0] == "Connections") {
                        conns_size = stoi(tokens[1]);
                } else if (tokens[0] == "Triggers") {
                        triggers_size = stoi(tokens[1]);
                } else if (tokens[0] == "Failures") {
                        failures_size = stoi(tokens[1]);
                }
                else if (tokens[0].find("->") != string::npos || tokens[0] == "trigger" || tokens[0] == "failure") {
                        // we're done with the header
                        break;
                }
    }
    linecount--;
    cout << "Nodes: " << N << " Connections: " << conns_size << " Triggers: " << triggers_size << " Failures: " << failures_size << endl;
    //parse rest of file
    do {
        linecount++;
        vector<string> tokens;
        tokenize(line, ' ', tokens);
        if (tokens.size() < 1) {
            continue;
        }
        size_t dstix = tokens[0].find("->");
        if (dstix != string::npos) {
            // we're parsing a connection
            dstix += 2;
            connection * c = new connection;
            conn_count++;
            c->flowid = 0;
            c->send_done_trigger = 0;
            c->recv_done_trigger = 0;
            c->trigger = 0;
            c->src = stoi(tokens[0]);
            c->dst = stoi(tokens[0].substr(dstix));
            c->priority = 2000000;
            c->start = NO_START;
            for (size_t i = 1; i < tokens.size(); i++) {
                if (tokens[i] == "start") {
                    i++;
                    double start = stof(tokens[i]);
                    c->start = start; // start is in picoseconds already
                } else if (tokens[i] == "size") {
                    i++;
                    c->size = stoi(tokens[i]);
                } else if (tokens[i] == "id") {
                    i++;
                    c->flowid = stoi(tokens[i]);
                    if (c->flowid == 0) {
                        cerr << "Flow ID zero is not allowed\n";
                        exit(1);
                    }
                } else if (tokens[i] == "trigger") {
                    i++;
                    c->trigger = stoi(tokens[i]);
                    c->start = TRIGGER_START;
                    map<uint32_t, trigger*>::iterator it = triggers.find(c->trigger);
                    if (it == triggers.end()) {
                                trigger *t = new trigger;
                                t->id = c->trigger;
                                t->count = 0;
                                t->type = UNSPECIFIED;
                                t->trigger = 0;
                                assert(c->flowid);
                                t->flows.push_back(c->flowid);
                                triggers[t->id] = t;
                    } else {
                                trigger *t = it->second;
                                assert(c->flowid);
                                t->flows.push_back(c->flowid);
                    }
                } else if (tokens[i] == "send_done_trigger") {
                    i++;
                    c->send_done_trigger = stoi(tokens[i]);
                } else if (tokens[i] == "recv_done_trigger") {
                    i++;
                    c->recv_done_trigger = stoi(tokens[i]);
                } else if (tokens[i] == "prio") {
                    i++;
                    c->priority = stoi(tokens[i]);
                } else {
                    cerr << "Error: unknown token: " << tokens[i] << " at line "
                         << linecount << endl;
                    exit(1);
                }
            }
            if (c->start == NO_START && !c->trigger) {
                        cerr << "Error: no start method specified for flow at line "
                                << linecount << endl;
                        exit(1);
            }
            if (c->start != TRIGGER_START && c->trigger) {
                        cerr << "Error: both start time and trigger specified for flow at line "
                                << linecount << endl;
                        exit(1);
            }
            conns->push_back(c);
        } else if (tokens[0] == "trigger") {
            // we're parsing a trigger
            trigger *t = new trigger;
            trig_count++;
            t->id = 0;
            t->count = 0;
            t->type = UNSPECIFIED;
            t->trigger = 0;
            for (size_t i = 1; i < tokens.size(); i++) {
                        if (tokens[i] == "id") {
                                i++;
                                t->id = stoi(tokens[i]);
                                if (t->id == 0) {
                                    cerr << "Trigger ID zero is not allowed\n";
                                    exit(1);
                                }
                        } else if (tokens[i] == "count") {
                                i++;
                                t->count = stoi(tokens[i]);
                        } else if (tokens[i] == "oneshot") {
                                assert(t->type == UNSPECIFIED);
                                t->type = SINGLE_SHOT;
                        } else if (tokens[i] == "multishot") {
                                assert(t->type == UNSPECIFIED);
                                t->type = MULTI_SHOT;                                
                        } else if (tokens[i] == "barrier") {
                                assert(t->type == UNSPECIFIED);
                                t->type = BARRIER;
                        } else {
                                cerr << "Error: unknown id: " << tokens[i] << " at line " << linecount << endl;
                                exit(1);
                        }
            }

            if (!t->id) {
                        cerr << "Trigger with no id at line " << linecount << endl;
                        exit(1);
            }
            if (t->type == UNSPECIFIED) {
                        cerr << "Trigger with no type at line " << linecount << endl;
                        exit(1);
            }
            map<triggerid_t, trigger*>::iterator it = triggers.find(t->id);
            if (it == triggers.end()) {
                        // first we've heard of this trigger ID
                        triggers[t->id] = t;
            } else {
                        // a trigger with this ID already exists, so we'll use that
                        trigger *old_trigger = it->second;
                        assert(t->id == old_trigger->id);
                        assert(old_trigger->type == UNSPECIFIED);
                        old_trigger->type = t->type;
                        old_trigger->count = t->count;
                        delete t;
            }
        } else if (tokens[0] == "failure"){
                assert (failures.size()<failures_size);

            failure *f = new failure;

            for (size_t i = 1; i < tokens.size(); i++) {
                        if (tokens[i] == "switch_type") {
                                i++;
                                if (tokens[i]=="TOR")
                                        f->switch_type = FatTreeSwitch::TOR;
                                else if (tokens[i]=="AGG")
                                        f->switch_type = FatTreeSwitch::AGG;
                                else if (tokens[i]=="CORE")
                                        f->switch_type = FatTreeSwitch::CORE;
                                else {
                                        cout << "Unknown switch type " << tokens[i] << ", expecting one of TOR, AGG or CORE "<<endl;
                                        exit(1);
                                }
                        } else if (tokens[i] == "switch_id") {
                                i++;
                                f->switch_id = stoi(tokens[i]);
                        } else if (tokens[i] == "link_id") {
                                i++;
                                f->link_id = stoi(tokens[i]);
                        } else {
                                cerr << "Error: unknown failure attribute " << tokens[i] << " at line " << linecount << endl;
                                exit(1);
                        }
            }
                failures.push_back(f);
        } else {
            cerr << "Error: unknown id: " << tokens[0] << " at line " << linecount << endl;
            exit(1);
        }
    } while (std::getline(file, line));

    // some sanity checks now we've loaded all the data
    if (conn_count != conns_size) {
                cerr << "Mismatch in connection count, specified: " << conns_size
                        << " actual " << conn_count << endl;
                exit(1);
    }
    if (trig_count != triggers_size) {
                cerr << "Mismatch in trigger count, specified: " << triggers_size
                        << " actual " << trig_count << endl;
                exit(1);
    }
    if (failures.size() != failures_size) {
                cerr << "Mismatch in failure count, specified: " << failures_size
                        << " actual " << failures.size() << endl;
                exit(1);
    }

    map<triggerid_t, trigger*>::iterator it;
    for (it = triggers.begin(); it != triggers.end(); it++) {
        if (it->second->type == UNSPECIFIED) {
            cerr << "Trigger " << it->second->id << " referenced but not specified\n";
            exit(1);
        }
    }
    return true;
}


Trigger*
ConnectionMatrix::getTrigger(triggerid_t id, EventList& eventlist) {
    struct trigger* t = triggers.at(id);
    if (t->trigger == 0) {
        // the actual trigger doesn't exist yet, so create it now
        switch (t->type) {
        case SINGLE_SHOT:
            t->trigger = new SingleShotTrigger(eventlist, t->id);
            //cout << "creating single_shot with id " << id << endl;
            break;
        case MULTI_SHOT:
            t->trigger = new MultiShotTrigger(eventlist, t->id);
            //cout << "creating single_shot with id " << id << endl;
            break;
        case BARRIER:
            t->trigger = new BarrierTrigger(eventlist, t->id, t->count);
            cout << "creating barrier with id " << id << endl;
            break;
        case UNSPECIFIED:
            abort();
        }
        /*
        vector <flowid_t>::iterator i;
        for (i = t->flows.begin(); i != t->flows.end(); i++) {
            flowid_t flowid = *i;
            cout << "looking for flow " << flowid << endl;
        }
        */
    }
    return t->trigger;
}

