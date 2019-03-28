#define _CRT_SECURE_NO_WARNINGS
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

void Init_Winsock();
void flip_bits(char chnl_buff_1[], double err_prob);
int send_frame(char buff[], int fd, struct sockaddr_in to_addr, int bytes_to_write);
int receive_frame(char buff[], int fd, int bytes_to_read, struct sockaddr_in *recv_addr, struct sockaddr_in *sender_addr);
struct sockaddr_in get_sender_ip(int sockfd);

int END_FLAG = 0;

int main(int argc, char** argv) {
	Init_Winsock();

	if (argc != 6) {
		printf("Error: not enough arguments were provided!\n");
		exit(1);
	}

	unsigned int local_port = (unsigned int)strtoul(argv[1], NULL, 10);
	unsigned int recv_ip_add = (unsigned int)strtoul(argv[2], NULL, 10);
	unsigned int recv_port = (unsigned int)strtoul(argv[3], NULL, 10);
	double err_prob = (unsigned int)strtoul(argv[4], NULL, 10)*pow(2, -16);
	unsigned int rand_seed = (unsigned int)strtoul(argv[5], NULL, 10);
	int s_fd = -1;
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
	memset(&recv_addr, 0, addrsize);
	recv_addr.sin_family = AF_INET;
	recv_addr.sin_addr.s_addr = htonl(recv_ip_add);
	recv_addr.sin_port = htons(recv_port);

	
	if (bind(s_fd, (SOCKADDR *)&chnl_addr, addrsize) != 0) {
		fprintf(stderr, "Bind failed. exiting...\n");
		exit(1);
	}

	while (receive_frame(chnl_buff, s_fd, UDP_BUFF, &recv_addr, &sender_addr) == 0 && END_FLAG == 0) {
		flip_bits(chnl_buff, err_prob);//manipulate flipping on received bits, change in place in chnl_buff
		send_frame(chnl_buff, s_fd, recv_addr, UDP_BUFF);//send to receiver
	}

	send_frame(chnl_buff, s_fd, sender_addr, UDP_BUFF); //write back to sender

	if (closesocket(s_fd) != 0) {
		fprintf(stderr, "%s\n", strerror(errno));
	}
	WSACleanup();
	return 0;
}

struct sockaddr_in get_sender_ip(int sockfd) {
	struct sockaddr_in addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	int res = getpeername(sockfd, (struct sockaddr *)&addr, &addr_size);
	return addr;
}

void Init_Winsock() {
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		printf("Error at WSAStartup()\n");
		exit(1);
	}
}


void flip_bits(char chnl_buff_1[], double err_prob) {

	int i, j, flip;
	double r;
	char mask, tmp;

	for (i = 0; i < UDP_BUFF; i++) {
		tmp = 1;
		mask = 0;
		for (j = 0; j < 8; j++) {
			r = (rand() % 101) / 100; // rand num [0,1]
			flip = r < err_prob; // 0 -not flip, 1- flip
			if (flip) {
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
		num_sent = sendto(fd, buff + totalsent, bytes_to_write, 0, (SOCKADDR*)&to_addr, sizeof(to_addr));
		if (num_sent == -1) {
			fprintf(stderr, "%s\n", strerror(errno));
			return -1;
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
	while (totalread < bytes_to_read && END_FLAG == 0) {
		bytes_been_read = recvfrom(fd, buff + totalread, bytes_to_read, 0,(struct sockaddr*) &from_addr, &addrsize);
		if (from_addr.sin_addr.s_addr == recv_addr->sin_addr.s_addr) { // got from receiver
			END_FLAG = 1;
		}
		else {
			memcpy(sender_addr, &from_addr, addrsize);
		}
		if (bytes_been_read < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		totalread += bytes_been_read;
	}
	return 0;
}