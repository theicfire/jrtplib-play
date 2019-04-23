#include <iostream>
#include <map>
#include "rtpconfig.h"
#include "utils.h"
#include "jitterbuffer.h"

using namespace std;
using namespace jrtplib;
using fecpp::byte;

class SingleFrame {
public:
	SingleFrame() {
		fec_k = 10000;
	}

	~SingleFrame() {
		cout << "destruct singleframe" << endl;
	}

	void clear(Deleter& d) {
		for (auto& pair: m) {
			//printf("d %d\n", pair.first);
			d.deletePacket(pair.second);
		}
	}

	void add_packet(RTPPacket& packet) {
		PacketPayload* payload = (PacketPayload*) packet.GetPayloadData();
		//cout << "p" << payload->fec_block_no << "=" << &packet << endl;
		m[payload->fec_block_no] = &packet;
		fec_k = payload->fec_k;
	}

	bool frame_ready() {
		return m.size() >= fec_k;
	}

	uint16_t get_fec_k() {
		return fec_k;
	}

	uint32_t size() {
		return m.size();
	}

	std::map<size_t, const byte*> getFrameMap() {
		std::map<size_t, const byte*> ret;
		for (auto& pair : m) {
			RTPPacket* pack = pair.second;
			PacketPayload* payload = (PacketPayload*) pack->GetPayloadData();
			//std::cout << "first is " << pair.first << std::endl;
			ret[pair.first] = (byte*) payload->data;
		}
		return ret;
	}

private:
	std::map<uint16_t, RTPPacket*> m;
	uint16_t fec_k;
};

JitterBuffer::JitterBuffer() { }

void JitterBuffer::clear_frame(Deleter& d, uint8_t frame_no) {
	// Delete the past 20 frames, in case they weren't full
	for (uint8_t j = 0; j < 20; j++) {
		uint8_t current_frame_no = frame_no - j;
		if (m.count(current_frame_no) > 0) {
			printf("delete frame %d\n", frame_no);
			m[current_frame_no]->clear(d);
			delete m[current_frame_no];
			m.erase(current_frame_no);
		}
	}
}

void JitterBuffer::add_packet(uint8_t frame_no, RTPPacket& packet) {
	if (m.count(frame_no) == 0) {
		m[frame_no] = new SingleFrame();
	}
	m[frame_no]->add_packet(packet);
}

int JitterBuffer::next_frame_ready() {
	bool found = false;
	uint8_t max_frame_ready = 0;
	uint8_t min_frame_ready = 0xFF;

	for (auto& pair : m) {
		if (pair.second->frame_ready()) {
			found = true;
			if (pair.first < min_frame_ready) {
				min_frame_ready = pair.first;
			}
			if (pair.first > max_frame_ready) {
				max_frame_ready = pair.first;
			}
		}
	}
	if (!found) {
		return -1;
	}

	// Out of the range of ready frames, return the earliest one, assuming frame numbers increase over time.
	bool frames_wrapped = max_frame_ready - min_frame_ready > 128;
	if (frames_wrapped) {
		return max_frame_ready;
	}
	return min_frame_ready;
}

uint16_t JitterBuffer::get_fec_k(uint8_t frame_no) {
	if (!m[frame_no]->frame_ready()) {
		throw "Frame not ready";
	}
	return m[frame_no]->get_fec_k();

}

uint32_t JitterBuffer::total_size() {
	uint32_t size = 0;
	for (auto& pair : m) {
		size += pair.second->size();
	}
	return size;
}

std::map<size_t, const byte*> JitterBuffer::getFrameMap(uint8_t frame_no) {
	if (!m[frame_no]->frame_ready()) {
		throw "Frame not ready";
	}
	return m[frame_no]->getFrameMap();
}

