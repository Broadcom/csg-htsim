// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#include "network.h"

#define DEFAULTDATASIZE 1500
int Packet::_data_packet_size = DEFAULTDATASIZE;
bool Packet::_packet_size_fixed = false;
PacketFlow Packet::_defaultFlow(nullptr);

// use set_attrs only when we want to do a late binding of the route -
// otherwise use set_route or set_rg
void 
Packet::set_attrs(PacketFlow& flow, int pkt_size, packetid_t id){
    _flow = &flow;
    _size = pkt_size;
    _oldsize = pkt_size;
    _id = id;
    _nexthop = 0;
    _oldnexthop = 0;
    //_detour = NULL;
    _route = 0;
    _is_header = 0;
    _flags = 0;
    _next_routed_hop = 0;
}

void 
Packet::set_route(PacketFlow& flow, const Route &route, int pkt_size, 
                  packetid_t id){
    _flow = &flow;
    _size = pkt_size;
    _oldsize = pkt_size;
    _id = id;
    _nexthop = 0;
    _oldnexthop = 0;    
    //_detour = NULL;
    _route = &route;
    _is_header = 0;
    _flags = 0;
}

void 
Packet::set_route(const Route &route){
    _route = &route;
    _nexthop = 0;
    //_detour = NULL;
}

void 
Packet::set_route(const Route *route){
    _route = route;
    _nexthop = 0;
    //_detour = NULL;
}

PacketSink *
Packet::sendOn() {
    PacketSink* nextsink;

    /*if (_detour){
        nextsink = _detour;
        _detour = NULL;
        }else*/
    
    if (_route) {
        if (_bounced) {
            assert(_nexthop > 0);
            assert(_nexthop < _route->size());
            assert(_nexthop < _route->reverse()->size());
            //assert(_route->size() == _route->reverse()->size());
            nextsink = _route->reverse()->at(_nexthop);
            _nexthop++;
        } else {
            assert(_nexthop<_route->size());
            nextsink = _route->at(_nexthop);
            _nexthop++;
        }
    } else if (_next_routed_hop)
        nextsink = _next_routed_hop;
    else {
        assert(0);
    }
    //cout << "sendOn nextsink is: " << nextsink->nodename() << endl;
    nextsink->receivePacket(*this);
    return nextsink;
}

PacketSink *
Packet::sendOn2(VirtualQueue* crtSink) {
    PacketSink* nextsink;
    if (_route) {
        if (_bounced) {
            assert(_nexthop > 0);
            assert(_nexthop < _route->size());
            assert(_nexthop < _route->reverse()->size());
            //assert(_route->size() == _route->reverse()->size());
            nextsink = _route->reverse()->at(_nexthop);
            _nexthop++;
        } else {
            assert(_nexthop<_route->size());
            nextsink = _route->at(_nexthop);
            _nexthop++;
        }
    } else if (_next_routed_hop)
        nextsink = _next_routed_hop;
    else {
        assert(0);
    }
    nextsink->receivePacket(*this,crtSink);
    return nextsink;
}

// AKA, return to sender
void 
Packet::bounce() { 
    assert(!_bounced); 
    assert(_route); // we only implement return-to-sender on regular routes
    _bounced = true; 
    _is_header = true;
    _nexthop = _route->size() - _nexthop;
    //    _nexthop--;
    // we're now going to use the _route in reverse. The alternative
    // would be to modify the route, but all packets travelling the
    // same route share a single Route, and we won't want have to
    // allocate routes on a per packet basis.
}

void 
Packet::unbounce(uint16_t pktsize) { 
    assert(_bounced); 
    assert(_route); // we only implement return-to-sender on regular
    // routes, not route graphs. If we go back to using
    // route graphs at some, we'll need to fix this, but
    // for now we're not using them.

    // clear the packet for retransmission
    _bounced = false; 
    _is_header = false;
    _size = pktsize;
    _nexthop = 0;
}

void 
Packet::free() {
}

string
Packet::str() const {
    string s;
    switch (_type) {
    case IP:
        s = "IP";
        break;
    case TCP:
        s = "TCP";
        break;
    case TCPACK:
        s = "TCPACK";
        break;
    case SWIFT:
        s = "SWIFT";
        break;
    case SWIFTACK:
        s = "SWIFTACK";
        break;
    case STRACK:
        s = "SWIFT";
        break;
    case STRACKACK:
        s = "SWIFTACK";
        break;
    case TCPNACK:
        s = "TCPNACK";
        break;
    case NDP:
        s = "NDP";
        break;
    case NDPACK:
        s = "NDPACK";
        break;
    case NDPNACK:
        s = "NDPNACK";
        break;
    case NDPPULL:
        s = "NDPPULL";
        break;
    case NDPRTS:
        s = "NDPRTS";
        break;        
    case NDPLITE:
        s = "NDPLITE";
        break;
    case NDPLITEACK:
        s = "NDPLITEACK";
        break;
    case NDPLITERTS:
        s = "NDPLITERTS";
        break;
    case NDPLITEPULL:
        s = "NDPLITEPULL";
        break;
    case ETH_PAUSE:
        s = "ETHPAUSE";
        break;
    case TOFINO_TRIM:
        s = "TofinoTrimPacket";
        break;        
    case ROCE:
        s = "ROCE";
        break;
    case ROCEACK:
        s = "ROCEACK";
        break;
    case ROCENACK:
        s = "ROCENACK";
        break;
    case HPCC:
        s = "HPCC";
        break;
    case HPCCACK:
        s = "HPCCACK";
        break;
    case HPCCNACK:
        s = "HPCCNACK";
        break;
    case EQDSDATA:
        s = "EQDSDATA";
        break;
    case EQDSNACK:
        s = "EQDSNACK";
        break;
    case EQDSACK:
        s = "EQDSACK";
        break;
    case EQDSPULL:
        s = "EQDSPULL";
        break;
    case EQDSRTS:
        s = "EQDSRTS";
        break;
    }
    return s;
}

// flow ids above this are dynamically allocated; ones less than this can be manually allocated
#define FLOW_ID_DYNAMIC_BASE 1000000000
flowid_t PacketFlow::_max_flow_id = FLOW_ID_DYNAMIC_BASE;

PacketFlow::PacketFlow(TrafficLogger* logger)
    : Logged("PacketFlow"),
      _logger(logger)
{
    _flow_id = _max_flow_id++;
}

void PacketFlow::set_flowid(flowid_t id) {
    if (id >= FLOW_ID_DYNAMIC_BASE) {
        cerr << "Illegal flow ID - manually allocation must be less than dynamic base\n";
        assert(0);
    }
    _flow_id = id;
}

void PacketFlow::set_logger(TrafficLogger *logger) {
    _logger = logger;
}

void 
PacketFlow::logTraffic(Packet& pkt, Logged& location, TrafficLogger::TrafficEvent ev) {
    if (_logger)
        _logger->logTraffic(pkt, location, ev);
}

void print_route(const Route& route) {
    for (size_t i = 0; i < route.size(); i++) {
        PacketSink* sink = route.at(i);
        if (i > 0) 
            cout << " -> ";
        cout << sink->nodename();
    }
    cout << endl;
}

Logged::id_t Logged::LASTIDNUM = 1;
