#include <iostream>
#include <map>
#include "rtppacket.h"
#include "rtpconfig.h"
#include "utils.h"

using namespace jrtplib;
using namespace std;

class MemDeleter {
	public:
		virtual void DeletePacket(RTPPacket* p) = 0;
};

class SingleFrame {
public:
	SingleFrame(MemDeleter* deleter) : deleter(deleter) {
		fec_k = 10000;
	}
	~SingleFrame() {
		for (auto& pair : m) {
			deleter->DeletePacket(pair.second);
		}
	}

	void add_packet(uint16_t block_no, RTPPacket& packet) {
		m[block_no] = &packet;
		PacketPayload* payload = (PacketPayload*)packet.GetPayloadData();
		fec_k = payload->fec_k;
	}

	bool frame_ready() {
		return m.size() >= fec_k;
	}

	std::map<uint16_t, char*> getFrameMap(uint16_t frame_no) {
		std::map<uint16_t, char*> ret;
		for (auto& pair : m) {
			const RTPPacket* pack = pair.second;
			PacketPayload* payload = (PacketPayload*)pack->GetPayloadData();
			ret[payload->fec_block_no] = payload->data;
		}
		return ret;
	}

private:
	MemDeleter* deleter;
	std::map<uint16_t, RTPPacket*> m;
	uint16_t fec_k;
};

class JitterBuffer {
public:
	JitterBuffer(MemDeleter* deleter) :
		deleter(deleter) { }

	void clear_frame(uint16_t frame_no) {
		m.erase(frame_no);
	}

	void add_packet(uint16_t frame_no, uint16_t block_no, RTPPacket& packet) {
		if (m.count(frame_no) == 0) {
			m[frame_no] = new SingleFrame(deleter);
		}
		m[frame_no]->add_packet(block_no, packet);
	}

	bool frame_ready(uint16_t frame_no) {
		return m.count(frame_no) > 0 ? m[frame_no]->frame_ready() : false;
	}

	std::map<uint16_t, char*> getFrameMap(uint16_t frame_no) {
		if (!m[frame_no]->frame_ready()) {
			throw "Frame not ready";
		}
		return m[frame_no]->getFrameMap(frame_no);
	}

private:
	MemDeleter* deleter;
	std::map<uint16_t, SingleFrame*> m;
};
