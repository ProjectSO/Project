#include <stdio.h>
#include <semaphore.h>

#include "common.h"
#include "methods.h"

#define MSG_BUFFER_SIZE 256

// buffer circolare per gestire una coda FIFO (First In First Out)
msg_t* msg_buffer[MSG_BUFFER_SIZE];
int read_index;
int write_index;

sem_t empty_sem;
sem_t fill_sem;
sem_t write_sem;

/*
 * Inizializza la coda.
 */
void initialize_queue() {
    int ret;

    read_index = 0,
    write_index = 0;

    ret = sem_init(&empty_sem, 0, MSG_BUFFER_SIZE);
    ERROR_HELPER(ret, "Errore nell'inizializzazione del semaforo empty_sem");

    ret = sem_init(&fill_sem, 0, 0);
    ERROR_HELPER(ret, "Errore nell'inizializzazione del semaforo fill_sem");

    ret = sem_init(&write_sem, 0, 1);
    ERROR_HELPER(ret, "Errore nell'inizializzazione del semaforo write_sem");
}

/*
 * Genera un messaggio a partire dagli argomenti e lo inserisce nella coda.
 *
 * Può essere eseguito da più thread contemporaneamente.
 */
void enqueue(const char *nickname, const char *msg) {

    int ret;
    msg_t *msg_data = (msg_t*)malloc(sizeof(msg_t));
    sprintf(msg_data->nickname, "%s", nickname);
    sprintf(msg_data->msg, "%s", msg);

    ret = sem_wait(&empty_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_wait su empty_sem");

    ret = sem_wait(&write_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_wait su write_sem");

    msg_buffer[write_index] = msg_data;
    write_index = (write_index + 1) % MSG_BUFFER_SIZE;

    ret = sem_post(&write_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_post su write_sem");

    ret = sem_post(&fill_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_post su fill_sem");

}

/*
 * Estrae e restituisce il primo messaggio dalla coda (politica FIFO).
 *
 * Il thread che esegue questo metodo è uno soltanto.
 */

msg_t* dequeue() {

    int ret;
    msg_t *msg = NULL;

    ret = sem_wait(&fill_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_wait su fill_sem");

    msg = msg_buffer[read_index];
    read_index = (read_index + 1) % MSG_BUFFER_SIZE;

    ret = sem_post(&empty_sem);
    ERROR_HELPER(ret, "Errore nella chiamata sem_post su empty_sem");

    return msg;
}
