#ifndef __METHODS_H__ // accorgimento per evitare inclusioni multiple di un header
#define __METHODS_H__

#include <sys/socket.h>
#include "common.h"

// prototipi dei metodi definiti in msg_queue.c
void    initialize_queue();
void    enqueue(const char *nickname, const char *msg);
msg_t*  dequeue();

// prototipi dei metodi definiti in send_recv.c
void    send_msg(int socket, const char *msg);
size_t  recv_msg(session_thread_args_t* args, char *buf, size_t buf_len);

// prototipi dei metodi definiti in util.c
int     parse_join_msg(char* msg, size_t msg_len, char* nickname);
int     user_joining(int socket, const char *nickname, struct sockaddr_in* address);
int     user_leaving(int socket);
void    send_msg_by_server(int socket, const char *msg);
void    broadcast(msg_t* msg);
void    end_chat_session_for_closed_socket(session_thread_args_t* args);
void    end_chat_session_for_closed_socket_bis(int socket, char *address);
void    end_chat_session(session_thread_args_t* args, const char *msg);
void    end_chat_session_bis(int socket, char *address, const char *msg); 
void    send_help(int socket);
void    send_list(int socket);
void    send_stats(int socket);
void 	*send_create(session_thread_args_t* args, char *chat_name, char *nickname); 
void	send_destroy(int socket, char* chat_name);
void	send_leave(int socket, const char *nickname, unsigned int descriptor);
void     send_join(session_thread_args_t* args, const char *nickname, unsigned int descriptor);
char    **str_split(char* a_str, const char a_delim);
void    send_channels(int socket);
void gestione_timeout();
#endif
