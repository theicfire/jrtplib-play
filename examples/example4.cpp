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


JMutex mutex;
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

class save_to_map
   {
   public:
      save_to_map(size_t& share_len_arg,
                  std::map<size_t, const byte*>& m_arg) :
         share_len(share_len_arg), m(m_arg) {}

      void operator()(size_t block_no, size_t,
                      const byte share[], size_t len)
         {
         share_len = len;

         // Contents of share[] are only valid in this scope, must copy
         byte* share_copy = new byte[share_len];
         memcpy(share_copy, share, share_len);
         m[block_no] = share_copy;
         }
   private:
      size_t& share_len;
      std::map<size_t, const byte*>& m;
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

void benchmark_fec(size_t k, size_t n)
   {

   fecpp::fec_code fec(k, n);

   std::vector<byte> input(k * 1300);
   for(size_t i = 0; i != input.size(); ++i)
      input[i] = i;

   std::map<size_t, const byte*> shares;
   size_t share_len;

   save_to_map saver(share_len, shares);

   fec.encode(&input[0], input.size(), std::tr1::ref(saver));

   while(shares.size() > k)
      shares.erase(shares.begin());

   output_checker check_output;
   fec.decode(shares, share_len, check_output);
   }


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

//
// The new class routine
//

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
				//DeletePacket(pack);
			}
		} while (GotoNextSourceWithData());
	}
		
	EndDataAccess();
}

long now = getMicroseconds();
long max_time_passed = 0;
void MyRTPSession::ProcessRTPPacket(const RTPSourceData &srcdat,RTPPacket &rtppack)
{
   PacketPayload* payload = (PacketPayload*) rtppack.GetPayloadData();
   //printf("Adding packet of frame_no %d %d\n", payload->frame_no, payload->fec_block_no);
   mutex.Lock();
   jb.add_packet(payload->frame_no, rtppack);
   mutex.Unlock();
}

//
// The main routine
// 
int main(void)
{
	 mutex.Init();
	printf("most current ex4\n");
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);
#endif // RTP_SOCKETTYPE_WINSOCK
	
	JitterBuffer jb;
	MyRTPSession sess(jb);
	GeneralDeleter deleter(sess);
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
	
	uint8_t frame_no = 0;
	while (true) {
	   mutex.Lock();
	   if (jb.frame_ready(frame_no)) {
	      printf("Got a frame %d and fec_k %d\n", frame_no, jb.get_fec_k(frame_no));
	      auto frame_map = jb.getFrameMap(frame_no);
	      output_checker check_output;
	      fecpp::fec_code fec(jb.get_fec_k(frame_no), 255); // TODO the N should also come in the packets...
	      std::cout << "Now decode " << std::endl;
	      mutex.Unlock();
	      fec.decode(frame_map, 1300, check_output);
	      mutex.Lock();
	      std::cout << "Done decoding" << std::endl;

	      jb.clear_frame(deleter, frame_no);
	      frame_no += 1;
	      mutex.Unlock();
	   }
	   mutex.Unlock();
	}
	//Wait a number of seconds
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

