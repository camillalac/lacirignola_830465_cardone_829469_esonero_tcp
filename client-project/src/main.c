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
#include <string.h>

#include "protocol.h"

void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
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

void usage(const char* p) {
    printf("\nUso corretto:\n");
    printf("  %s [-s server_ip] [-p port] -r \"<type> <city>\"\n", p);
}

int parse(int argc, char *argv[],
          char *server_ip, int *port,
          char *type, char *city)
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
        usage(argv[0]);
        clearwinsock();
        return 0;
    }

    int s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) return 0;

    struct sockaddr_in sad;
    memset(&sad, 0, sizeof(sad));
    sad.sin_family = AF_INET;
    sad.sin_port   = htons(port);
    sad.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(s, (struct sockaddr*)&sad, sizeof(sad)) < 0) {
        printf("Connessione al server fallita.\n");
        closesocket(s);
        clearwinsock();
        return -1;
    }

    weather_request_t req;
    req.type = type;
    strncpy(req.city, city, CITY_MAX);

    if (send(s, (char*)&req, sizeof(req), 0) != sizeof(req)) {
        closesocket(s);
        clearwinsock();
        return -1;
    }

    weather_response_t resp;
    if (recv(s, (char*)&resp, sizeof(resp), 0) <= 0) {
        closesocket(s);
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

    closesocket(s);
    clearwinsock();
    return 0;
}
