// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef LOGFILE_H
#define LOGFILE_H

/*
 * Logfile is a class for specifying the log file format.
 * The loggers (loggers.h) face both
 *  1. the log file, using the base class Logger (defined here)
 *  2. the simulator, using the base classes in loggertypes.h
 */

#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include "config.h"
#include "network.h"
#include "eventlist.h"

class Logfile;
class Logger;

class RawLogEvent {
 public:
    RawLogEvent(double time, uint32_t type, uint32_t id, uint32_t ev, 
                double val1, double val2, double val3,string name);
    virtual string str();
    double _time;
    uint32_t _type;
    uint32_t _id;
    uint32_t _ev;
    double _val1; 
    double _val2; 
    double _val3;
    string _name;
};

class Logfile {
 public:
    Logfile(const string& filename, EventList& eventlist);
    ~Logfile();
    void setStartTime(simtime_picosec starttime);
    void write(const string& msg);
    void writeName(Logged& logged);
    void writeRecord(uint32_t type, uint32_t id, uint32_t ev, 
                     double val1, double val2, double val3); // prepend uint64_t time
    void addLogger(Logger& logger);
    simtime_picosec _starttime;
 private:
    EventList& _eventlist;
    vector<Logger*> _loggers;
    // managing the files for writing
    void transposeLog();
    stringstream _preamble;
    string _logfilename;
    FILE* _logfile;
    //bool _startedTrace;
    long int _numRecords;
};

#endif
