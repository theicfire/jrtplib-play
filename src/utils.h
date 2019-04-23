#ifndef UTILS_H
#define UTILS_H

struct PacketPayload {
	uint32_t frame_no;
	uint16_t fec_block_no;
	uint16_t fec_k;
	char data[1300];
};

#endif
