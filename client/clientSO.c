#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <winsock2.h>
#include <windows.h>
#include <signal.h>

#include "common.h"

int shouldStop = 0;

void* receiveMessage(void* arg) {
    int socket_desc = (int)arg;

    char close_command[MSG_SIZE];
    sprintf(close_command,"%c%s", COMMAND_CHAR, QUIT_COMMAND);
    size_t close_command_len = strlen(close_command);

    /* select() uses sets of descriptors and a timeval interval. The
     * methods returns when either an event occurs on a descriptor in
     * the sets during the given interval, or when that time elapses.
     *
     * The first argument for select is the maximum descriptor among
     * those in the sets plus one. Note also that both the sets and
     * the timeval argument are modified by the call, so you should
     * reinitialize them across multiple invocations.
     *
     * On success, select() returns the number of descriptors in the
     * given sets for which data may be available, or 0 if the timeout
     * expires before any event occurs. */
    struct timeval timeout;
    fd_set read_descriptors;
    int nfds = socket_desc + 1;

    char buf[MSG_SIZE];
    char nickname[NICKNAME_SIZE];
    char delimiter[2];
    sprintf(delimiter, "%c", MSG_DELIMITER_CHAR);

    while (!shouldStop) {
        int ret;

        // check every 1.5 seconds 
        timeout.tv_sec  = 1;
        timeout.tv_usec = 500000;

        FD_ZERO(&read_descriptors);
        FD_SET(socket_desc, &read_descriptors);

        /** perform select() **/
        ret = select(nfds, &read_descriptors, NULL, NULL, &timeout);
        if (ret == -1 && errno == EINTR) continue;
		if (ret < 0) perror("Unable to select()");
        //ERROR_HELPER(ret, "Unable to select()");

        if (ret == 0) continue; // timeout expired

        // ret is 1: read available data!
        int read_completed = 0;
        int read_bytes = 0; // index for writing into the buffer
        int bytes_left = MSG_SIZE - 1; // number of bytes to (possibly) read
        while(!read_completed) {
            ret = recv(socket_desc, buf + read_bytes, 1, 0);
            if (ret == 0) break;
            if (ret == -1 && errno == EINTR) continue;
			if (ret < 0) perror("Errore nella lettura da socket");
            //ERROR_HELPER(ret, "Errore nella lettura da socket");
            bytes_left -= ret;
            read_bytes += ret;
            read_completed = bytes_left == 0 || buf[read_bytes - 1] == '\n';
        }

        if (ret == 0) {
            shouldStop = 1;
        } else {
            buf[read_bytes - 1] = '\0';
            int sender_length = strcspn(buf, delimiter);
            if (sender_length == strlen(buf)) {
                printf("[???] %s\n", buf);
            } else {
                snprintf(nickname, sender_length + 1, "%s", buf);
                printf("[%s] %s\n", nickname, buf + sender_length + 1);
            }
        }
    }

	ExitThread(1);

}

void* sendMessage(void* arg) {
    int socket_desc = (int)arg;

    char close_command[MSG_SIZE];
    sprintf(close_command, "%c%s\n", COMMAND_CHAR, QUIT_COMMAND);
    size_t close_command_len = strlen(close_command);

    char buf[MSG_SIZE];

    while (!shouldStop) {
        int ret;

        /* Read a line from stdin: fgets() reads up to sizeof(buf)-1
         * bytes and on success returns the first argument passed. */
        if (fgets(buf, sizeof(buf), stdin) != (char*)buf) {
            fprintf(stderr, "Error while reading from stdin, exiting...\n");
            exit(EXIT_FAILURE);
        }

        if (shouldStop) break; // the endpoint might have closed the connection

        size_t msg_len = strlen(buf);

        // send message
        while ( (ret = send(socket_desc, buf, msg_len, 0)) < 0) {
            if (errno == EINTR) continue;
            if (errno == EPIPE) continue;
			if (ret < 0) {
				perror("Cannot write to socket");
			}
        }

        // After a BYE command we should update shouldStop
        if (msg_len == close_command_len && !memcmp(buf, close_command, close_command_len)) {
            shouldStop = 1;
        }
    }

	ExitThread(1);
}

void chat_session(int socket_desc) {
    int ret;

	HANDLE hCThread[2];
	DWORD dwGenericThread;

	hCThread[0] = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE) receiveMessage, (void*)socket_desc,0,NULL);
	if (hCThread[0] == NULL) perror("Cannot create thread for receiving messages");

	hCThread[1] = CreateThread(NULL, 0,(LPTHREAD_START_ROUTINE) sendMessage, (void*)socket_desc, 0, NULL);
	if (hCThread[1] == NULL) perror("Cannot create thread for sending messages");

	WaitForMultipleObjects(2, hCThread, TRUE, INFINITE);

	CloseHandle(hCThread[0]);
	CloseHandle(hCThread[1]);

    // close socket
	ret = closesocket(socket_desc);
	if (ret < 0) perror("Cannot close socket");
	WSACleanup(); //terminates use of the Winsock 2 DLL (Ws2_32.dll).
}

