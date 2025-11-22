
/*SERVER*/
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
        "palermo","genova","bologna","firenze","venezia", "reggio calabria"
    };

    char lower[CITY_MAX];
    strncpy(lower, c, CITY_MAX);
    lower[CITY_MAX - 1] = '\0';


    for (char* p = lower; *p; p++)
        *p = tolower(*p);


    for (int i = 0; i < 11; i++) {
        if (strcmp(lower, list[i]) == 0)
            return 1;   // città trovata valida
    }

    // città non trovata
    return 0;
}


int valid_type(char t) {
    return (t == TYPE_TEMP || t == TYPE_HUM ||
            t == TYPE_WIND || t == TYPE_PRESS);
}

void errorhandler(char *errorMessage) {
printf ("%s", errorMessage);
}

int parse_port(int argc, char *argv[], int *port) {

    if (argc == 1) return 1;

    if (argc != 3) return 0;

    if (strcmp(argv[1], "-p") != 0)
        return 0;

    if (argv[2][0] == '-')
        return 0;

    int p = atoi(argv[2]);

    if (p <= 0 || p > 65535)
        return 0;

    *port = p;
    return 1;
}


int main(int argc, char *argv[]) {

#if defined WIN32
WSADATA wsa_data;
int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
if (result != NO_ERROR) {
    printf("Error at WSAStartup()\n");
    return 1;
}
#endif


	srand((unsigned)time(NULL));

	int port = SERVER_PORT;

	if (!parse_port(argc, argv, &port)) {
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

	    #if defined WIN32
	    	int client_len = sizeof(client_addr);
		#else
	    	socklen_t client_len = sizeof(client_addr);
		#endif

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
