#include <iostream>
#include <map>
#include "rtpconfig.h"
#include "utils.h"
#include "jitterbuffer.h"

using namespace std;

class SingleFrame {
public:
	SingleFrame() {
		fec_k = 10000;
	}

	void add_packet(FecPacket& packet) {
		m[packet.get_block_no()] = &packet;
		fec_k = packet.get_fec_k();
	}

	bool frame_ready() {
		return m.size() >= fec_k;
	}

	std::map<uint16_t, char*> getFrameMap() {
		std::map<uint16_t, char*> ret;
		for (auto& pair : m) {
			FecPacket* pack = pair.second;
			std::cout << "first is " << pair.first << std::endl;
			ret[pair.first] = pack->get_block();
		}
		return ret;
	}

private:
	std::map<uint16_t, FecPacket*> m;
	uint16_t fec_k;
};

JitterBuffer::JitterBuffer() { }

void JitterBuffer::clear_frame(uint16_t frame_no) {
	m.erase(frame_no);
}

void JitterBuffer::add_packet(uint16_t frame_no, FecPacket& packet) {
	if (m.count(frame_no) == 0) {
		m[frame_no] = new SingleFrame();
	}
	m[frame_no]->add_packet(packet);
}

bool JitterBuffer::frame_ready(uint16_t frame_no) {
	return m.count(frame_no) > 0 ? m[frame_no]->frame_ready() : false;
}

std::map<uint16_t, char*> JitterBuffer::getFrameMap(uint16_t frame_no) {
	if (!m[frame_no]->frame_ready()) {
		throw "Frame not ready";
	}
	return m[frame_no]->getFrameMap();
}

