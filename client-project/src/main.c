/*
 * TCP Client - Weather Request
 */

#if defined WIN32
#include <winsock.h>
#else
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define closesocket close
#endif

#include <stdio.h>
#include <stdlib.h>
#include "protocol.h"


void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}

void arguments_error(const char* progname) {
    printf("\nUso corretto:\n");
    printf("  %s [-s server_ip] [-p port] -r \"<type> <city>\"\n\n", progname);
    printf("Dove:\n");
    printf("  <type>  = t (temperatura), h (umidita'), w (vento), p (pressione)\n");
    printf("  <city>  = nome della citta' (anche con spazi)\n");
    printf("  -s ip   = opzionale, default 127.0.0.1\n");
    printf("  -p port = opzionale, default %d\n\n", SERVER_PORT);
    clearwinsock();
}

int valid_type(char t) {
    return (t == 't' || t == 'h' || t == 'w' || t == 'p');
}ma

int main(int argc, char *argv[]) {

#if defined WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (result != NO_ERROR) {
        printf("Error at WSAStartup()\n");
        return 0;
    }
#endif

    char server_ip[64] = "127.0.0.1";
    int port = SERVER_PORT;

    char type = '\0';
    char city[CITY_MAX] = {0};
    int request_found = 0;

    // =========================
    //     PARSING ARGOMENTI
    // =========================

    if (argc < 3) {
        arguments_error(argv[0]);
        return 0;
    }

    int i = 1;
    while (i < argc) {

        // -- -s server_ip --
        if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc) {
                printf("Errore: manca l'indirizzo IP dopo -s\n");
                arguments_error(argv[0]);
                return 0;
            }
            strncpy(server_ip, argv[i+1], sizeof(server_ip));
            server_ip[sizeof(server_ip)-1] = '\0';
            i += 2;
            continue;
        }

        // -- -p port --
        if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 >= argc) {
                printf("Errore: manca la porta dopo -p\n");
                arguments_error(argv[0]);
                return 0;
            }
            port = atoi(argv[i+1]);
            if (port <= 0 || port > 65535) {
                printf("Errore: porta non valida\n");
                arguments_error(argv[0]);
                return 0;
            }
            i += 2;
            continue;
        }

        // -- L’ULTIMA DEVE ESSERE -r --
        if (strcmp(argv[i], "-r") == 0) {
            if (i + 1 >= argc) {
                printf("Errore: manca la richiesta dopo -r\n");
                arguments_error(argv[0]);
                return 0;
            }

            char* tmp = argv[i+1];
            type = tmp[0];

            // Salta gli spazi prima della città
            char* city_start = tmp + 1;
            while (*city_start == ' ') city_start++;

            strncpy(city, city_start, CITY_MAX - 1);
            city[CITY_MAX - 1] = '\0';

            request_found = 1;

            // DEVE essere l'ultimo parametro
            if (i + 2 != argc) {
                printf("Errore: -r \"type city\" deve essere l'ultimo parametro\n");
                arguments_error(argv[0]);
                return 0;
            }

            break;
        }

        // -- Argomento sconosciuto --
        printf("Argomento sconosciuto: %s\n", argv[i]);
        arguments_error(argv[0]);
        return 0;
    }

    if (!request_found) {
        printf("Errore: devi specificare -r \"type city\"\n");
        arguments_error(argv[0]);
        return 0;
    }

    if (!valid_type(type)) {
        printf("Errore: type non valido (usa t/h/w/p)\n");
        arguments_error(argv[0]);
        return 0;
    }

    if (strlen(city) == 0) {
        printf("Errore: nome città vuoto\n");
        arguments_error(argv[0]);
        return 0;
    }

    // 1) CREATE SOCKET
    int my_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (my_socket < 0) {
        printf("socket() failed\n");
        clearwinsock();
        return 0;
    }

    // 2) SERVER ADDRESS
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    //il client deve connettersi a uno specifico server

    // 3) CONNECT
    if (connect(my_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Connessione al server fallita.\n");
        closesocket(my_socket);
        clearwinsock();
        return 0;
    }

    // INVIO RICHIESTA
    weather_request_t req;
    req.type = type;
    strncpy(req.city, city, CITY_MAX);

    if (send(my_socket, (char*)&req, sizeof(req), 0) != sizeof(req)) {
        printf("Errore durante l'invio.\n");
        closesocket(my_socket);
        clearwinsock();
        return 0;
    }

    // RICEZIONE RISPOSTA
    weather_response_t resp;

    if (recv(my_socket, (char*)&resp, sizeof(resp), 0) <= 0) {
        printf("Errore durante la ricezione\n");
        closesocket(my_socket);
        clearwinsock();
        return 0;
    }

    printf("Ricevuto risultato dal server ip %s. ", server_ip);

    if (resp.status == STATUS_OK) {
        switch (resp.type) {
            case TYPE_TEMP:
            	printf("%s: Temperatura = %.1f°C\n", city, resp.value);
            	break;
            case TYPE_HUM:
            	printf("%s: Umidità = %.1f%%\n", city, resp.value);
            	break;
            case TYPE_WIND:
            	printf("%s: Vento = %.1f km/h\n", city, resp.value);
            	break;
            case TYPE_PRESS:
            	printf("%s: Pressione = %.1f hPa\n", city, resp.value);
            	break;
        }
    }
    else if (resp.status == STATUS_CITY_UNKNOWN) {
        printf("Citta' non disponibile\n");
    }
    else {
        printf("Richiesta non valida\n");
    }
    printf("Client terminated.\n");

    closesocket(my_socket);
    clearwinsock();
    return 0;
}
