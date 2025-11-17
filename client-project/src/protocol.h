#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <stdint.h>

//
// ---------------- COSTANTI DEL PROTOCOLLO ----------------
//

// Porta di default
#define SERVER_PORT 56700

// Lunghezza massima nome città
#define CITY_MAX 64

// Size della listen queue
#define QUEUE_SIZE 5

// Tipi di richiesta validi
#define TYPE_TEMP  't'
#define TYPE_HUM   'h'
#define TYPE_WIND  'w'
#define TYPE_PRESS 'p'

// Codici di stato
#define STATUS_OK            0u
#define STATUS_CITY_UNKNOWN  1u
#define STATUS_BAD_REQUEST   2u

//
// ---------------- STRUTTURE DEL PROTOCOLLO ----------------
//

typedef struct {
    char type;                   // 't','h','w','p'
    char city[CITY_MAX];         // stringa città null-terminated
} weather_request_t;

typedef struct {
    unsigned int status;         // 0,1,2
    char type;                   // eco del tipo richiesto
    float value;                 // valore meteo
} weather_response_t;


//
// ---------------- FUNZIONI (solo server) ----------------
//

// Generatori valori meteo
float get_temperature(void);
float get_humidity(void);
float get_wind(void);
float get_pressure(void);

// Verifica città supportata (case-insensitive)
int is_valid_city(const char* c);

#endif /* PROTOCOL_H_ */

