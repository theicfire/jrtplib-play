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
#include <sys/time.h>
#include <unistd.h>

using namespace jrtplib;
using namespace std;

//
// This function checks if there was a RTP error. If so, it displays an error
// message and exists.
//
long getMicrotime(){
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
		exit(-1);
	}
}

//
// The main routine
//

int main(void)
{
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);
#endif // RTP_SOCKETTYPE_WINSOCK
	
	RTPSession sess;
	uint16_t portbase,destport;
	uint32_t destip;
	std::string ipstr;
	int status,i,num;

	std::cout << "Using version " << RTPLibraryVersion::GetVersion().GetVersionString() << std::endl;

	// First, we'll ask for the necessary information
		
	portbase = 2000;
	
	destip = inet_addr("127.0.0.1");
	if (destip == INADDR_NONE)
	{
		std::cerr << "Bad IP address specified" << std::endl;
		return -1;
	}
	
	// The inet_addr function returns a value in network byte order, but
	// we need the IP address in host byte order, so we use a call to
	// ntohl
	destip = ntohl(destip);
	
	destport = 9000;
	
	// Now, we'll create a RTP session, set the destination, send some
	// packets and poll for incoming data.
	
	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;
	
	// IMPORTANT: The local timestamp unit MUST be set, otherwise
	//            RTCP Sender Report info will be calculated wrong
	// In this case, we'll be sending 10 samples each second, so we'll
	// put the timestamp unit to (1.0/10.0)
	sessparams.SetOwnTimestampUnit(1.0/10.0);		
	
	sessparams.SetAcceptOwnPackets(true);
	transparams.SetPortbase(portbase);
	status = sess.Create(sessparams,&transparams);	
	checkerror(status);
	
	RTPIPv4Address addr(destip,destport);
	
	status = sess.AddDestination(addr);
	checkerror(status);

	int count = 0;
	long start = getMicrotime();
	// long max_time_passed = 0;
	printf("Send 100 packets\n");
	for (int j = 0; j < 1000000; j++) {
		char buff[50];
		// long time_passed = getMicrotime() - now;
		// if (time_passed > max_time_passed) {
		// 	printf("max_time_passed at %d is now %lu\n", count, time_passed);
		// 	max_time_passed = time_passed;
		// }
		sprintf(buff, "Thing: %d", count);
		status = sess.SendPacket((void *)buff,50,0,false,10);
		checkerror(status);
		count += 1;

		// Send at 7000 packets/second
		while (getMicrotime() - start < 142 * j) {
			// usleep(1000);
		}
	}
		

	
	sess.BYEDestroy(RTPTime(10,0),0,0);

#ifdef RTP_SOCKETTYPE_WINSOCK
	WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK
	return 0;
}

