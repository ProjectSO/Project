#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "common.h"
#include "methods.h"

// variabili globali di altri moduli possono essere "richiamate" tramite extern
extern sem_t user_data_sem;
extern user_data_t* users[];
extern unsigned int current_users;
channel_t* channel[MAX_CHANNEL];
extern unsigned int desc;

/*
 * Processa un messaggio #join ed estrae il nickname in esso specificato
 * scrivendolo nel buffer passato come terzo argomento.
 */
char join_msg_prefix[MSG_SIZE];
size_t join_msg_prefix_len = 0;

int parse_join_msg(char* msg, size_t msg_len, char* nickname) {
    // vogliamo eseguire le operazioni nel blocco una volta sola per efficienza
    if (join_msg_prefix_len == 0) {
        sprintf(join_msg_prefix, "%c%s ", COMMAND_CHAR, JOIN_COMMAND);
        join_msg_prefix_len = strlen(join_msg_prefix);
    }

    if (msg_len > join_msg_prefix_len && !strncmp(msg, join_msg_prefix, join_msg_prefix_len)) {
        sprintf(nickname, "%s", msg + join_msg_prefix_len);
        return 0;
    } else {
        return -1;
    }
}

/*
 * Gestisce il tentativo di accedere alla chatroom da parte di un utente
 * appena connessosi al server. In caso di successo registra l'utente
 * nella struttura dati del server e notifica tutti gli utenti.
 */
