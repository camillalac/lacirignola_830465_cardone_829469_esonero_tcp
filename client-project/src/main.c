/*CLIENT*/
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

#include "protocol.h"

#define NO_ERROR 0

void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}

void errorhandler(char *errorMessage){
	printf("%s", errorMessage);
}


void maiuscola(char *s) {
    int start = 1;
    for (int i = 0; s[i]; i++) {
        if (isspace(s[i])) start = 1;
        else {
            s[i] = start ? toupper(s[i]) : tolower(s[i]);
            start = 0;
        }
    }
}

void print_usage(const char *progname) {
    printf("Uso corretto: %s [-s server] [-p port] -r \"type city\"\n", progname);
}


int parse(int argc, char *argv[],char *server_ip, int *port, char *type, char *city)
{
    int found_r = 0;

    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "-s") == 0) {
            if (i+1 >= argc) return 0;
            strncpy(server_ip, argv[i+1], 63);
            server_ip[63] = '\0';
            i++;
            continue;
        }

        if (strcmp(argv[i], "-p") == 0) {
            if (i+1 >= argc) return 0;
            *port = atoi(argv[i+1]);
            if (*port <= 0 || *port > 65535) return 0;
            i++;
            continue;
        }

        if (strcmp(argv[i], "-r") == 0) {
            if (i+1 >= argc) return 0;

            *type = argv[i+1][0];

            char *ptr = argv[i+1] + 1;
            while (*ptr == ' ') ptr++;

            strncpy(city, ptr, CITY_MAX-1);
            city[CITY_MAX-1] = '\0';

            found_r = 1;

            if (i+2 != argc) return 0;

            break;
        }

        return 0;
    }

    return found_r;
}


int main(int argc, char *argv[]) {

#if defined WIN32
    WSADATA d;
    if (WSAStartup(MAKEWORD(2,2), &d) != 0)
        return -1;
#endif

    char server_ip[64] = "127.0.0.1";
    int port = SERVER_PORT;
    char type = 0;
    char city[CITY_MAX] = {0};

    if (!parse(argc, argv, server_ip, &port, &type, city)) {
    	print_usage(argv[0]);
        clearwinsock();
        return 0;
    }

    int my_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (my_socket < 0)
    {
    	errorhandler("Creazione della socket fallita.\n");
    	closesocket(my_socket);
    	clearwinsock();
    		return -1;
    }


    struct sockaddr_in sad;
    memset(&sad, 0, sizeof(sad));
    sad.sin_family = AF_INET;
    sad.sin_port   = htons(port);
    sad.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(my_socket, (struct sockaddr*)&sad, sizeof(sad)) < 0) {
    	errorhandler("Connessione al server fallita.\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    weather_request_t req;
    req.type = type;
    strncpy(req.city, city, CITY_MAX);

    if (send(my_socket, (char*)&req, sizeof(req), 0) != sizeof(req)) {
    	errorhandler("send() fallita");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    weather_response_t resp;
    if (recv(my_socket, (char*)&resp, sizeof(resp), 0) <= 0) {
    	errorhandler("recv() fallita");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }

    // Controllo che lo status sia uno di quelli previsti
    if (resp.status != STATUS_OK &&
        resp.status != STATUS_CITY_UNKNOWN &&
        resp.status != STATUS_BAD_REQUEST) {
        printf("Errore: risposta non valida dal server.\n");
        closesocket(my_socket);
        clearwinsock();
        return -1;
    }


    maiuscola(city);

    printf("Ricevuto risultato dal server ip %s. ", server_ip);

    if (resp.status == STATUS_OK) {
        switch (resp.type) {
            case TYPE_TEMP:  printf("%s: Temperatura = %.1f°C\n", city, resp.value); break;
            case TYPE_HUM:   printf("%s: Umidità = %.1f%%\n", city, resp.value); break;
            case TYPE_WIND:  printf("%s: Vento = %.1f km/h\n", city, resp.value); break;
            case TYPE_PRESS: printf("%s: Pressione = %.1f hPa\n", city, resp.value); break;
        }
    }
    else if (resp.status == STATUS_CITY_UNKNOWN)
        printf("Città non disponibile\n");
    else
        printf("Richiesta non valida\n");

    closesocket(my_socket);
    clearwinsock();
    return 0;
}
