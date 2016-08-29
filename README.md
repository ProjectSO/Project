# Project for Operating Systems

- SPECIFICA DEL PROGETTO

    Chat multicanale
    
    Realizzazione di un servizio "chat" via internet offerto tramite server
    che gestisce un insieme di processi client (residenti, in generale, su
    macchine diverse). Il server deve essere in grado di acquisire in input
    da ogni client una linea alla volta e inviarla a tutti gli altri client
    attualmente presenti nella chat.
    Ogni client potrà decidere se creare un nuovo canale di conversazione,
    effettuare il join ad un canale già esistente, lasciare il canale,
    o chiudere il canale (solo nel caso che l'abbia istanziato).

- SCELTE DI PROGETTO E REALIZZATIVE, TECNICHE E METODOLOGIE GENERALI USATE

    - Utilizzo dei socket per la comunicazione client-server
    
    - Utilizzo di un thread per il broacast dei messaggi e creazione di un thread per la gestione della sessione di chat per ogni client
    
    - Gestione dei segnali : cattura e gestione con handler per SIGINT, SIGQUIT, SIGTSTP e SIGHUP, SIGPIPE gestito nella recv
 
- MANUALE D'USO

    Server :

       Sviluppato in Unix
       
       compilazione: make
       
       lancio: ./server < porta >
    
    Client :
    
       Sviluppato in Windows
       
       compilazione: gcc clientSO.c -o client -lws2_32
       
       lancio: client < indirizzo-inet > < porta > nomeUtente // questo indirizzo è l' indirizzo statico inet eth0 del server

- SORGENTI DEL PROGETTO
