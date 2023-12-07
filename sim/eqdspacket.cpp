#include "eqdspacket.h"

PacketDB<EqdsDataPacket> EqdsDataPacket::_packetdb;
PacketDB<EqdsAckPacket> EqdsAckPacket::_packetdb;
PacketDB<EqdsNackPacket> EqdsNackPacket::_packetdb;
PacketDB<EqdsPullPacket> EqdsPullPacket::_packetdb;
PacketDB<EqdsRtsPacket> EqdsRtsPacket::_packetdb;

EqdsBasePacket::pull_quanta
EqdsBasePacket::quantize_floor(mem_b bytes) {
  return bytes >> PULL_SHIFT;
}

EqdsBasePacket::pull_quanta
EqdsBasePacket::quantize_ceil(mem_b bytes) {
  return (bytes + PULL_QUANTUM - 1) / PULL_QUANTUM;
}

mem_b
EqdsBasePacket::unquantize(EqdsBasePacket::pull_quanta credit_chunks) {
  return credit_chunks << PULL_SHIFT;
}
