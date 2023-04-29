// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#ifndef SENT_PACKETS
#define SENT_PACKETS
#include "config.h"

class SentPackets {
public:
    int size;
    int crt_count;
    int crt_start;
    int crt_end;
    int highest_seq;

    uint64_t* sub_seq;
    uint64_t* data_seq;
 
    SentPackets(int max=20000);

    int have_mapping(int seq);
  
    void add_packet(uint64_t seq, uint64_t data_seq);
    int ack_packet(uint64_t ack_seq);

    int get_data_seq(uint64_t seq,uint64_t* dseq);
    int has_data_seq(uint64_t dseq);
};


#endif