void syntax_error(char* prog_name) {
    fprintf(stderr, "Usage: %s <IP_address> <port_number> <nickname>\n", prog_name);
    exit(EXIT_FAILURE);
}

void consoleHandler(int sig) {

	signal(sig, SIG_IGN);
	shouldStop = 1;
	char  c;

	printf("\n OUCH, did you hit Ctrl-C?\n"
		"Do you really want to quit? [y/n] ");
	c = getchar();
	if (c == 'y' || c == 'Y')
		exit(0);
	else {
		signal(sig, consoleHandler);
		shouldStop = 0;
	}
	//getchar(); // Get new line character
}
/*
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
		// Handle the CTRL-C signal. 
	case CTRL_C_EVENT:
		printf("Ctrl-C event\n\n");
		Beep(750, 300);
		return(TRUE);

		// CTRL-CLOSE: confirm that the user wants to exit. 
	case CTRL_CLOSE_EVENT:
		Beep(600, 200);
		printf("Ctrl-Close event\n\n");
		return(TRUE);

		// Pass other signals to the next handler. 
	case CTRL_BREAK_EVENT:
		Beep(900, 200);
		printf("Ctrl-Break event\n\n");
		return FALSE;

	case CTRL_LOGOFF_EVENT:
		Beep(1000, 200);
		printf("Ctrl-Logoff event\n\n");
		return FALSE;

	case CTRL_SHUTDOWN_EVENT:
		Beep(750, 500);
		printf("Ctrl-Shutdown event\n\n");
		return FALSE;

	default:
		return FALSE;
	}
}*/

int main(int argc, char* argv[]) {
    if (argc == 4) {
		/*
		if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)consoleHandler, TRUE)) {
			printf("\nERROR: Could not set control handler");
			return -1;
		}*/
		signal(CTRL_C_EVENT, SIG_IGN);

        // we use network byte order
		uint32_t ip_addr;
        unsigned short port_number_no;
        char nickname[NICKNAME_SIZE];

        // retrieve IP address
        ip_addr = inet_addr(argv[1]); 

		//checking error for function inet_addr
		if (ip_addr == INADDR_NONE) {
			printf("inet_addr failed and returned INADDR_NONE\n");
			WSACleanup();
			return -1;
		}

		if (ip_addr == INADDR_ANY) {
			printf("inet_addr failed and returned INADDR_ANY\n");
			WSACleanup();
			return -1;
		}

        // retrieve port number
        long tmp = strtol(argv[2], NULL, 0); // safer than atoi()
        if (tmp < 1024 || tmp > 49151) {
            fprintf(stderr, "Please use a port number between 1024 and 49151.\n");
            exit(EXIT_FAILURE);
        }
        port_number_no = htons((unsigned short)tmp);

        sprintf(nickname, "%s", argv[3]);

        int ret;
        struct sockaddr_in server_addr = {0}; // some fields are required to be filled with 0
		
        WSADATA wsaData; //contains information about the Windows Sockets implementation.
	    // Initialize Winsock
    	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData); //returns a pointer to the WSADATA structure in the lpWSAData parameter.
    	if (iResult != 0) { //the first param is the version we want to load
        	printf("WSAStartup failed with error: %d\n", iResult);
        	return 1;
    	}
    	SOCKET s; //create socket
    	if((s = socket(AF_INET , SOCK_STREAM , 0 )) == INVALID_SOCKET)
    	   printf("Could not create socket : %d" , WSAGetLastError());
	
        // set up parameters for the connection
        server_addr.sin_addr.s_addr = ip_addr;
        server_addr.sin_family      = AF_INET;
        server_addr.sin_port        = port_number_no;

        //Connect to remote server
		if (connect(s , (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)) == SOCKET_ERROR){
			closesocket(s);
			perror("connect error");
		}
        
        // invio comando di join
        char buf[MSG_SIZE];
        sprintf(buf, "%c%s %s\n", COMMAND_CHAR, JOIN_COMMAND, nickname);
        int msg_len = strlen(buf);
        while ( (ret = send(s, buf, msg_len, 0)) < 0) {
            if (errno == EINTR) continue;
            if( ret < 0) perror("Cannot write to socket");
        }

        chat_session(s);
    } else {
        syntax_error(argv[0]);
    }

    exit(EXIT_SUCCESS);
}