int user_joining(int socket, const char *nickname, struct sockaddr_in* address) {
    int ret;

    ret = sem_wait(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_wait su user_data_sem");

    // verifica limite sul massimo numero di utenti
    if (current_users == MAX_USERS) {
        ret = sem_post(&user_data_sem);
        ERROR_HELPER(ret, "Errore nella chiamata sem_post su user_data_sem");
        return TOO_MANY_USERS;
    }

    // verifica disponibilità nickname
    int i;
    for (i = 0; i < current_users; i++)
        if (strcmp(nickname, users[i]->nickname) == 0) {
            ret = sem_post(&user_data_sem);
            ERROR_HELPER(ret, "Errore nella chiamata sem_post su user_data_sem");
            return NICKNAME_NOT_AVAILABLE;
        }

    // creazione nuovo utente
    user_data_t* new_user = (user_data_t*)malloc(sizeof(user_data_t));
    new_user->socket = socket;
    sprintf(new_user->nickname, "%s", nickname);
    inet_ntop(AF_INET, &(address->sin_addr), new_user->address, INET_ADDRSTRLEN);
    new_user->port = ntohs(address->sin_port);
    new_user->sent_msgs = 0,
    new_user->rcvd_msgs = 0;
    new_user->channel = 0;
    users[current_users++] = new_user;

    // notifica la presenza a tutti gli utenti
    char msg[MSG_SIZE];
    sprintf(msg, "L'utente %s e' entrato nella chatroom", new_user->nickname);
    if (LOG) printf("%s\n", msg);
    enqueue(SERVER_NICKNAME, msg); 

    ret = sem_post(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_post su user_data_sem");

    return 0;
}

/*
 * Notifica gli utenti dell'imminente uscita di un utente.
 */
int user_leaving(int socket) {
    int ret, i, j, k;

    ret = sem_wait(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_wait su user_data_sem");
    
    for (i = 0; i < current_users; i++) {
        if (users[i]->socket == socket) {
            char msg[MSG_SIZE];
			
			//verifico se l' utente che sta uscendo ha creato un canale
			
			for(k = 0; k < desc; k++) {
				if(strcmp(channel[k]->name,users[i]->nickname) == 0) 
					send_destroy_bis(users[i]->nickname);
			}
			
			//verifico se l' utente che sta uscendo è in un canale
			
			for(k = 0; k < desc; k++) {
				for (j=0; j < channel[k]->num; j++){
					if(strcmp(channel[k]->users_list[j]->nickname, users[i]->nickname) == 0)
						send_leave_quit(users[i]->socket, users[i]->nickname, k);
				}
			}
			
            // notifica a tutti gli utenti che users[i] sta lasciando la chatroom
            sprintf(msg, "L'utente %s ha lasciato la chatroom", users[i]->nickname);
            if (LOG) printf("%s\n", msg);

            enqueue(SERVER_NICKNAME, msg);

            free(users[i]);
            for (; i < current_users - 1; i++)
                users[i] = users[i+1]; // shift di 1 per tutti gli elementi successivi
            current_users--;

            ret = sem_post(&user_data_sem);
            ERROR_HELPER(ret, "Errore nella chiamata sem_post su user_data_sem");

            return 0;
        }
    }

    ret = sem_post(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_post su user_data_sem");

    return USER_NOT_FOUND;
}

/*
 * Aggiunge al messaggio passato come argomento un prefisso contenente
 * il nickname dell'utente prima di inviarlo sulla socket desiderata.
 */
void send_msg_by_server(int socket, const char *msg) {
    char msg_by_server[MSG_SIZE];
    sprintf(msg_by_server, "%s%c%s", SERVER_NICKNAME, MSG_DELIMITER_CHAR, msg);
    send_msg(socket, msg_by_server);
}

/*
 * Inoltra un messaggio di un utente a tutti gli altri nella chatroom.
 *
 * Nel caso in cui il messaggio sia originato da SERVER_NICKNAME, esso
 * viene inviato a tutti gli utenti connessi.
 */
void broadcast(msg_t* msg) {

    int ret;
    ret = sem_wait(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_wait su user_data_sem");

    int i, j, k, jump = 0;
    char msg_to_send[MSG_SIZE];
    sprintf(msg_to_send, "%s%c%s", msg->nickname, MSG_DELIMITER_CHAR, msg->msg);
	
	for(i=0;i<desc;i++) {
		for(j=0;j<channel[i]->num;j++) {
			if(strcmp(msg->nickname,channel[i]->users_list[j]->nickname) == 0) {
				printf("nome: %s, channel: %d\n", msg->nickname, channel[i]->users_list[j]->channel);
				jump = 1; // l' utente è in un canale
				for(k=0;k<channel[i]->num;k++) {
					//inoltra il messaggio nel canale dell'utente
					if (strcmp(msg->nickname, channel[i]->users_list[k]->nickname) != 0) {
						send_msg(channel[i]->users_list[k]->socket, msg_to_send);
						channel[i]->users_list[k]->rcvd_msgs++;
					} else if(strcmp(msg->nickname, channel[i]->users_list[k]->nickname) == 0){
						channel[i]->users_list[k]->sent_msgs++;
					}
				}
			}
		}
	}
    
    if(!jump){ //inoltra il messaggio nella global chat
		for (i = 0; i < current_users; i++) {
			if (users[i]->channel == 0){
				if (strcmp(msg->nickname, users[i]->nickname) != 0){
					send_msg(users[i]->socket, msg_to_send);
					users[i]->rcvd_msgs++;
				}
			} else if (strcmp(msg->nickname, users[i]->nickname) == 0)
				users[i]->sent_msgs++;
		}
    }
	

    ret = sem_post(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_post su user_data_sem");
}
 
/*
 * Chiude i descrittori aperti e libera la memoria per conto del metodo
 * end_chat_session(). Il suffisso "_for_closed_socket" indica che esso
 * può essere usato anche nel caso in cui il client abbia chiuso la
 * sua connessione in modo inatteso per il server.
 */
void end_chat_session_for_closed_socket(session_thread_args_t* args) {
    int ret = close(args->socket);
    ERROR_HELPER(ret, "Errore nella chiusura di una socket");
    free(args->address);
    free(args);
    pthread_exit(NULL);
}

/*
 * Invia un messaggio di chiusura al client, e procede chiudendo i
 * descrittori aperti e liberando la memoria per conto di chat_session().
 */
void end_chat_session(session_thread_args_t* args, const char *msg) {
    send_msg_by_server(args->socket, msg);
    end_chat_session_for_closed_socket(args);
}

/*
 * Eseguito in risposta ad un comando #help.
 */
void send_help(int socket) {
    char msg[MSG_SIZE];

    sprintf(msg, "Oltre ai messaggi da condividere con gli altri utenti, e' possibile inviare "
                    "dei comandi che verranno visualizzati ed interpretati solo dal server.");
    send_msg_by_server(socket, msg);

    sprintf(msg, "La lista dei comandi disponibili e' la seguente:");
    send_msg_by_server(socket, msg);

    sprintf(msg, "\t%c%s: stampa la lista degli utenti correntemente connessi", COMMAND_CHAR, LIST_COMMAND);
    send_msg_by_server(socket, msg);

    sprintf(msg, "\t%c%s: termina la sessione", COMMAND_CHAR, QUIT_COMMAND);
    send_msg_by_server(socket, msg);

    sprintf(msg, "\t%c%s: stampa alcune statistiche sulla sessione corrente", COMMAND_CHAR, STATS_COMMAND);
    send_msg_by_server(socket, msg);

    sprintf(msg, "\t%c%s: mostra nuovamente la lista dei comandi disponibili", COMMAND_CHAR, HELP_COMMAND);
    send_msg_by_server(socket, msg);
    
    sprintf(msg, "\t%c%s: crea un canale privato", COMMAND_CHAR, CREATE_COMMAND);
    send_msg_by_server(socket, msg);

    sprintf(msg, "\t%c%s: permette di entrare in un canale privato", COMMAND_CHAR, JOINCHANNEL_COMMAND);
    send_msg_by_server(socket, msg);

    sprintf(msg, "\t%c%s: stampa la lista dei canali esistenti", COMMAND_CHAR, CHANNELS_COMMAND);
    send_msg_by_server(socket, msg);
        
    sprintf(msg, "Un messaggio che inizia per %c viene sempre interpretato come comando.", COMMAND_CHAR);
    send_msg_by_server(socket, msg);
}

/*
 * Eseguito in risposta ad un comando #help quando l'utente ha creato un canale
 */

void send_help_channel(int socket) {
    char msg[MSG_SIZE];

    sprintf(msg, "Oltre ai messaggi da condividere con gli altri utenti, e' possibile inviare "
                    "dei comandi che verranno visualizzati ed interpretati solo dal server.");
    send_msg_by_server(socket, msg);

    sprintf(msg, "La lista dei comandi disponibili e' la seguente:");
    send_msg_by_server(socket, msg);

    sprintf(msg, "\t%c%s: stampa la lista degli utenti correntemente connessi", COMMAND_CHAR, LIST_COMMAND);
    send_msg_by_server(socket, msg);

    sprintf(msg, "\t%c%s: termina la sessione", COMMAND_CHAR, QUIT_COMMAND);
    send_msg_by_server(socket, msg);

    sprintf(msg, "\t%c%s: stampa alcune statistiche sulla sessione corrente", COMMAND_CHAR, STATS_COMMAND);
    send_msg_by_server(socket, msg);

    sprintf(msg, "\t%c%s: mostra nuovamente la lista dei comandi disponibili", COMMAND_CHAR, HELP_COMMAND);
    send_msg_by_server(socket, msg);
    
    sprintf(msg, "\t%c%s: elimina il canale", COMMAND_CHAR, DESTROY_COMMAND);
    send_msg_by_server(socket, msg);      
    
    sprintf(msg, "Un messaggio che inizia per %c viene sempre interpretato come comando.", COMMAND_CHAR);
    send_msg_by_server(socket, msg);
} 

/*
 * Eseguito in risposta ad un comando #help quando l'utente è entrato in un canale
 */
void send_help_joinchannel(int socket) {
    char msg[MSG_SIZE];

    sprintf(msg, "Oltre ai messaggi da condividere con gli altri utenti, e' possibile inviare "
                    "dei comandi che verranno visualizzati ed interpretati solo dal server.");
    send_msg_by_server(socket, msg);

    sprintf(msg, "La lista dei comandi disponibili e' la seguente:");
    send_msg_by_server(socket, msg);

    sprintf(msg, "\t%c%s: stampa la lista degli utenti correntemente connessi", COMMAND_CHAR, LIST_COMMAND);
    send_msg_by_server(socket, msg);

    sprintf(msg, "\t%c%s: termina la sessione", COMMAND_CHAR, QUIT_COMMAND);
    send_msg_by_server(socket, msg);

    sprintf(msg, "\t%c%s: stampa alcune statistiche sulla sessione corrente", COMMAND_CHAR, STATS_COMMAND);
    send_msg_by_server(socket, msg);

    sprintf(msg, "\t%c%s: mostra nuovamente la lista dei comandi disponibili", COMMAND_CHAR, HELP_COMMAND);
    send_msg_by_server(socket, msg);
    
    sprintf(msg, "\t%c%s: permette di abbandonare il canale", COMMAND_CHAR, LEAVE_COMMAND);
    send_msg_by_server(socket, msg);  
    
    sprintf(msg, "Un messaggio che inizia per %c viene sempre interpretato come comando.", COMMAND_CHAR);
    send_msg_by_server(socket, msg);
}

/*
 * Eseguito in risposta ad un comando #list.
 */
void send_list(int socket) {
    char msg[MSG_SIZE];
    int ret;

    ret = sem_wait(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_wait su user_data_sem");

    sprintf(msg, "Lista utenti connessi (%d): ", current_users);
    int i;
    for (i = 0; i < current_users; i++) {
        char tmp[MSG_SIZE];
        sprintf(tmp, "%s (%s:%u), ", users[i]->nickname, users[i]->address, users[i]->port);
        strcat(msg, tmp);
    }

    ret = sem_post(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_post su user_data_sem");

    send_msg_by_server(socket, msg);
}

/*
 * Eseguito in risposta ad un comando #stats.
 */
void send_stats(int socket) {
    char msg[MSG_SIZE];
    int ret;

    ret = sem_wait(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_wait su user_data_sem");

    int i;
    for (i = 0; i < current_users; i++)
        if (users[i]->socket == socket) {
            sprintf(msg, "Messaggi inviati ad altri utenti: %u, messaggi ricevuti: %u", users[i]->sent_msgs, users[i]->rcvd_msgs);
            break;
        }

    ret = sem_post(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_post su user_data_sem");

    send_msg_by_server(socket, msg);
}

/*
 * Eseguito in risposta ad un comando #create.
 * Crea un canale privato
 */ 
void *send_create(session_thread_args_t* args, char *chat_name, char *nickname) {
	
    char msg[MSG_SIZE];
    char error_msg[MSG_SIZE];
    int ret;
    int i;
    
    ret = sem_post(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_post su user_data_sem");
    
    // creazione nuovo channel
    channel_t* new_channel = (channel_t*)malloc(sizeof(channel_t));
    sprintf(new_channel->name, "%s", chat_name);
    new_channel->descriptor = desc;
    new_channel->num = 0;
    for(i=0;i<current_users;i++) {
		if(strcmp(users[i]->nickname,nickname) == 0) {
			int tmp = new_channel->num++;
			users[i]->channel = 1;
			new_channel->users_list[tmp] = users[i];
		}
	}
    channel[desc++] = new_channel;
	
    ret = sem_post(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_post su user_data_sem");
	
    sprintf(msg, "Utente %s, benvenuto nel tuo canale", nickname);
    send_msg_by_server(args->socket, msg);
    send_help_channel(args->socket);

    // sessione di chat
    int quit = 0;
    do {
	alarm(TIMEOUT);
	signal(SIGALRM, gestione_timeout);
        size_t len = recv_msg(args, msg, MSG_SIZE);
        signal(SIGALRM,SIG_IGN);

        if (len > 0) {
            // determina se l'input ricevuto è un comando o un messaggio da inoltrare
            if (msg[0] == COMMAND_CHAR) {
                if (strcmp(msg + 1, LIST_COMMAND) == 0) {
                    printf("Ricevuto comando list dall'utente %s\n", nickname);
                    send_list(args->socket);
                } else if (strcmp(msg + 1, QUIT_COMMAND) == 0) {
                    printf("Ricevuto comando quit dall'utente %s\n", nickname);
                    quit = 1;
                } else if (strcmp(msg + 1, STATS_COMMAND) == 0) {
                    printf("Ricevuto comando stats dall'utente %s\n", nickname);
                    send_stats(args->socket);
                } else if (strcmp(msg + 1, HELP_COMMAND) == 0) {
                    if (LOG) printf("Invio help all'utente %s\n", nickname);
                    send_help_channel(args->socket);
                } else if (strcmp(msg + 1, DESTROY_COMMAND) == 0 && (strcmp(nickname,chat_name) == 0))  {
		    printf("Ricevuto comando destroy dall'utente %s\n", nickname);
	            send_destroy(args->socket, nickname);
		    return NULL;
                } else {
                    sprintf(error_msg, "Comando sconosciuto, inviare %c%s per la lista dei comandi disponibili.", COMMAND_CHAR, HELP_COMMAND);
                    send_msg_by_server(args->socket, error_msg);
                }
            } else {
                // inserisci il messaggio nella coda dei messaggi da inviare
                enqueue(nickname, msg);
            }
        } else if (len < 0) {
            quit = -1; // errore: connessione chiusa dal client inaspettatamente
        } else {
            // ignora messaggi vuoti (len == 0) inviati dal client
        }

    } while(!quit);

    ret = user_leaving(args->socket);
    if (quit > 0) { // quit == 1: invio un messaggio di uscita al client
        if (ret == USER_NOT_FOUND) {
            sprintf(error_msg, "Utente non trovato: %s", nickname); // bug nel server?
        } if ( quit == TIMEOUT ){
			sprintf(error_msg, "Timeout scaduto! %s, grazie per aver partecipato alla chatroom!!!", nickname);
			quit = 0;
	}else {
            sprintf(error_msg, "%s, grazie per aver partecipato alla chatroom!!!", nickname);
        }
        end_chat_session(args, error_msg);
    } else { // quit == -1: il client ha chiuso la connessione inaspettatamente
        end_chat_session_for_closed_socket(args);
    }
    
    return NULL; 
}

/*
 * Eseguito in risposta ad un comando #<channel_number>.
 * Gestisce il tentativo di accedere al canale da parte di un utente
 * In caso di successo registra l'utente
 * nella struttura dati del canale e notifica tutti gli utenti di quel canale.
 */
void send_join(session_thread_args_t* args, const char *nickname, unsigned int descriptor) {
    
    int ret, i, j;
	char error_msg[MSG_SIZE];
	
    ret = sem_wait(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_wait su user_data_sem");
	
    char msg[MSG_SIZE];
	for(j=0; j<current_users; j++) {
		if(strcmp(users[j]->nickname,nickname) == 0) {
			int tmp = channel[descriptor]->num++;
			channel[descriptor]->users_list[tmp] = users[j];
		}
	}
	
    // notifica la presenza a tutti gli utenti nel canale
    for(i = 0; i < channel[descriptor]->num; i++) {
		sprintf(msg, "L'utente %s e' entrato nel canale", nickname);
		send_msg_by_server(channel[descriptor]->users_list[i]->socket, msg);
	}

    ret = sem_post(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_post su user_data_sem");
    
    send_help_joinchannel(args->socket);

    // sessione di chat
    int quit = 0;
    user_data_t* user = (user_data_t*)malloc(sizeof(user_data_t));
    
    for(i=0; i<current_users; i++){
		if(users[i]->socket == args->socket){
			user = users[i];
			user->channel = 1;
		}
	}
	
    while(!quit && user->channel){
		
	alarm(TIMEOUT);
	signal(SIGALRM, gestione_timeout);
        size_t len = recv_msg(args, msg, MSG_SIZE);
        signal(SIGALRM,SIG_IGN);

        if (len > 0) {
            // determina se l'input ricevuto è un comando o un messaggio da inoltrare
            if (msg[0] == COMMAND_CHAR) {
                if (strcmp(msg + 1, LIST_COMMAND) == 0) {
                    printf("Ricevuto comando list dall'utente %s\n", nickname);
                    send_list(args->socket);
                } else if (strcmp(msg + 1, QUIT_COMMAND) == 0) {
                    printf("Ricevuto comando quit dall'utente %s\n", nickname);
                    quit = 1;
                } else if (strcmp(msg + 1, STATS_COMMAND) == 0) {
                    printf("Ricevuto comando stats dall'utente %s\n", nickname);
                    send_stats(args->socket);
                } else if (strcmp(msg + 1, HELP_COMMAND) == 0) {
                    if (LOG) printf("Invio help all'utente %s\n", nickname);
                    send_help_joinchannel(args->socket);
                } else if (strcmp(msg + 1, LEAVE_COMMAND) == 0) {
					printf("Ricevuto comando leave dall'utente %s\n", nickname);
					send_leave(args->socket, nickname, descriptor);
					return;
                } else {
                    sprintf(error_msg, "Comando sconosciuto, inviare %c%s per la lista dei comandi disponibili.", COMMAND_CHAR, HELP_COMMAND);
                    send_msg_by_server(args->socket, error_msg);
                }
            } else {
                // inserisci il messaggio nella coda dei messaggi da inviare
                enqueue(nickname, msg);
            }
        } else if (len < 0) {
            quit = -1; // errore: connessione chiusa dal client inaspettatamente
        } else {
            // ignora messaggi vuoti (len == 0) inviati dal client
        }

    }
    
    if(!(user->channel)) return;

    ret = user_leaving(args->socket);
    if (quit > 0) { // quit == 1: invio un messaggio di uscita al client
        if (ret == USER_NOT_FOUND) {
            sprintf(error_msg, "Utente non trovato: %s", nickname); // bug nel server?
        }if ( quit == TIMEOUT ){
			sprintf(error_msg, "Timeout scaduto! %s, grazie per aver partecipato alla chatroom!!!", nickname);
			quit = 0;
	    } else {
            sprintf(error_msg, "%s, grazie per aver partecipato alla chatroom!!!", nickname);
        }
        end_chat_session(args, error_msg);
    } else { // quit == -1: il client ha chiuso la connessione inaspettatamente
        end_chat_session_for_closed_socket(args);
    }
    
    return;
    
}

void send_destroy(int socket, char *chat_name) {
    char msg[MSG_SIZE]; int i, temp, ret, j, k;
	
    ret = sem_wait(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_wait su user_data_sem");
    
	for(i = 0;i < desc; i++) {
		if(strcmp(channel[i]->name,chat_name) == 0) {
			
			for(j=0; j<channel[i]->num; j++){
				if(strcmp(channel[i]->users_list[j]->nickname, chat_name) != 0){
					send_message_leave(channel[i]->users_list[j]->socket, channel[i]->users_list[j]->nickname, channel[i]->descriptor);
					send_leave_bis(channel[i]->users_list[j]->socket, channel[i]->users_list[j]->nickname, channel[i]->descriptor);
					for(k=0; k<current_users; k++){
						if(strcmp(users[k]->nickname,channel[i]->users_list[j]->nickname) == 0)
							users[k]->channel = 0;
					
					}
				} else{
					for(k=0; k<current_users; k++){
						if(strcmp(users[k]->nickname,channel[i]->users_list[j]->nickname) == 0)
							users[k]->channel = 0;
					}
				}
			} 

            for (; i < desc - 1; i++){
                temp = channel[i]->descriptor - 1;
                channel[i]->descriptor = temp;
                channel[i] = channel[i+1]; // shift di 1 per tutti gli elementi successivi
            }
            desc--;
            channel[desc]->descriptor = desc;
		}
	}
	
	for(i = 0;i < desc; i++) {
		if(strcmp(channel[i]->name,chat_name) == 0) {
			
			for(j=0; j<channel[i]->num; j++){
				if(strcmp(channel[i]->users_list[j]->nickname, chat_name) != 0){
					send_leave_bis(channel[i]->users_list[j]->socket, channel[i]->users_list[j]->nickname, channel[i]->descriptor);
				}
			} 
		}
	}
	
	ret = sem_post(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_post su user_data_sem");
	
	sprintf(msg, "Il canale %s e' stato distrutto :(. Bentornato nella chat globale :)", chat_name);	
	send_msg_by_server(socket, msg);
	
}

void send_destroy_bis(char *chat_name) {
	int i, temp, ret, j, k;
	
	ret = sem_wait(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_wait su user_data_sem");
    
	for(i = 0;i < desc; i++) {
		if(strcmp(channel[i]->name,chat_name) == 0) {
			
			for(j=0; j<channel[i]->num; j++){
				if(strcmp(channel[i]->users_list[j]->nickname, chat_name) != 0){
					send_message_leave(channel[i]->users_list[j]->socket, channel[i]->users_list[j]->nickname, channel[i]->descriptor);
					send_leave_bis(channel[i]->users_list[j]->socket, channel[i]->users_list[j]->nickname, channel[i]->descriptor);
					for(k=0; k<current_users; k++){
						if(strcmp(users[k]->nickname,channel[i]->users_list[j]->nickname) == 0)
							users[k]->channel = 0;
					
					}
				} else{
					for(k=0; k<current_users; k++){
						if(strcmp(users[k]->nickname,channel[i]->users_list[j]->nickname) == 0)
							users[k]->channel = 0;
					}
				}
			} 

            for (; i < desc - 1; i++){
                temp = channel[i]->descriptor - 1;
                channel[i]->descriptor = temp;
                channel[i] = channel[i+1]; // shift di 1 per tutti gli elementi successivi
            }
            desc--;
            channel[desc]->descriptor = desc;
		}
	}
	
	for(i = 0;i < desc; i++) {
		if(strcmp(channel[i]->name,chat_name) == 0) {
			
			for(j=0; j<channel[i]->num; j++){
				if(strcmp(channel[i]->users_list[j]->nickname, chat_name) != 0){
					send_leave_bis(channel[i]->users_list[j]->socket, channel[i]->users_list[j]->nickname, channel[i]->descriptor);
				}
			} 
		}
	}
	
	ret = sem_post(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_post su user_data_sem");
	
	
}

/*
 * Eseguito in risposta ad un comando #leave.
 */
void send_leave(int socket, const char *nickname, unsigned int descriptor) {
	char msg[MSG_SIZE];int i, j, k, ret;
	
	ret = sem_wait(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_wait su user_data_sem");
	
	for (j=0; j < channel[descriptor]->num; j++){
		if(strcmp(channel[descriptor]->users_list[j]->nickname, nickname) == 0){
			for(k=0; k<current_users; k++){
				if(strcmp(users[k]->nickname,channel[descriptor]->users_list[j]->nickname) == 0)
					users[k]->channel = 0; //users[k] esce dal canale
			}
			break;
		}
	}
    for (; j < channel[descriptor]->num - 1; j++)
        channel[descriptor]->users_list[j] = channel[descriptor]->users_list[j+1]; // shift di 1 per tutti gli elementi successivi
    
    channel[descriptor]->num--;
	
    //informo tutti gli utenti del canale che l'utente è ha lasciato il canale
	for(i = 0; i < channel[descriptor]->num; i++) {
		sprintf(msg, "L'utente %s e' uscito dal canale", nickname);
		send_msg_by_server(channel[descriptor]->users_list[i]->socket, msg);
	}
	
	ret = sem_post(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_post su user_data_sem");
	
	sprintf(msg, "Sei uscito dal canale %s(%d)", channel[descriptor]->name, descriptor);	
	send_msg_by_server(socket, msg);
	
}

void send_leave_bis(int socket, const char *nickname, unsigned int descriptor) {
	int j, k;
	
	for (j=0; j < channel[descriptor]->num; j++){
		if(strcmp(channel[descriptor]->users_list[j]->nickname, nickname) == 0){
			for(k=0; k<current_users; k++){
				if(strcmp(users[k]->nickname,channel[descriptor]->users_list[j]->nickname) == 0)
					users[k]->channel = 0;
			}
			break;
		}
	}
    for (; j < channel[descriptor]->num - 1; j++)
        channel[descriptor]->users_list[j] = channel[descriptor]->users_list[j+1]; // shift di 1 per tutti gli elementi successivi
    
    channel[descriptor]->num--;
	
}

void send_leave_quit(int socket, const char *nickname, unsigned int descriptor) {
	int i, j, k; char msg[MSG_SIZE];
	
	for (j=0; j < channel[descriptor]->num; j++){
		if(strcmp(channel[descriptor]->users_list[j]->nickname, nickname) == 0){
			for(k=0; k<current_users; k++){
				if(strcmp(users[k]->nickname,channel[descriptor]->users_list[j]->nickname) == 0)
					users[k]->channel = 0;
			}
			break;
		}
	}
    for (; j < channel[descriptor]->num - 1; j++)
        channel[descriptor]->users_list[j] = channel[descriptor]->users_list[j+1]; // shift di 1 per tutti gli elementi successivi
    
    channel[descriptor]->num--;
	
	for(i = 0; i < channel[descriptor]->num; i++) {
		sprintf(msg, "L'utente %s e' uscito dal canale", nickname);
		send_msg_by_server(channel[descriptor]->users_list[i]->socket, msg);
	}
}

void send_message_leave(int socket, const char *nickname, unsigned int descriptor) {
	char msg[MSG_SIZE];
	
	sprintf(msg, "Il canale %s e' stato distrutto :(. Premi un qualsiasi tasto per ritornare nella chat globale", channel[descriptor]->name);
	send_msg_by_server(socket, msg);
	
}

/*
 * Eseguito in risposta ad un comando #channels.
 */
void send_channels(int socket) {
    char msg[MSG_SIZE];
    int ret;

    ret = sem_wait(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_wait su user_data_sem");
	
	sprintf(msg, "Lista channels (%d): ", desc);
    int i;
    for (i = 0; i < desc; i++) {
        char tmp[MSG_SIZE];
        sprintf(tmp, "%s (%d), ", channel[i]->name, i);
        strcat(msg, tmp);
    }

    ret = sem_post(&user_data_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_post su user_data_sem");

    send_msg_by_server(socket, msg);
}

