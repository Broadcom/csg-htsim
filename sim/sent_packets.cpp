// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#include "sent_packets.h"
#include <iostream>

SentPackets::SentPackets(int max){
    size = max;
    crt_start = 0;
    crt_end = 0;
    crt_count = 0;
  
    sub_seq = new uint64_t[max];
    data_seq = new uint64_t[max];

    highest_seq = 0;
}

void SentPackets::add_packet(uint64_t seq, uint64_t data_s){
    sub_seq[crt_end] = seq;
    data_seq[crt_end] = data_s;

    highest_seq = seq;

    crt_end = (crt_end+1)%size;
    crt_count ++;

    assert(crt_start!=crt_end);
}

int SentPackets::ack_packet(uint64_t ack_seq){
    int acked = 0;
    while (crt_start!=crt_end && crt_count>0){
        if (sub_seq[crt_start]<ack_seq){
            crt_start = (crt_start+1)%size;
            crt_count --;
            acked++;
        }
        else break;
    }
    return acked;
}

int SentPackets::get_data_seq(uint64_t seq, uint64_t* dseq){
    int t = crt_start;
    while (t!=crt_end){
        if (sub_seq[t]==seq){
            *dseq = data_seq[t];
            return 1;
        }
        t = (t+1)%size;
    }

    cout << "Didn't find packet in sent list! Seq No: " << seq << " First Sent " << sub_seq[crt_start] << "[" << data_seq[crt_start] <<"] Last Sent " << sub_seq[crt_end-1] << "["<< data_seq[crt_end-1] << "] start pos " << crt_start << " end pos " << crt_end << " count " << crt_count <<endl;
    return 0;
}

int SentPackets::has_data_seq(uint64_t seq){
    int t = crt_start;
    while (t!=crt_end){
        if (data_seq[t]==seq){
            return 1;
        }
        t = (t+1)%size;
    }
    return 0;
}

int SentPackets::have_mapping(int seq){
    return highest_seq > seq;
}
