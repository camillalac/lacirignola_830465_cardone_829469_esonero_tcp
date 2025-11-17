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

#define NO_ERROR 0

void clearwinsock() {
#if defined WIN32
	WSACleanup();
#endif
}

void arguments_error(const char* progname) {
    printf("\nUso corretto:\n");
    printf("  %s -r \"<type> <city>\" [-s server_ip] [-p port]\n\n", progname);
    printf("Dove:\n");
    printf("  <type>  = t (temperatura), h (umidita), w (vento), p (pressione)\n");
    printf("  <city>  = nome citta supportata (bari, roma, ...)\n");
    printf("  -s ip   = opzionale, default 127.0.0.1\n");
    printf("  -p port = opzionale, default %d\n\n", SERVER_PORT);
    clearwinsock();
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

	/*
	if (argc < 3) {
		printf("Uso: %s -r \"<type> <city>\" [-s server_ip] [-p port]\n", argv[0]);
		clearwinsock();
		return 0;
	}

	char server_ip[64] = "127.0.0.1";
	int port = SERVER_PORT;

	char type;
	char city[CITY_MAX];

	// PARSE ARGUMENTS
	for (int i=1; i<argc; i++) {
		if (strcmp(argv[i],"-s")==0 && i+1<argc) {
			strncpy(server_ip, argv[++i], sizeof(server_ip));
		}
		else if (strcmp(argv[i],"-p")==0 && i+1<argc) {
			port = atoi(argv[++i]);
		}
		else if (strcmp(argv[i],"-r")==0 && i+1<argc) {
			char tmp[128];
			strncpy(tmp, argv[++i], sizeof(tmp));
			sscanf(tmp, "%c %63s", &type, city);
		}
	}*/

	// =========================
	//     PARSING ARGOMENTI
	// =========================

	char server_ip[64] = "127.0.0.1";
	int port = SERVER_PORT;

	char type = '\0';
	char city[CITY_MAX] = {0};
	int request_found = 0;

	// controlla solo caso senza argomenti
	if (argc < 2) {
	    arguments_error(argv[0]);
	    return 0;
	}


	for (int i = 1; i < argc; i++) {

	    if (strcmp(argv[i], "-s") == 0) {
	        if (i + 1 >= argc) {
	            printf("Errore: manca l'indirizzo IP dopo -s\n");
	            arguments_error(argv[0]);
	            return 0;
	        }
	        strncpy(server_ip, argv[++i], sizeof(server_ip));
	        continue;
	    }

	    if (strcmp(argv[i], "-p") == 0) {
	        if (i + 1 >= argc) {
	            printf("Errore: manca la porta dopo -p\n");
	            arguments_error(argv[0]);
	            return 0;
	        }
	        port = atoi(argv[++i]);
	        if (port <= 0 || port > 65535) {
	            printf("Errore: porta non valida\n");
	            arguments_error(argv[0]);
	            return 0;
	        }
	        continue;
	    }

	    if (strcmp(argv[i], "-r") == 0) {
	        if (i + 1 >= argc) {
	            printf("Errore: manca la richiesta dopo -r\n");
	            arguments_error(argv[0]);
	            return 0;
	        }

	        char tmp[128];
	        strncpy(tmp, argv[++i], sizeof(tmp));
	        tmp[sizeof(tmp)-1] = '\0';

	        // parse "<type> <city>"
	        if (sscanf(tmp, " %c %63s", &type, city) != 2) {
	            printf("Errore: formato non valido. Usa: -r \"t bari\"\n");
	            arguments_error(argv[0]);
	            return 0;
	        }

	        request_found = 1;
	        continue;
	    }

	    // argomento sconosciuto
	    printf("Argomento sconosciuto: %s\n\n", argv[i]);
	    arguments_error(argv[0]);
	    return 0;
	}

	// =========================
	//   VALIDAZIONE LOGICA
	// =========================

	if (!request_found) {
	    printf("Errore: devi usare -r \"<type> <city>\"\n");
	    arguments_error(argv[0]);
	    return 0;
	}

	if (type!='t' && type!='h' && type!='w' && type!='p') {
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

	// 3) CONNECT
	if (connect(my_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		printf("Connessione al server fallita.\n");
		closesocket(my_socket);
		clearwinsock();
		return 0;
	}

	weather_request_t req;
	req.type = type;
	strncpy(req.city, city, CITY_MAX);

	send(my_socket, (char *) &req, (int)sizeof(req), 0);

	weather_response_t resp;

	if (recv(my_socket,(char *) &resp, (int)sizeof(resp), 0) <= 0) {
		printf("Errore durante la ricezione\n");
		closesocket(my_socket);
		clearwinsock();
		return 0;
	}

	printf("Ricevuto risultato dal server ip %s. ", server_ip);

	if (resp.status == STATUS_OK) {
		if (resp.type=='t') printf("%s: Temperatura = %.1f°C\n", city, resp.value);
		if (resp.type=='h') printf("%s: Umidità = %.1f%%\n", city, resp.value);
		if (resp.type=='w') printf("%s: Vento = %.1f km/h\n", city, resp.value);
		if (resp.type=='p') printf("%s: Pressione = %.1f hPa\n", city, resp.value);
	}
	else if (resp.status == STATUS_CITY_UNKNOWN) {
		printf("Città non disponibile\n");
	}
	else {
		printf("Richiesta non valida\n");
	}

	printf("Client terminated.\n");

	closesocket(my_socket);
	clearwinsock();
	return 0;
}
