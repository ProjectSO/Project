# Project for Operating Systems

Chat multicanale

Realizzazione di un servizio "chat" via internet offerto tramite server
che gestisce un insieme di processi client (residenti, in generale, su
macchine diverse). Il server deve essere in grado di acquisire in input
da ogni client una linea alla volta e inviarla a tutti gli altri client
attualmente presenti nella chat.
Ogni client potrà decidere se creare un nuovo canale di conversazione,
effettuare il join ad un canale già esistente, lasciare il canale,
o chiudere il canale (solo nel caso che l'abbia istanziato).

Scelte di progetto e realizzative, tecniche e metodologie generali usate
---------------

-utilizzo dei socket per la comunicazione client-server

-utilizzo di un thread per il broacast dei messaggi e creazione di un thread per la gestione della sessione di chat per ogni client

-gestione dei segnali :

 cattura e gestione con handler per SIGINT, SIGQUIT, SIGTSTP e SIGHUP, SIGPIPE gestito nella recv
 

Server 
-------
Sviluppato in Unix

compilazione: make

lancio: ./server < porta >

Client 
-----------
Sviluppato in Windows

compilazione: gcc clientSO.c -o client -lws2_32

lancio: client < indirizzo-inet > < porta > nomeUtente // questo indirizzo è l' indirizzo statico inet eth0 del server
