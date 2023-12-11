// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <sys/types.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sstream>
double drand();


#ifdef _WIN32
// Ways to refer to integer types
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef signed __int64 sint64_t;
#else
typedef long long sint64_t;
#endif

// Specify units for simulation time, link speed, buffer capacity
typedef uint64_t simtime_picosec;

int pareto(int xm, int mean);
double exponential(double lambda);

simtime_picosec timeFromSec(double secs);
simtime_picosec timeFromMs(double msecs);
simtime_picosec timeFromMs(int msecs);
simtime_picosec timeFromUs(double usecs);
simtime_picosec timeFromUs(uint32_t usecs);
simtime_picosec timeFromNs(double nsecs);
double timeAsMs(simtime_picosec ps);
double timeAsUs(simtime_picosec ps);
double timeAsNs(simtime_picosec ps);
double timeAsSec(simtime_picosec ps);
typedef sint64_t mem_b; // memory in bytes (prefer over int for anything measured in bytes)
mem_b memFromPkt(double pkts);

typedef uint64_t linkspeed_bps;
linkspeed_bps speedFromGbps(double Gbitps);
linkspeed_bps speedFromMbps(uint64_t Mbitps);
linkspeed_bps speedFromMbps(double Mbitps);
linkspeed_bps speedFromKbps(uint64_t Kbitps);
linkspeed_bps speedFromPktps(double packetsPerSec);
double speedAsPktps(linkspeed_bps bps);
double speedAsGbps(linkspeed_bps bps);
typedef int mem_pkts; // memory in packets (prefer over int for anything that counts packets)

typedef uint32_t addr_t;
typedef uint16_t port_t;

std::string ntoa(double n);
std::string itoa(uint64_t n);

class Route; 
void print_path(std::iostream &paths,const Route* rt);

// Gumph
#if defined (__cplusplus) && !defined(__STL_NO_NAMESPACES)
using namespace std;
#endif

#ifdef _WIN32
#define max(a,b) (((a)>(b))?(a):(b))
#define min(a,b) (((a)<(b))?(a):(b))
#endif

#endif
