/*
 * TCP Server - Weather Service
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
#include <ctype.h>
#include <time.h>

#include "protocol.h"


void clearwinsock() {
#if defined WIN32
	WSACleanup();
#endif
}

// ---- funzioni per simulare dati meteo ----
static float frand(float a, float b) {
    return a + ((float) rand() / RAND_MAX) * (b - a);
}

float get_temperature()
{ return frand(-10.0f, 40.0f); }
float get_humidity()
{ return frand( 20.0f,100.0f); }
float get_wind()
{ return frand(  0.0f,100.0f); }
float get_pressure()
{ return frand(950.0f,1050.0f); }


int is_valid_city(const char* c) {

    const char* list[] = {
        "bari","roma","milano","napoli","torino",
        "palermo","genova","bologna","firenze","venezia", "new york"
    };

    char lower[CITY_MAX];


    strncpy(lower, c, CITY_MAX);

    lower[CITY_MAX - 1] = '\0';


    for (char* p = lower; *p; p++)
        *p = tolower(*p);


    for (int i = 0; i < 11; i++) {
        if (strcmp(lower, list[i]) == 0)
            return 1;   // città trovata → valida
    }

    // Nessuna corrispondenza trovata → città non supportata
    return 0;
}


int valid_type(char t) {
    return (t == TYPE_TEMP || t == TYPE_HUM ||
            t == TYPE_WIND || t == TYPE_PRESS);
}


int parse_port_argument(int argc, char *argv[], int *port) {

    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "-p") == 0) {

            // Controlla che ci sia un valore dopo -p
            if (i + 1 >= argc) {
                printf("Errore: manca la porta dopo -p\n");
                return 0;
            }

            // Controlla che non sia un altro flag
            if (argv[i+1][0] == '-') {
                printf("Errore: dopo -p devi inserire una porta, non un flag\n");
                return 0;
            }

            // Conversione e validazione
            int given_port = atoi(argv[i+1]);
            if (given_port <= 0 || given_port > 65535) {
                printf("Porta non valida. Valori ammessi: 1–65535\n");
                return 0;
            }

            *port = given_port;
            i++; // Salta la porta
            continue;
        }

        // Argomento sconosciuto
        else {
            printf("Argomento sconosciuto: %s\n", argv[i]);
            return 0;
        }
    }

    return 1; // Parsing ok
}


int main(int argc, char *argv[]) {

#if defined WIN32
	WSADATA wsa_data;
	int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
	if (result != NO_ERROR) {
		printf("Error at WSAStartup()\n");
		return 0;
	}
#endif

	srand((unsigned)time(NULL));

	int port = SERVER_PORT;

	if (!parse_port_argument(argc, argv, &port)) {
	    printf("Uso corretto: %s [-p porta]\n", argv[0]);
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
	struct sockaddr_in sad;
	memset(&sad, 0, sizeof(sad));
	sad.sin_family      = AF_INET;
	sad.sin_port        = htons(port);
	sad.sin_addr.s_addr = INADDR_ANY;
	//accetta connessione da qualunque client

    // 3) BIND
    if (bind(my_socket, (struct sockaddr*)&sad, sizeof(sad)) < 0) {
        printf("bind() failed\n");
        closesocket(my_socket);
        clearwinsock();
        return 0;
    }

    // 4) LISTEN
    if (listen(my_socket, QUEUE_SIZE) < 0) {
        printf("listen() failed\n");
        closesocket(my_socket);
        clearwinsock();
        return 0;
    }
	printf("Server meteo in ascolto sulla porta %d...\n", port);

	// 5) LOOP DI ACCETTAZIONE
	while (1) {

		struct sockaddr_in client_addr;
		int client_len = sizeof(client_addr);
		int client_socket = accept(my_socket, (struct sockaddr*)&client_addr, &client_len);

		if (client_socket < 0) {
			printf("accept() failed\n");
			closesocket(client_socket);
			clearwinsock();
			return 0;
		}

		weather_request_t req;
		if (recv(client_socket,(char *) &req, sizeof(req), 0) <= 0) {
			printf("recv() fallita\n");
			closesocket(client_socket);
			clearwinsock();
			return -1;
		}

		printf("Richiesta '%c %s' dal client %s\n",
		       req.type, req.city, inet_ntoa(client_addr.sin_addr));

		weather_response_t resp;
		resp.status = STATUS_OK;
		resp.type   = req.type;
		resp.value  = 0.0f;

		// VALIDAZIONE TIPO
		if (!valid_type(req.type)) {
		    resp.status = STATUS_BAD_REQUEST;
		    resp.type = '\0';
		    resp.value = 0.0f;
		}

		else if (!is_valid_city(req.city)) {
			resp.status = STATUS_CITY_UNKNOWN;
			resp.type = '\0';
			resp.value = 0.0f;
		}
		else {
		    switch (req.type) {
		        case TYPE_TEMP:
		            resp.value = get_temperature();
		            break;
		        case TYPE_HUM:
		            resp.value = get_humidity();
		            break;
		        case TYPE_WIND:
		            resp.value = get_wind();
		            break;
		        case TYPE_PRESS:
		            resp.value = get_pressure();
		            break;
		    }
		}


		send(client_socket, (char *)&resp, sizeof(resp), 0);

		closesocket(client_socket);
	}

	printf("Server terminated.\n");

	closesocket(my_socket);
	clearwinsock();
	return 0;
}
