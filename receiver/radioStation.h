#ifndef RADIO_STATION_H
#define RADIO_STATION_H

#include <string>
#include <set>
#include <vector>
#include <netinet/in.h>

struct RadioStation {
    char name[64];
    char address[20];
    int dataPort;
    time_t lastSeen;
    in_port_t sin_port;
    struct in_addr sin_addr;

};

#endif