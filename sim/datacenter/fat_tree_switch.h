// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef _FATTREESWITCH_H
#define _FATTREESWITCH_H

#include "switch.h"
#include "callback_pipe.h"
#include <unordered_map>

class FatTreeTopology;

/*
 * Copyright (C) 2013-2014 Universita` di Pisa. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Shamelessly copied from FreeBSD
 */

/* ----- FreeBSD if_bridge hash function ------- */

/*
 * The following hash function is adapted from "Hash Functions" by Bob Jenkins
 * ("Algorithm Alley", Dr. Dobbs Journal, September 1997).
 *
 * http://www.burtleburtle.net/bob/hash/spooky.html
 */

#define MIX(a, b, c)                            \
    do {                                        \
        a -= b; a -= c; a ^= (c >> 13);         \
        b -= c; b -= a; b ^= (a << 8);          \
        c -= a; c -= b; c ^= (b >> 13);         \
        a -= b; a -= c; a ^= (c >> 12);         \
        b -= c; b -= a; b ^= (a << 16);         \
        c -= a; c -= b; c ^= (b >> 5);          \
        a -= b; a -= c; a ^= (c >> 3);          \
        b -= c; b -= a; b ^= (a << 10);         \
        c -= a; c -= b; c ^= (b >> 15);         \
    } while (/*CONSTCOND*/0)

static inline uint32_t freeBSDHash(uint32_t target1, uint32_t target2 = 0, uint32_t target3 = 0)
{
    uint32_t a = 0x9e3779b9, b = 0x9e3779b9, c = 0; // hask key
        
    b += target3;
    c += target2;
    a += target1;        
    MIX(a, b, c);
    return c;
}

#undef MIX

class FlowletInfo {
public:
    uint32_t _egress;
    simtime_picosec _last;

    FlowletInfo(uint32_t egress,simtime_picosec lasttime) {_egress = egress; _last = lasttime;};

};

class FatTreeSwitch : public Switch {
public:
    enum switch_type {
        NONE = 0, TOR = 1, AGG = 2, CORE = 3
    };

    enum routing_strategy {
        NIX = 0, ECMP = 1, ADAPTIVE_ROUTING = 2, ECMP_ADAPTIVE = 3, RR = 4, RR_ECMP = 5
    };

    enum sticky_choices {
        PER_PACKET = 0, PER_FLOWLET = 1
    };

    FatTreeSwitch(EventList& eventlist, string s, switch_type t, uint32_t id,simtime_picosec switch_delay, FatTreeTopology* ft);
  
    virtual void receivePacket(Packet& pkt);
    virtual Route* getNextHop(Packet& pkt, BaseQueue* ingress_port);
    virtual uint32_t getType() {return _type;}

    uint32_t adaptive_route(vector<FibEntry*>* ecmp_set, int8_t (*cmp)(FibEntry*,FibEntry*));
    uint32_t replace_worst_choice(vector<FibEntry*>* ecmp_set, int8_t (*cmp)(FibEntry*,FibEntry*),uint32_t my_choice);
    uint32_t adaptive_route_p2c(vector<FibEntry*>* ecmp_set, int8_t (*cmp)(FibEntry*,FibEntry*));

    static int8_t compare_flow_count(FibEntry* l, FibEntry* r);
    static int8_t compare_pause(FibEntry* l, FibEntry* r);
    static int8_t compare_bandwidth(FibEntry* l, FibEntry* r);
    static int8_t compare_queuesize(FibEntry* l, FibEntry* r);
    static int8_t compare_pqb(FibEntry* l, FibEntry* r);//compare pause,queue, bw.
    static int8_t compare_pq(FibEntry* l, FibEntry* r);//compare pause, queue
    static int8_t compare_pb(FibEntry* l, FibEntry* r);//compare pause, bandwidth
    static int8_t compare_qb(FibEntry* l, FibEntry* r);//compare pause, bandwidth

    static int8_t (*fn)(FibEntry*,FibEntry*);

    virtual void addHostPort(int addr, int flowid, PacketSink* transport);

    virtual void permute_paths(vector<FibEntry*>* uproutes);

    static void set_strategy(routing_strategy s) { assert (_strategy==NIX); _strategy = s; }
    static void set_ar_fraction(uint16_t f) { assert(f>=1);_ar_fraction = f;} 

    static routing_strategy _strategy;
    static uint16_t _ar_fraction;
    static uint16_t _ar_sticky;
    static simtime_picosec _sticky_delta;
    static double _ecn_threshold_fraction;
    static double _speculative_threshold_fraction;
private:
    switch_type _type;
    Pipe* _pipe;
    FatTreeTopology* _ft;
    
    //CAREFUL: can't always have a single FIB for all up destinations when there are failures!
    vector<FibEntry*>* _uproutes;

    unordered_map<uint32_t,FlowletInfo*> _flowlet_maps;

    static unordered_map<BaseQueue*,uint32_t> _port_flow_counts;

    uint32_t _crt_route;
    uint32_t _hash_salt;
    simtime_picosec _last_choice;

    unordered_map<Packet*,bool> _packets;
};

#endif
    
