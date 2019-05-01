#include <map>
#include "utils.h"
#include "rtppacket.h"
#include "fecpp.h"

using fecpp::byte;
class SingleFrame;

class Deleter {
	public:
		virtual void deletePacket(jrtplib::RTPPacket* packet) = 0;
};

class JitterBuffer {
public:
	JitterBuffer();

	void clear_frame(Deleter& d, uint8_t frame_no);
	void add_packet(uint8_t frame_no, jrtplib::RTPPacket& packet);
	int next_frame_ready();
	uint16_t get_fec_k(uint8_t frame_no);
	std::map<size_t, const byte*> getFrameMap(uint8_t frame_no);
	uint32_t total_size();

private:
	std::map<uint8_t, SingleFrame*> m;
};
