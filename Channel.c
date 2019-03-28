#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "winsock2.h"
#include <ws2tcpip.h>
#include <math.h>
#include <windows.h>
#include <process.h>

#pragma comment(lib, "Ws2_32.lib")

#define UDP_BUFF 64

int recvfromTimeOutUDP(SOCKET socket, long sec, long usec);
void Init_Winsock();
void flip_bits(char chnl_buff_1[], double err_prob, int *flipped_cnt);
int send_frame(char buff[], int fd, struct sockaddr_in to_addr, int bytes_to_write);
int receive_frame(char buff[], int fd, int bytes_to_read, struct sockaddr_in *recv_addr, struct sockaddr_in *sender_addr);

int END_FLAG = 0;
int SelectTiming;

int main(int argc, char** argv) {
	Init_Winsock();

	if (argc != 6) {
		printf("Error: not enough arguments were provided!\n");
		exit(1);
	}

	unsigned int local_port = (unsigned int)strtoul(argv[1], NULL, 10);
	char* recv_ip_add = argv[2];
	unsigned int recv_port = (unsigned int)strtoul(argv[3], NULL, 10);
	double err_prob = (double) (strtoul(argv[4], NULL, 10)*pow(2, -16));
	int rand_seed = (int) strtoul(argv[5], NULL, 10);
	int s_fd = -1, flipped_cnt = 0;
	char chnl_buff[UDP_BUFF];
	struct sockaddr_in chnl_addr, recv_addr, sender_addr;

	srand(rand_seed);
	
	socklen_t addrsize = sizeof(struct sockaddr_in);
	//channel - receiver - sender socket
	if ((s_fd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}
	//channel address
	memset(&chnl_addr, 0, addrsize);
	chnl_addr.sin_family = AF_INET;
	chnl_addr.sin_addr.s_addr = htonl(INADDR_ANY);	// INADDR_ANY = any local machine address
	chnl_addr.sin_port = htons(local_port);
	//sender sddress. other feilds determined in receive frame
	memset(&sender_addr, 0, addrsize);
	//receiver address
	memset(&recv_addr, 0, sizeof(recv_addr));
	recv_addr.sin_family = AF_INET;
	recv_addr.sin_port = htons(recv_port);
	recv_addr.sin_addr.s_addr = inet_addr(recv_ip_add);

	
	if (bind(s_fd, (SOCKADDR *)&chnl_addr, addrsize) != 0) {
		fprintf(stderr, "Bind failed. exiting...\n");
		exit(1);
	}

	SelectTiming = recvfromTimeOutUDP(s_fd, 100000, 0);
	while (receive_frame(chnl_buff, s_fd, UDP_BUFF, &recv_addr, &sender_addr) == 0 && END_FLAG == 0) {
		flip_bits(chnl_buff, err_prob, &flipped_cnt);//manipulate flipping on received bits, change in place in chnl_buff
		send_frame(chnl_buff, s_fd, recv_addr, UDP_BUFF);//send to receiver
		SelectTiming = recvfromTimeOutUDP(s_fd, 100000, 0);
	}
	printf("out of while \n");
	/*if (shutdown(s_fd, SD_RECEIVE) != 0) {
		fprintf(stderr, "socket shutdowm failed. exiting...\n");
		exit(1);
	}*/
	send_frame(chnl_buff, s_fd, sender_addr, UDP_BUFF); //write back to sender

	char ip_str_1[20] = { 0 }; char ip_str_2[20] = { 0 };
	inet_ntop(AF_INET, &(sender_addr.sin_addr.s_addr), ip_str_1, 20);
	inet_ntop(AF_INET, &(recv_addr.sin_addr.s_addr), ip_str_2, 20);
	printf("sender: %s\nreceiver: %s\n%d bytes, flipped %d bits\n",
		ip_str_1, ip_str_2, ((int*)chnl_buff)[0], flipped_cnt);

	if (closesocket(s_fd) != 0) {
		fprintf(stderr, "%s\n", strerror(errno));
	}
	WSACleanup();
	return 0;
}


void Init_Winsock() {
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		printf("Error at WSAStartup()\n");
		exit(1);
	}
}


void flip_bits(char chnl_buff_1[], double err_prob, int *flipped_cnt) {

	int i, j, flip;
	double r;
	char mask, tmp;

	for (i = 0; i < UDP_BUFF; i++) {
		tmp = 1;
		mask = 0;
		for (j = 0; j < 8; j++) {
			r = (rand() % 101) / 100.0; // rand num [0,1]
			flip = r < err_prob; // 0 -not flip, 1- flip
			if (flip) {
				(*flipped_cnt)++;
				mask = mask | tmp;
			}
			tmp <<= 1; //left shift by 1 position
		}
		chnl_buff_1[i] ^= mask;
	}

	return;
}


int send_frame(char buff[], int fd, struct sockaddr_in to_addr, int bytes_to_write) {
	int totalsent = 0, num_sent = 0;

	while (bytes_to_write > 0) {
		num_sent = sendto(fd, buff + totalsent , bytes_to_write, 0, (SOCKADDR*)&to_addr, sizeof(to_addr));
		if (num_sent == -1) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		totalsent += num_sent;
		bytes_to_write -= num_sent;
	}
	return 0;
}


int receive_frame(char buff[], int fd, int bytes_to_read, struct sockaddr_in *recv_addr, struct sockaddr_in *sender_addr){
	int totalread = 0, bytes_been_read = 0, addrsize;
	struct sockaddr_in from_addr;

	memset(buff, '\0', UDP_BUFF);
	while (totalread < bytes_to_read && END_FLAG == 0  && SelectTiming > 0) {
		printf("in start of while \n");
		addrsize = sizeof(from_addr);
		bytes_been_read = recvfrom(fd, buff + totalread, bytes_to_read, 0,(struct sockaddr*) &from_addr, &addrsize);
		printf("bytes_been_read: %d \n", bytes_been_read);
		if (bytes_been_read < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		totalread += bytes_been_read;
		printf("before comparison\n");
		if (from_addr.sin_addr.s_addr == recv_addr->sin_addr.s_addr
			&& from_addr.sin_port == recv_addr->sin_port) { // got from receiver
			printf("in comparison\n");
			END_FLAG = 1;
		}
		else {
			memcpy(sender_addr, &from_addr, addrsize);
		}
		printf("after comparesion \n");
	}
	return 0;
}


int recvfromTimeOutUDP(SOCKET socket, long sec, long usec)
{

	// Setup timeval variable

	struct timeval timeout;

	struct fd_set fds;



	timeout.tv_sec = sec;

	timeout.tv_usec = usec;

	// Setup fd_set structure

	FD_ZERO(&fds);

	FD_SET(socket, &fds);

	// Return value:

	// -1: error occurred

	// 0: timed out

	// > 0: data ready to be read

	return select(0, &fds, 0, 0, &timeout);

}