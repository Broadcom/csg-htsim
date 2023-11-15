// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sstream>
#include <iostream>
#include <math.h>
#include <algorithm>
#include <vector>
using namespace std;

//#ifdef __clang__
#include <unordered_map>
#define hashmap unordered_map
//#else
//#include <ext/hash_map>
//#define hashmap __gnu_cxx::hash_map
//#endif

#include <vector>

#include "loggers.h"
#include "eqds_logger.h"

struct eqint
{
    bool operator()(int s1, int s2) const
    {
        return s1==s2;
    }
};

int main(int argc, char** argv){
    if (argc < 2){
        printf("Usage %s filename [-show|-verbose|-ascii]\n", argv[0]);
        return 1;
    }

    bool show = false, verbose = false, ascii = false;
    stringstream filename;
    filename.str(std::string());
    filename << "";
    vector <string> filters;
    vector <string> splits;
    vector <int> fields;

    int i = 2;
    while (i<argc) {
        if (!strcmp(argv[i],"-idmap")){
            filename << argv[i+1];
            i++;
        } else if (!strcmp(argv[i],"-show")){
            show = true;
        } else if (!strcmp(argv[i],"-verbose")){
            verbose = true;
        } else if (!strcmp(argv[i],"-ascii") || !strcmp(argv[i],"--ascii")){
            ascii = true;
        } else if (!strcmp(argv[i],"-filter")){
            string* s = new string(argv[i+1]);
            filters.push_back(*s);
            cout << *s << endl;
            i++;
        } else if (!strcmp(argv[i],"-split")){
            string* s = new string(argv[i+1]);
            splits.push_back(*s);
            i++;
        } else if (!strcmp(argv[i],"-field")){
            fields.push_back(atoi(argv[i+1]));
            i++;
        }
        i++;
    }

    /*    
          if ((argc>2 && !strcmp(argv[2], "-show"))
          || (argc>3 && !strcmp(argv[3], "-show")))
          show = 1;

          if ((argc>2 && !strcmp(argv[2], "-ascii"))
          || (argc>3 && !strcmp(argv[3], "-ascii")))
          ascii = 1;

          if ((argc>2 && !strcmp(argv[2], "-verbose"))
          || (argc>3 && !strcmp(argv[3], "-verbose")))
          verbose = 1;
    */
    if (ascii && (show || verbose)) {
        perror("Use -ascii by itself, not with -show or -verbose!\n");
        exit(1);
    }


    FILE* logfile;
    hashmap<int, string> object_names;

    logfile = fopen(argv[1], "rbS");
    if (logfile==NULL) {
        cerr << "Failed to open logfile " << argv[1] << endl;
        exit(1);
    }

    //parse preamble first
    char* line = new char[10000];
    //cout << "reading preamble\n";
    int numRecords = 0, transpose = 1;
    while (1){
        if(!fgets(line, 10000, logfile)) {
            perror("File ended while reading preamble!\n");
            exit(1);
        }
        if (strstr(line, "# TRACE")) {
            //we have finished the preamble;
            if(numRecords<=0) {
                printf("Numrecords is %d after preamble, bailing\n", numRecords);
                exit(1);
            }
            break;
        }

        if (strstr(line, "# numrecords=")) {
            numRecords = atoi(line+13);
        };

        if (strstr(line, "# transpose=")) {
            transpose = atoi(line+12);
        };

        //
        if (strstr(line, ": ")){
            //logged names and ids
            char* split = strstr(line,"=");

            int id = -1;
            if (split)
                id = atoi(split+1);
            
            split[0]=0;
            string * name = new string(line+2);
            assert(id >= 0);
            object_names[id] = *name;
        }
    }
    //cout << "done\n";
    FILE* idmapfile;

    idmapfile = fopen(filename.str().c_str(), "rbS");
    if (idmapfile!=NULL) {
        cout << "Parsing IDMAP file";
        while (1){
            if(!fgets(line, 10000, idmapfile)) {
                break;
            }
            char* split = strstr(line," ");

            int id;
            if (!split) 
                continue;

            split++;
            split[strlen(split)-1] = 0;

            string * name = new string(split);
            split[0]=0;

            id = atoi(line);
                                
            //cout << "Found mapping " << id << " to " << *name << endl;
            object_names[id] = *name;
        }
    }


    //must find the number of records here, and go to #TRACE

    int numread = numRecords;

    double* timeRec = new double[numRecords];
    uint32_t* typeRec = new uint32_t[numRecords];
    uint32_t* idRec = new uint32_t[numRecords];
    uint32_t* evRec = new uint32_t[numRecords];
    double* val1Rec = new double[numRecords];
    double* val2Rec = new double[numRecords];
    double *val3Rec = new double[numRecords];

    if (transpose) {
        /* old-style transposed data */
        std::ignore = fread(timeRec, sizeof(double), numread, logfile);
        std::ignore = fread(typeRec, sizeof(uint32_t), numread, logfile);
        std::ignore = fread(idRec,   sizeof(uint32_t), numread, logfile);
        std::ignore = fread(evRec,   sizeof(uint32_t), numread, logfile);
        std::ignore = fread(val1Rec, sizeof(double), numread, logfile);
        std::ignore = fread(val2Rec, sizeof(double), numread, logfile);
        std::ignore = fread(val3Rec, sizeof(double), numread, logfile);  
    } else {
        /* new-style one record at a time */
        for (int i = 0; i < numRecords; i++) {
            std::ignore = fread(&timeRec[i], sizeof(double), 1, logfile);
            std::ignore = fread(&typeRec[i], sizeof(uint32_t), 1, logfile);
            std::ignore = fread(&idRec[i],   sizeof(uint32_t), 1, logfile);
            std::ignore = fread(&evRec[i],   sizeof(uint32_t), 1, logfile);
            std::ignore = fread(&val1Rec[i], sizeof(double), 1, logfile);
            std::ignore = fread(&val2Rec[i], sizeof(double), 1, logfile);
            std::ignore = fread(&val3Rec[i], sizeof(double), 1, logfile);  
        }
    }

    //type=mtcp
    //ev=rate
    //group by ID

    //lets compute 
    hashmap<int, double> flow_rates;
    hashmap<int, double> flow_count;

    hashmap<int, double> flow_rates2;
    hashmap<int, double> flow_count2;

    int TYPE = 11, EV = 1100;

    if (argc>2&&!strcmp(argv[2], "-memory")) {
        TYPE = 12; EV = 1204;
    } else if (argc>2&&!strcmp(argv[2], "-tcp")) {
        TYPE = 11; EV = 1100;
    } else if (argc>2&&!strcmp(argv[2], "-tcp_cwnd")) {
        TYPE = 2; EV = 200;
    } else if (argc>2&&!strcmp(argv[2], "-tcp_seq")) {
        TYPE = 2; EV = 201;
    } else if (argc>2&&!strcmp(argv[2], "-ndp")) {
        TYPE = 18; EV = 1800;
    } else if (argc>2&&!strcmp(argv[2], "-roce")) {
        TYPE = 26; EV = 2600;
    } else if (argc>2&&!strcmp(argv[2], "-eqds")) {
        TYPE = 41; EV = 4100;
    } else if (argc>2&&!strcmp(argv[2], "-hpcc")) {
        TYPE = 28; EV = 2800;
    } else if (argc>2&&!strcmp(argv[2], "-swift")) {
        TYPE = 23; EV = 2300;
    } else if (argc>2&&!strcmp(argv[2], "-mptcp")) {
        TYPE = 12; EV = 1203;
    } else if (argc>2&&!strcmp(argv[2], "-mptcp-cwnd")) {
        TYPE = 12; EV = 1202;
    } else if (argc>2&&!strcmp(argv[2], "-mptcp-alfa")) {
        TYPE = 12; EV = 1200;
    } else if (argc>2&&!strcmp(argv[2], "-queue")) {
        TYPE = 5; EV = 500;
    } else if (argc>2&&!strcmp(argv[2], "-queue-verbose")) {
        TYPE = 0; EV = 0;
    } else if (argc>2&&!strcmp(argv[2], "-all")) {
        TYPE = -1; EV = -1;
    }

    for (int i=0;i<numRecords;i++){
        if (!timeRec[i]) {
            continue;
        }

        if (ascii) {
            RawLogEvent event(timeRec[i], typeRec[i], idRec[i], evRec[i], 
                              val1Rec[i], val2Rec[i], val3Rec[i],object_names[idRec[i]]);
            //cout << Logger::event_to_str(event) << endl;
            string out;
            //cout << "ev type: " << (Logger::EventType)typeRec[i] << endl;
            switch((Logger::EventType)typeRec[i]) {
            case Logger::QUEUE_EVENT: //0
                out = QueueLoggerSimple::event_to_str(event);
                break;
            case Logger::TCP_EVENT: //1
            case Logger::TCP_STATE: //2
                out = TcpLoggerSimple::event_to_str(event); 
                break;
            case Logger::TRAFFIC_EVENT: //3
                out = TrafficLoggerSimple::event_to_str(event); 
                break;
            case Logger::QUEUE_RECORD: //4
            case Logger::QUEUE_APPROX: //5
                out = QueueLoggerSampling::event_to_str(event);
                break;
            case Logger::TCP_RECORD: //6
                out = AggregateTcpLogger::event_to_str(event);
                break;
            case Logger::QCN_EVENT: //7
            case Logger::QCNQUEUE_EVENT: //8
                out = QcnLoggerSimple::event_to_str(event);
                break;
            case Logger::TCP_TRAFFIC: //9
                out = TcpTrafficLogger::event_to_str(event);
                break;
            case Logger::NDP_TRAFFIC: //10
                out = NdpTrafficLogger::event_to_str(event);
                break;
            case Logger::ROCE_TRAFFIC: //10
                out = RoceTrafficLogger::event_to_str(event);
                break;
                break;                
            case Logger::HPCC_TRAFFIC: //10
                out = HPCCTrafficLogger::event_to_str(event);
                break;
            case Logger::TCP_SINK: //11
                out = TcpSinkLoggerSampling::event_to_str(event);
                break;
            case Logger::MTCP: //12
                out = MultipathTcpLoggerSimple::event_to_str(event);
                break;
            case Logger::ENERGY: //13
                // not currently used, so use default logger
                out = Logger::event_to_str(event);
                break;
            case Logger::TCP_MEMORY: //14
                out = MemoryLoggerSampling::event_to_str(event);
                break;
            case Logger::NDP_EVENT: //15
            case Logger::NDP_STATE: //16
            case Logger::NDP_RECORD: //17
            case Logger::NDP_MEMORY: //19
                // not currently used, so use default logger
                out = Logger::event_to_str(event);
                break;
            case Logger::EQDS_EVENT: 
            case Logger::EQDS_STATE: 
            case Logger::EQDS_RECORD:
            case Logger::EQDS_MEMORY:
            case Logger::EQDS_TRAFFIC:
                // not currently used, so use default logger
                out = Logger::event_to_str(event);
                break;
            case Logger::NDP_SINK: //18
                out = NdpSinkLoggerSampling::event_to_str(event);
                break;
            case Logger::EQDS_SINK: //18
                out = EqdsSinkLoggerSampling::event_to_str(event);
                break;
            case Logger::ROCE_SINK: //18
                out = RoceSinkLoggerSampling::event_to_str(event);
                break;
            case Logger::HPCC_SINK: //18
                out = HPCCSinkLoggerSampling::event_to_str(event);
                break;                
            case Logger::SWIFT_EVENT: //20
            case Logger::SWIFT_STATE: //21
                out = SwiftLoggerSimple::event_to_str(event); 
                break;
            case Logger::SWIFT_MEMORY: //22
                // not currently used, so use default logger
                out = Logger::event_to_str(event);
                break;
            case Logger::SWIFT_SINK: //23
                out = SwiftSinkLoggerSampling::event_to_str(event);
                out += " ";
                out.append(to_string(typeRec[i]));
                out += " ";
                out.append(to_string(evRec[i]));
                break;
            case Logger::SWIFT_TRAFFIC: //10
                out = SwiftTrafficLogger::event_to_str(event);
                break;
            case Logger::STRACK_EVENT:
            case Logger::STRACK_STATE: 
                out = STrackLoggerSimple::event_to_str(event); 
                break;
            case Logger::STRACK_MEMORY: //22
                // not currently used, so use default logger
                out = Logger::event_to_str(event);
                break;
            case Logger::STRACK_SINK: //23
                out = STrackSinkLoggerSampling::event_to_str(event);
                out += " ";
                out.append(to_string(typeRec[i]));
                out += " ";
                out.append(to_string(evRec[i]));
                break;
            case Logger::STRACK_TRAFFIC:
                out = STrackTrafficLogger::event_to_str(event);
                break;
            case Logger::FLOW_EVENT:
                out = FlowEventLoggerSimple::event_to_str(event);
                break;
            }
            bool do_output = true;
            for (size_t f=0; f < filters.size(); f++) {
                size_t pos = out.find(filters[f]);
                if (pos == string::npos) {
                    // not found
                    do_output = false;
                    break;
                }
            }
            if (do_output) {
                if (fields.size() > 0) {
                    stringstream out2(ios_base::out);
                    //out2.str(std::string());
                    std::istringstream iss(out);
                    string item;
                    int inum = 0;
                    while (std::getline(iss, item, ' ')) {
                        for (vector<int>::const_iterator fi = fields.begin(); fi != fields.end(); fi++) {
                            if (inum == *fi) {
                                out2 << item << " ";
                            }
                        }
                        inum++;
                    }
                    cout << out2.str() << endl;
                } else {
                    cout << out << endl;
                }
            }
        } else {
            if ((typeRec[i]==(uint32_t)TYPE || TYPE==-1) 
                && (evRec[i]==(uint32_t)EV || EV==-1)) {
                if (verbose)
                    cout << timeRec[i] << " Type=" << typeRec[i] << " EV=" << evRec[i] 
                         << " ID=" << idRec[i] << " VAL1=" << val1Rec[i] 
                         << " VAL2=" << val2Rec[i] << " VAL3=" << val3Rec[i] << endl;        

                if (!isnan((long double)val3Rec[i])) {
                    if (flow_rates.find(idRec[i]) == flow_rates.end()){
                        flow_rates[idRec[i]] = val3Rec[i];
                        flow_count[idRec[i]] = 1;
                    } else {
                        flow_rates[idRec[i]] += val3Rec[i];
                        flow_count[idRec[i]]++;
                    }
                }

                if (!isnan((long double)val2Rec[i])) {
                    if (flow_rates2.find(idRec[i]) == flow_rates2.end()) {
                        flow_rates2[idRec[i]] = val2Rec[i];
                        flow_count2[idRec[i]] = 1;
                    } else {
                        flow_rates2[idRec[i]]+= val2Rec[i];
                        flow_count2[idRec[i]]++;
                    }
                }
            }
        }
    }
    if (ascii) {
        exit(0);
    }

    //now print rates;
    vector<double> rates;

    double mean_rate2 = 0;
    hashmap <int, double> :: iterator it2;
    it2 = flow_rates2.begin ( );
    while (it2!=flow_rates2.end()){
        int id = it2->first;
        double r = it2->second/flow_count2[id];
        mean_rate2 += r;
        it2++;
    }

    hashmap <int, double> :: iterator it;
    it = flow_rates.begin ( );
    double mean_rate = 0;
    while (it!=flow_rates.end()){
        int id = it->first;
        //cout << "Flow with ID " << id << " has mean rate " << it->second/flow_count[id] << endl;
        double r = it->second/flow_count[id];
        //printf("%f %d\n", r, id);
        mean_rate += r;
        rates.push_back(r);

        if (show)
            printf("%.2f Mbps val %d name %s\n", r*8/1000000,id,object_names[id].c_str());

        it++;
    }

    std::sort(rates.begin(), rates.end());
  
    double total = 0;
    int cnt = rates.size()/10;
    int nn = rates.size();
    if (cnt<1) cnt = 1;

    for (int i=0;i<nn;i++){
        if (i<cnt)
            total += rates[i]; 

        if (i>cnt&&!show)
            break;
    }
    printf("Mean of lower 10pc (%d entries) is %f Mbps total mean %f Mbps mean2 %f Mbps\n", 
           cnt, (total/cnt)*8/1000000, mean_rate/rates.size()*8/1000000, 
           mean_rate2/flow_rates2.size()*8/1000000);
  
    delete[] timeRec;
    delete[] typeRec;
    delete[] idRec;
    delete[] evRec;
    delete[] val1Rec;
    delete[] val2Rec;
    delete[] line;
}
