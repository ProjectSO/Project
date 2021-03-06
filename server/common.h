#ifndef __COMMON_H__ // accorgimento per evitare inclusioni multiple di un header
#define __COMMON_H__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <assert.h>

#define GENERIC_ERROR_HELPER(cond, errCode, msg) do {               \
        if (cond) {                                                 \
            fprintf(stderr, "%s: %s\n", msg, strerror(errCode));    \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
    } while(0)

#define ERROR_HELPER(ret, msg)          GENERIC_ERROR_HELPER((ret < 0), errno, msg)
#define PTHREAD_ERROR_HELPER(ret, msg)  GENERIC_ERROR_HELPER((ret != 0), ret, msg)

// parametri di configurazione dei messaggi
#define MSG_SIZE            1024
#define NICKNAME_SIZE       128
#define CHANNEL_NAME_SIZE	128
#define MAX_USERS           128
#define MSG_BUFFER_SIZE 	256
#define TIMEOUT				1800

// struttura dati per i messaggi
typedef struct msg_s {
    char    nickname[NICKNAME_SIZE];
    char    msg[MSG_SIZE];
} msg_t;

// struttura dati per i thread chat_session()
typedef struct session_thread_args_s {
    int socket;
    struct sockaddr_in* address;
} session_thread_args_t;

// struttura dati per gli utenti
typedef struct user_data_s {
    int     socket;
    char    nickname[NICKNAME_SIZE];
    char    address[INET_ADDRSTRLEN];
    uint16_t    port;
    unsigned int sent_msgs;
    unsigned int rcvd_msgs;
    unsigned int quit;
    unsigned int channel;
} user_data_t;

typedef struct channel_s {
	char name[CHANNEL_NAME_SIZE];
	unsigned int descriptor;
	struct user_data_s* users_list[MAX_USERS];
	unsigned int num;
} channel_t;

// altri parametri di configurazione del server
#define MAX_CONN_QUEUE      3
#define LOG                 1
#define SERVER_NICKNAME     "chatroom"
#define GLOBAL_CHAT         "global chat"
#define MAX_CHANNEL 		99

// codici interni di errore
#define NICKNAME_NOT_AVAILABLE  -10
#define TOO_MANY_USERS          -11
#define USER_NOT_FOUND          -12

// gestione dei messaggi
#define MSG_DELIMITER_CHAR  '|'
#define COMMAND_CHAR        '#'

// ogni comando inizia per #
#define JOIN_COMMAND        "join"
#define QUIT_COMMAND        "quit"
#define LIST_COMMAND        "list"
#define STATS_COMMAND       "stats"
#define HELP_COMMAND        "help"
#define CREATE_COMMAND		"create"
#define JOINCHANNEL_COMMAND	"<channel_number>"
#define DESTROY_COMMAND		"destroy"
#define LEAVE_COMMAND		"leave"
#define CHANNELS_COMMAND	"channels"

#endif
