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

#define NO_ERROR 0

void clearwinsock() {
#if defined WIN32
	WSACleanup();
#endif
}

// ---- funzioni per simulare dati meteo ----
static float frand(float a, float b) {
    return a + ((float) rand() / RAND_MAX) * (b - a);
}

float get_temperature() { return frand(-10.0f, 40.0f); }
float get_humidity()    { return frand( 20.0f,100.0f); }
float get_wind()        { return frand(  0.0f,100.0f); }
float get_pressure()    { return frand(950.0f,1050.0f); }

// Funzione che controlla se una città è supportata dal server.
// Restituisce 1 se la città esiste nella lista, altrimenti 0.
int is_valid_city(const char* c) {

    // Lista delle 10 città italiane supportate (tutte minuscole)
    const char* list[] = {
        "bari","roma","milano","napoli","torino",
        "palermo","genova","bologna","firenze","venezia"
    };

    // Buffer per contenere la versione della città tutta minuscola
    char lower[CITY_MAX];

    // Copia sicura della stringa in input dentro il buffer "lower"
    // Limitiamo a CITY_MAX caratteri per evitare overflow
    strncpy(lower, c, CITY_MAX);

    // Garantiamo la terminazione del buffer con '\0'
    lower[CITY_MAX - 1] = '\0';

    // Convertiamo tutta la stringa in minuscolo
    // (il server deve essere case-insensitive)
    for (char* p = lower; *p; p++)
        *p = tolower(*p);

    // Confrontiamo la stringa (ora minuscola) con ognuna delle città supportate
    for (int i = 0; i < 10; i++) {
        if (strcmp(lower, list[i]) == 0)
            return 1;   // città trovata → valida
    }

    // Nessuna corrispondenza trovata → città non supportata
    return 0;
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
	server_addr.sin_family      = AF_INET;
	server_addr.sin_port        = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	// 3) BIND
	if (bind(my_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
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

	printf("Server meteo in ascolto sulla porta %d...\n", SERVER_PORT);

	// 5) LOOP DI ACCETTAZIONE
	while (1) {

		struct sockaddr_in client_addr;
		int client_len = sizeof(client_addr);
		int client_socket = accept(my_socket, (struct sockaddr*)&client_addr, &client_len);

		if (client_socket < 0) {
			printf("accept() failed\n");
			continue;
		}

		weather_request_t req;
		if (recv(client_socket,(char *) &req, sizeof(req), 0) <= 0) {
			printf("recv() fallita\n");
			closesocket(client_socket);
			continue;
		}

		printf("Richiesta '%c %s' dal client %s\n",
		       req.type, req.city, inet_ntoa(client_addr.sin_addr));

		weather_response_t resp;
		resp.status = STATUS_OK;
		resp.type   = req.type;
		resp.value  = 0.0f;

		// VALIDAZIONE TIPO
		if (req.type!='t' && req.type!='h' && req.type!='w' && req.type!='p') {
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
			if (req.type=='t') resp.value = get_temperature();
			if (req.type=='h') resp.value = get_humidity();
			if (req.type=='w') resp.value = get_wind();
			if (req.type=='p') resp.value = get_pressure();
		}

		send(client_socket, (char *)&resp, sizeof(resp), 0);

		closesocket(client_socket);
	}

	printf("Server terminated.\n");

	closesocket(my_socket);
	clearwinsock();
	return 0;
}
