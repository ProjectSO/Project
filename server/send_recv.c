#include <string.h>
#include <sys/socket.h>

#include "common.h"
#include "methods.h"

/*
 * Invia il messaggio contenuto nel buffer sulla socket desiderata.
 */
void send_msg(int socket, const char *msg) {
    int ret;

    char msg_to_send[MSG_SIZE];
    sprintf(msg_to_send, "%s\n", msg);
     int bytes_send=0;
     bytes_left=strlen(msg_to_send);
     while(bytes_left>0){
		 ret=send(socket,msg_to_send + bytes_send, bytes_left,0);
		 if(ret==-1 && errno==EINTR)continue;
		 HERROR_HELPER(ret,"er");
		 bytes_send+=ret;
		 bytes_left-=ret;     
}

size_t recv_msg(int socket, char *buf, size_t buf_len) {
    int ret;
    int bytes_read = 0;

    // messaggi più lunghi di buf_len bytes vengono troncati
    while (bytes_read <= buf_len) {
         ret=recv(socket,buf+bytes_read,1,0);
         if(ret==-1 && errno==EINTR) continue;
         if(ret==0) return -1;
         if(buf[bytes_read]=='/n') break;
         bytes_read+=ret;
			
    }

    /* Quando un messaggio viene ricevuto correttamente, il carattere
     * finale '\n' viene sostituito con un terminatore di stringa. */
    buf[bytes_read] = '\0';
    return bytes_read; 
}
