compilation: gcc client.c -o client -lws2_32

launch: client 192.168.1.100 1025 nomeUtente // questo indirizzo � l' indirizzo inet di eth0 del server