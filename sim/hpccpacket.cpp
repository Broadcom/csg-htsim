// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#include "hpccpacket.h"

PacketDB<HPCCPacket> HPCCPacket::_packetdb;
PacketDB<HPCCAck> HPCCAck::_packetdb;
PacketDB<HPCCNack> HPCCNack::_packetdb;

void HPCCAck::copy_int_info(IntEntry* info, int cnt){
    for (int i = 0;i<cnt;i++)
        _int_info[i] = info[i];
        
    _int_hop = cnt;
};
