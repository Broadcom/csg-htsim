#include "ndppacket.h"

PacketDB<NdpPacket> NdpPacket::_packetdb;
PacketDB<NdpAck> NdpAck::_packetdb;
PacketDB<NdpNack> NdpNack::_packetdb;
PacketDB<NdpPull> NdpPull::_packetdb;
PacketDB<NdpRTS> NdpRTS::_packetdb;
