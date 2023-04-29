// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#define _CRT_SECURE_NO_DEPRECATE  // For Visual Studio: this allows the unsafe operation fopen() without issuing a warning
#include "logfile.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ios>

RawLogEvent::RawLogEvent(double time, uint32_t type, uint32_t id, uint32_t ev, 
                         double val1, double val2, double val3, string name = "") :
    _time(time), _type(type), _id(id), _ev(ev - 100*type), _val1(val1), _val2(val2), _val3(val3),_name(name)
{
}


string RawLogEvent::str() {
    stringstream ss;
    ss << fixed << setprecision(6) << _time;
    ss << " Type=" << _type << " ID=" << _id  << " EV=" << _ev 
       << " VAL1=" << _val1 << " VAL2=" << _val2 << " VAL3=" << _val3;        
    return ss.str();
}


Logfile::Logfile(const string& filename, EventList& eventlist) 
: _starttime(0), _eventlist(eventlist), 
  _preamble(ios_base::out | ios_base::in), 
  _logfilename(filename), _numRecords(0)
{
    _logfile = fopen(_logfilename.c_str(), "wbS");
    if (_logfile==NULL) {
        cerr << "Failed to open logfile " << _logfilename << endl;
        exit(1);
    }
}

Logfile::~Logfile() {
    if (_logfile != NULL) {
        fclose(_logfile);
        transposeLog();
    }
}

void
Logfile::addLogger(Logger& logger) {
    logger.setLogfile(*this);
    _loggers.push_back(&logger);
}


void 
Logfile::write(const string& msg) {
    _preamble << msg << endl;
}

void
Logfile::writeName(Logged& logged) {
    _preamble << ": " << logged.str() << "=" << logged.get_id() << endl;
}

void
Logfile::setStartTime(simtime_picosec starttime) {
    _starttime=starttime;
}

void
Logfile::writeRecord(uint32_t type, uint32_t id, uint32_t ev, 
                     double val1, double val2, double val3) {
    uint64_t time = _eventlist.now();
    if (time<_starttime) return;
    double time_sec = timeAsSec(time);
    fwrite(&time_sec, sizeof(double), 1, _logfile);
    fwrite(&type, sizeof(uint32_t), 1, _logfile);
    fwrite(&id, sizeof(uint32_t), 1, _logfile);
    fwrite(&ev, sizeof(uint32_t), 1, _logfile);
    fwrite(&val1, sizeof(double), 1, _logfile);
    fwrite(&val2, sizeof(double), 1, _logfile);
    fwrite(&val3, sizeof(double), 1, _logfile);
    _numRecords++;
}

void
Logfile::transposeLog() {
    double* timeRec = new double[_numRecords];
    uint32_t* typeRec = new uint32_t[_numRecords];
    uint32_t* idRec = new uint32_t[_numRecords];
    uint32_t* evRec = new uint32_t[_numRecords];
    double* val1Rec = new double[_numRecords];
    double* val2Rec = new double[_numRecords];
    double *val3Rec = new double[_numRecords];
    FILE* logfile;
    logfile = fopen(_logfilename.c_str(),"rbS");
    if (logfile==NULL) {
        cerr << "Failed to open logfile " << _logfilename << endl;
        exit(1);
    }
    size_t read;
    double rd;
    uint32_t ri;
    int numread;
    for (numread=0; numread<_numRecords; numread++) {
        read = fread(&rd, sizeof(double), 1, logfile); if (read<1) break;
        timeRec[numread] = rd;
        read = fread(&ri, sizeof(uint32_t), 1, logfile); if (read<1) break;
        typeRec[numread] = ri;
        read = fread(&ri, sizeof(uint32_t), 1, logfile); if (read<1) break;
        idRec[numread] = ri;
        read = fread(&ri, sizeof(uint32_t), 1, logfile); if (read<1) break;
        evRec[numread] = ri + 100*typeRec[numread];
        read = fread(&rd, sizeof(double), 1, logfile); if (read<1) break;
        val1Rec[numread] = rd;
        read = fread(&rd, sizeof(double), 1, logfile); if (read<1) break;
        val2Rec[numread] = rd;
        read = fread(&rd, sizeof(double), 1, logfile); if (read<1) break;
        val3Rec[numread] = rd;
    }
    fclose(logfile);
    assert(numread==_numRecords);
    _preamble << "# numrecords=" << numread << endl;
    logfile = fopen(_logfilename.c_str(),"wbS");
    if (logfile==0) {
        cerr << "Failed to open logfile " << _logfilename << endl;
        exit(1);
    }
    while (true) {
        if (_preamble.peek()==-1) break;
        char thisLine[1000];
        _preamble.getline(&thisLine[0],1000);
        fputs(thisLine, logfile);
        fputs("\n", logfile);
    }
    fputs("# transpose=0\n",logfile);
    fputs("# TRACE\n",logfile);
    for (int i=0; i < numread; i++) {
        fwrite(&timeRec[i], sizeof(double), 1, logfile);
        fwrite(&typeRec[i], sizeof(uint32_t), 1, logfile);
        fwrite(&idRec[i],   sizeof(uint32_t), 1, logfile);
        fwrite(&evRec[i],   sizeof(uint32_t), 1, logfile);
        fwrite(&val1Rec[i], sizeof(double), 1, logfile);
        fwrite(&val2Rec[i], sizeof(double), 1, logfile);
        fwrite(&val3Rec[i], sizeof(double), 1, logfile);
    }
    fclose(logfile);
    delete[] timeRec;
    delete[] typeRec;
    delete[] idRec;
    delete[] evRec;
    delete[] val1Rec;
    delete[] val2Rec;
}
