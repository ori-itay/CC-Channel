#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "winsock2.h" 
#include <math.h>

#define BUFF_1 64
#define WB_BUFF 4

void flip_bits(char chnl_buff_1[], double err_prob);

struct ARGS {
	int socket_fd;
	int buff[WB_BUFF];
};

int END_FLAG = 0;

int main(int argc, char** argv) {
	Init_Winsock();


	if (argc != 6) {
		printf("Error: not enough arguments were provided!\n");
		exit(1);
	}

	int local_port = (unsigned int)strtoul(argv[1], NULL, 10);
	int rec_ip_add = (unsigned int)strtoul(argv[2], NULL, 10);
	int recv_port = (unsigned int)strtoul(argv[3], NULL, 10);
	double err_prob = (unsigned int)strtoul(argv[4], NULL, 10)*pow(2,-16);
	unsigned long rand_seed = (unsigned int)strtoul(argv[5], NULL, 10);
	float r;
	int c_s_fd = -1, c_r_fd = -1, i;
	unsigned int bytes_wrote, notwritten, totalsent, num_sent, bytes_read, notread, totalread;
	char chnl_buff[BUFF_1];
	int write_back_buff[WB_BUFF];
	struct sockaddr_in chnl_addr;
	struct sockaddr_in recv_addr;
	struct fd_set sockets_set;

	srand(rand_seed);

	//channel - sender socket
	if ((c_s_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}
	memset(&chnl_addr, 0, sizeof(struct sockaddr_in));
	chnl_addr.sin_family = AF_INET;
	chnl_addr.sin_addr.s_addr = htonl(INADDR_ANY);	// INADDR_ANY = any local machine address
	chnl_addr.sin_port = htonl(local_port);

	if (0 != bind(c_s_fd, (struct sockaddr*) &chnl_addr, sizeof(struct sockaddr_in))) {
		printf("Error : Bind Failed. %s \n", strerror(errno));
		return 1;
	}


	//channel - receiver socket
	if ((c_r_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}
	struct sockaddr_in cnl_addr;
	memset(&cnl_addr, 0, sizeof(cnl_addr));
	cnl_addr.sin_family = AF_INET;
	cnl_addr.sin_port = htons(recv_port); // htons for endiannes
	cnl_addr.sin_addr.s_addr = inet_addr(rec_ip_add);

	struct ARGS thread_args = { c_r_fd , write_back_buff };
	HANDLE thread = CreateThread(NULL, 0, thread_end_listen, &thread_args, 0, NULL);

	while (END_FLAG == 0) {

		notread = BUFF_1;
		bytes_read = 0;
		while (notread > 0) {
			bytes_read = recvfrom(c_s_fd, chnl_buff + bytes_read, BUFF_1, 0, 0, 0);
			if (bytes_read == -1) {
				fprintf(stderr, "%s\n", strerror(errno));
				exit(1);
			}
			totalread += bytes_read;
			notread -= num_sent;
		}
		if (bytes_read <= 0) { break; }

		//manipulate flipping on received bits, change in place in chnl_buff_1
		flip_bits(chnl_buff, err_prob);

		//send to receiver
		notwritten = bytes_read; //curr num of bytes to write
		totalsent = 0;
		while (notwritten > 0) {
			// notwritten = how much left to write ; totalsent = how much written so far ; num_sent = how much written in last write() call
			num_sent = sendto(c_r_fd, chnl_buff + totalsent, notwritten, 0, &cnl_addr, sizeof(cnl_addr));
			if (num_sent == -1) {// check if error occured (server closed connection?)
				fprintf(stderr, "%s\n", strerror(errno));
				exit(1);
			}
			totalsent += num_sent;
			notwritten -= num_sent;
		}
	}

	//write back to sender
	notwritten = WB_BUFF; 
	totalsent = 0;
	while (notwritten > 0) {
		num_sent = sendto(c_s_fd, write_back_buff + totalsent, notwritten, 0, &cnl_addr, sizeof(cnl_addr));
		if (num_sent == -1) {// check if error occured (server closed connection?)
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		totalsent += num_sent;
		notwritten -= num_sent;
	}


	if (closesocket(c_s_fd) != 0) {
		fprintf(stderr, "%s\n", strerror(errno));
	}

	return 0;
}



void flip_bits(char chnl_buff_1[], double err_prob) {

	int i, j, flip, r;
	char mask, tmp;

	for (i = 0; i < BUFF_1; i++) {
		tmp = 1;
		mask = 0;
		for (j = 0; j < 8; j++) {
			r = (rand() % 101) / 100; // rand num [0,1]
			flip = r < err_prob ; // 0 -not flip, 1- flip
			if (flip) {
				mask = mask | tmp;
			}
			tmp <<= 1; //left shift by 1 position
		}
		chnl_buff_1[i] ^= mask;
	}

	return;
}



DWORD WINAPI thread_end_listen(void *param) {

	int status, bytes_read, notread, totalread, num_sent;
	int recv_buff[] = ((struct ARGS*)param)->buff;
	int s_c_fd = ((struct ARGS*)param)->socket_fd;

	while (END_FLAG == 0) {
		notread = WB_BUFF;
		bytes_read = 0;
		while (notread > 0) {
			bytes_read = recvfrom(s_c_fd, recv_buff + bytes_read, WB_BUFF, 0, 0, 0); //deliver fd and buff as parameter or as global vars
			if (bytes_read == -1) {
				fprintf(stderr, "%s\n", strerror(errno));
				_endthread(1);
			}
			totalread += bytes_read;
			notread -= num_sent;
		}
		status = shutdown(s_c_fd, SD_BOTH);
		if (status) {
			printf("Error while closing socket. \n");
			_endthread(1);
		}
		END_FLAG = 1;
	}
	_endthread(0);
}