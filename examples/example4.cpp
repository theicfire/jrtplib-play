/*
   This IPv4 example uses the background thread itself to process all packets.
   You can use example one to send data to the session that's created in this
   example.
*/

#include "assert.h"
#include "rtpsession.h"
#include "rtppacket.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtpsourcedata.h"
#include "rtpconfig.h"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include "fecpp.h"
#include "jitterbuffer.h"

using namespace jrtplib;
using namespace jthread;
static int count = 0;

#ifdef RTP_SUPPORT_THREAD
long getMicroseconds(){
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}


using fecpp::byte;
std::map<size_t, const byte*> mymap;

class output_checker
   {
   public:
      void operator()(size_t block, size_t /*max_blocks*/,
                      const byte buf[], size_t size)
         {
         for(size_t i = 0; i != size; ++i)
            {
            byte expected = block*size + i;
            if(buf[i] != expected) {
               printf("block=%d i=%d got=%02X expected=%02X\n",
                      (int)block, (int)i, buf[i], expected);
	    }

            }
         }
   };

class GeneralDeleter : public Deleter {
public:
   GeneralDeleter(RTPSession& sess): sess(sess) {}
   void deletePacket(RTPPacket* pack) {
      sess.DeletePacket(pack);
   }
private:
      RTPSession& sess;
};

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

class MyRTPSession : public RTPSession
{
public:
     MyRTPSession(JitterBuffer& jb) : jb(jb) { }
protected:
	void OnPollThreadStep();
	void ProcessRTPPacket(const RTPSourceData &srcdat,RTPPacket &rtppack);
private:
	JitterBuffer& jb;
};

void MyRTPSession::OnPollThreadStep()
{
	BeginDataAccess();
		
	// check incoming packets
	if (GotoFirstSourceWithData())
	{
		do
		{
			RTPPacket *pack;
			RTPSourceData *srcdat;
			
			srcdat = GetCurrentSourceInfo();
			
			while ((pack = GetNextPacket()) != NULL)
			{
				ProcessRTPPacket(*srcdat,*pack);
			}
		} while (GotoNextSourceWithData());
	}
		
	EndDataAccess();
}

JitterBuffer jb;
MyRTPSession sess(jb);
GeneralDeleter deleter(sess);
long now = getMicroseconds();
long max_time_passed = 0;
void MyRTPSession::ProcessRTPPacket(const RTPSourceData &srcdat,RTPPacket &rtppack)
{
   PacketPayload* payload = (PacketPayload*) rtppack.GetPayloadData();
   //printf("Adding packet of frame_no %d %d\n", payload->frame_no, payload->fec_block_no);
   jb.add_packet(payload->frame_no, rtppack);

  ////printf("m\n");
  int next_frame_no = jb.next_frame_ready();
  if (next_frame_no != -1) {
     printf("Got a frame %d\n", next_frame_no);
     //printf("Got a frame %d and fec_k %d\n", next_frame_no, jb.get_fec_k(next_frame_no));
     auto frame_map = jb.getFrameMap(next_frame_no);
     output_checker check_output;
     fecpp::fec_code fec(jb.get_fec_k(next_frame_no), 110); // TODO the N should also come in the packets...
     fec.decode(frame_map, 1300, check_output);

     jb.clear_frame(deleter, next_frame_no);
  }
}

//
// The main routine
// 
int main(void)
{
	printf("most current ex4\n");
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);
#endif // RTP_SOCKETTYPE_WINSOCK
	
	uint16_t portbase = 9000;
	std::string ipstr;
	int status;
	
	// Now, we'll create a RTP session, set the destination
	// and poll for incoming data.
	
	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;
	
	// IMPORTANT: The local timestamp unit MUST be set, otherwise
	//            RTCP Sender Report info will be calculated wrong
	// In this case, we'll be just use 8000 samples per second.
	sessparams.SetOwnTimestampUnit(1.0/8000.0);		
	
	transparams.SetPortbase(portbase);
	status = sess.Create(sessparams,&transparams);	
	checkerror(status);

	RTPIPv4Address addr(ntohl(inet_addr("54.241.177.60")),9000);
	status = sess.AddDestination(addr);
	checkerror(status);

	printf("Sending hello to the sender\n");
	char buff[50] = "Hello";
	// Have to send multiple packets so that the source isn't ignored for some time. Weird SRTP!
	status = sess.SendPacket((void *)buff,50,0,false,10);
	checkerror(status);
	status = sess.SendPacket((void *)buff,50,0,false,10);
	checkerror(status);
	status = sess.SendPacket((void *)buff,50,0,false,10);
	checkerror(status);
	status = sess.SendPacket((void *)buff,50,0,false,10);
	checkerror(status);
	printf("Done sending\n");

	RTPTime::Wait(RTPTime(10000,0));
	
	sess.BYEDestroy(RTPTime(10,0),0,0);

#ifdef RTP_SOCKETTYPE_WINSOCK
	WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK
	return 0;
}

#else

int main(void)
{
	std::cerr << "Thread support is required for this example" << std::endl;
	return 0;
}

#endif // RTP_SUPPORT_THREAD

