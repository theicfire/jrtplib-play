/*
   Here's a small IPv4 example: it asks for a portbase and a destination and
   starts sending packets to that destination.
*/

#include "rtpsession.h"
#include "rtppacket.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtplibraryversion.h"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <rtpsourcedata.h>
#include <fecpp.h>
#include "utils.h"

using namespace jrtplib;
using namespace std;
using fecpp::byte;

Timer t;
//
// This function checks if there was a RTP error. If so, it displays an error
// message and exists.
//
void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
		exit(-1);
	}
}


class output_checker
{
public:
	void operator()(size_t block, size_t /*max_blocks*/,
		const byte buf[], size_t size)
	{
		for (size_t i = 0; i != size; ++i)
		{
			byte expected = block * size + i;
			if (buf[i] != expected)
				printf("block=%d i=%d got=%02X expected=%02X\n",
				(int)block, (int)i, buf[i], expected);

		}
	}
};

class save_to_map
{
public:
	save_to_map(uint8_t frame_no, uint8_t fec_k, RTPSession& yay) : frame_no(frame_no), fec_k(fec_k), sess(yay) { }

	void operator()(size_t block_no, size_t,
		const byte share[], size_t len)
	{
		int status;
		PacketPayload payload;
		payload.fec_block_no = block_no;
		memcpy(payload.data, share, len); // TODO may be slow...
		payload.frame_no = frame_no;
		payload.fec_k = fec_k;
		// TODO this void* &payload is a bit sketch.. is the array memory correct?
		//printf("Sending packet that is %zu long\n", sizeof(PacketPayload));
		//printf("sending fec block %d\n", payload.fec_block_no);
		while (t.getElapsedMicroseconds() < 400) {
			// TODO sleep
		}
		t.reset();
		status = sess.SendPacket((void*)& payload, sizeof(PacketPayload), 0, false, 10);
		checkerror(status);
	}
private:
	RTPSession& sess;
	uint8_t frame_no;
	uint8_t fec_k;
};


void send_sample(std::vector<uint8_t>& input, uint8_t frame_no, size_t k, size_t n, RTPSession& sess)
{
	fecpp::fec_code fec(k, n);
	save_to_map saver(frame_no, k, sess);

	fec.encode(&input[0], input.size(), std::tr1::ref(saver));
}


unsigned long long getMicroseconds()
{
	/* Windows */
	FILETIME ft;
	LARGE_INTEGER li;

	/* Get the amount of 100 nano seconds intervals elapsed since January 1, 1601 (UTC) and copy it
	 *   * to a LARGE_INTEGER structure. */
	GetSystemTimeAsFileTime(&ft);
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;

	unsigned long long ret = li.QuadPart;
	ret -= 116444736000000000LL; /* Convert from file time to UNIX epoch time. */
	ret /= 10; /* From 100 nano seconds (10^-7) to 1 microsecond (10^-6) intervals */
	return ret;
}

//
// The main routine
//

int main(void)
{
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSADATA dat;
	WSAStartup(MAKEWORD(2, 2), &dat);
#endif // RTP_SOCKETTYPE_WINSOCK

	RTPSession sess;
	uint16_t portbase;
	std::string ipstr;
	int status, i, num;

	std::cout << "Using version " << RTPLibraryVersion::GetVersion().GetVersionString() << std::endl;

	// First, we'll ask for the necessary information

	portbase = 9000;

	// Now, we'll create a RTP session, set the destination, send some
	// packets and poll for incoming data.

	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;

	// IMPORTANT: The local timestamp unit MUST be set, otherwise
	//            RTCP Sender Report info will be calculated wrong
	// In this case, we'll be sending 10 samples each second, so we'll
	// put the timestamp unit to (1.0/10.0)
	sessparams.SetOwnTimestampUnit(1.0 / 10.0);

	sessparams.SetAcceptOwnPackets(true);
	transparams.SetPortbase(portbase);
	status = sess.Create(sessparams, &transparams);
	checkerror(status);

	bool wait_for_first_packet = true;
	while (wait_for_first_packet) {
		sess.BeginDataAccess();
		if (sess.GotoFirstSource())
		{
			do
			{
				RTPPacket* packet;

				if ((packet = sess.GetNextPacket()) != 0)
				{
					const RTPSourceData* source = sess.GetCurrentSourceInfo();
					const RTPAddress* addr = source->GetRTPDataAddress();
					status = sess.AddDestination(*addr);
					checkerror(status);
					std::cout << "Added the destination!" << std::endl;
					sess.DeletePacket(packet);
					wait_for_first_packet = false;
					break;
				}
			} while (sess.GotoNextSource());
		}
		sess.EndDataAccess();
	}

	uint8_t frame_no = 0;
	size_t k = 100;
	size_t n = 110;
	while (true) {
		printf("Now sending %d\n", frame_no);
		std::vector<byte> input(k * 1300);
		for (size_t i = 0; i != input.size(); ++i) {
			input[i] = i;
		}
		send_sample(input, frame_no, k, n, sess);
		frame_no += 1;
		printf("Waiting 1 second\n");
		//RTPTime::Wait(RTPTime(0, 30000));
	}

	sess.BYEDestroy(RTPTime(10, 0), 0, 0);

#ifdef RTP_SOCKETTYPE_WINSOCK
	WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK
	return 0;
}


