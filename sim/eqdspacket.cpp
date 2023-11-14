#include "eqdspacket.h"

PacketDB<EqdsDataPacket> EqdsDataPacket::_packetdb;
PacketDB<EqdsAckPacket> EqdsAckPacket::_packetdb;
PacketDB<EqdsNackPacket> EqdsNackPacket::_packetdb;
PacketDB<EqdsPullPacket> EqdsPullPacket::_packetdb;
PacketDB<EqdsRtsPacket> EqdsRtsPacket::_packetdb;
