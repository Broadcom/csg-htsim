// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef MAIN_H
#define MAIN_H

#include <string>
#include "network.h"


#define HOST_NIC 100000 // host nic speed in Mbps
#define CORE_TO_HOST 4

//basic setup!


#define NI 3        //Number of intermediate switches
#define NA 6        //Number of aggregation switches
#define NT 9        //Number of ToR switches (180 hosts)

#define NS 20        //Number of servers per ToR switch
#define TOR_AGG2(tor) (10*NA - tor - 1)%NA

void check_non_null(Route* rt);


/*
#define NI 4        //Number of intermediate switches
#define NA 9        //Number of aggregation switches
#define NT 18        //Number of ToR switches (180 hosts)

#define NS 10        //Number of servers per ToR switch
#define TOR_AGG2(tor) (tor+1)%NA
*/
/*
#define NI 10        //Number of intermediate switches
#define NA 6        //Number of aggregation switches
#define NT 30        //Number of ToR switches (180 hosts)

#define NS 6        //Number of servers per ToR switch
#define TOR_AGG2(tor) (tor+1)%NA
*/

/*
#define NI 4        //Number of intermediate switches
#define NA 5        //Number of aggregation switches
#define NT 10        //Number of ToR switches (180 hosts)

#define NS 20        //Number of servers per ToR switch
#define TOR_AGG2(tor) (10*NA - tor - 1)%NA
*/

/*#define NI 2        //Number of intermediate switches
#define NA 25        //Number of aggregation switches
#define NT 25        //Number of ToR switches (200 hosts)

#define NS 8        //Number of servers per ToR switch
#define TOR_AGG2(tor) (tor+1)%NA*/


/*//This is 40Gb/s in the core, 10Gb/s in the access; remember to change core_to_host above
#define NI 5        //Number of intermediate switches
#define NA 10        //Number of aggregation switches
#define NT 25        //Number of ToR switches (200 hosts)

#define NS 8        //Number of servers per ToR switch
#define TOR_AGG2(tor) (tor+5)%NA
*/

//oversubscribed VL2, 40Gb/s core

/*#define NI 4
#define NA 8
#define NT 16
#define NS 32
#define TOR_AGG2(tor) (tor+1)%NA
*/

//oversubscribed VL2
/*
#define NI 3
#define NA 4
#define NT 6
#define NS 80
#define TOR_AGG2(tor) (tor+2)%NA
*/

/*#define NI 3
#define NA 4
#define NT 6
#define NS 20
#define TOR_AGG2(tor) (tor+2)%NA
*/



#define SWITCH_BUFFER 97
#define RANDOM_BUFFER 3
#define FEEDER_BUFFER 1000

#endif
