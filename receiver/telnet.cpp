//
// Created by pawel on 13.06.18.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <iostream>
#include <fstream>
#include <asm/ioctls.h>
#include <sys/ioctl.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <climits>
#include <poll.h>
#include <csignal>

#include "../common/err.h"
#include "../common/helper.h"
#include "radioStation.h"

void clearTerminal(int socket) {
    char message[] = "\e[1;1H\e[2J";
    ssize_t len = sizeof(message) / sizeof(char);
    ssize_t write_len = write(socket, message, len);
    if (len != write_len) syserr("write to socket");
}

void printStations(int socket, const std::vector<RadioStation> &stations, int selection) {
    clearTerminal(socket);

    std::string data("-----------------------\r\nRadio SIK\r\n-----------------------\r\n");
    int i = 0;
    for (auto station : stations) {
        if (selection == i) {
            data.append("->");
        } else {
            data.append("  ");
        }
        std::string stationName(station.name);
        data.append(stationName);
        data.append("\r\n");
        i++;
    }


    const char *toWrite = data.c_str();
    std::string toWritString(data.c_str());
    write(socket, toWrite, data.length() + 1);
}

int isUp(int length, char buffer[]) {
    return length == 3 && buffer[0] == 27 && buffer[1] == 91 && buffer[2] == 65;
}

int isDown(int length, char buffer[]) {
    return length == 3 && buffer[0] == 27 && buffer[1] == 91 && buffer[2] == 66;
}


int telnet(int uiPort, std::atomic<int> &mainSession, std::atomic<bool> &changeUi, std::atomic<int> &stationSelection,
           std::vector<RadioStation> *radioStations, std::mutex &radioStationsMutex) {

    bool finish = false;

    struct pollfd client[_POSIX_OPEN_MAX];
    struct sockaddr_in server;
    char buf[2048];
    size_t length;
    ssize_t rval;
    int msgsock, activeClients, i, ret;

    /* Po Ctrl-C kończymy */
//    if (signal(SIGINT, catch_int) == SIG_ERR) {
//        perror("Unable to change signal handler");
//        exit(EXIT_FAILURE);
//    }

    /* Inicjujemy tablicę z gniazdkami klientów, client[0] to gniazdko centrali */
    for (i = 0; i < _POSIX_OPEN_MAX; ++i) {
        client[i].fd = -1;
        client[i].events = POLLIN;
        client[i].revents = 0;
    }
    activeClients = 0;

    /* Tworzymy gniazdko centrali */
    client[0].fd = socket(PF_INET, SOCK_STREAM, 0);
    if (client[0].fd < 0) {
        perror("Opening stream socket");
        exit(EXIT_FAILURE);
    }

    /* Co do adresu nie jesteśmy wybredni */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(uiPort);
    if (bind(client[0].fd, (struct sockaddr*)&server,
             (socklen_t)sizeof(server)) < 0) {
        perror("Binding stream socket");
        exit(EXIT_FAILURE);
    }

    /* Dowiedzmy się, jaki to port i obwieśćmy to światu */
    length = sizeof(server);
    if (getsockname (client[0].fd, (struct sockaddr*)&server,
                     (socklen_t*)&length) < 0) {
        perror("Getting socket name");
        exit(EXIT_FAILURE);
    }

    /* Zapraszamy klientów */
    if (listen(client[0].fd, 3) == -1) {
        perror("Starting to listen");
        exit(EXIT_FAILURE);
    }

    /* Do pracy */
    std::vector<RadioStation> currentStations;
    do {
        for (i = 0; i < _POSIX_OPEN_MAX; ++i)
            client[i].revents = 0;

        /* Po Ctrl-C zamykamy gniazdko centrali */
        if (finish && client[0].fd >= 0) {
            if (close(client[0].fd) < 0)
                perror("close");
            client[0].fd = -1;
        }

        if (changeUi.exchange(false)) {
            radioStationsMutex.lock();
            currentStations = *radioStations;
            radioStationsMutex.unlock();

            for (int i = 1; i < _POSIX_OPEN_MAX; i++) {
                if (client[i].fd != -1) {
                    printStations(client[i].fd, currentStations, stationSelection.load());
                }
            }
        }


        /* Czekamy przez 100 ms */
        ret = poll(client, _POSIX_OPEN_MAX, 100);
        if (ret < 0)
            perror("poll");
        else if (ret > 0) {
            if (!finish && (client[0].revents & POLLIN)) {
                msgsock = accept(client[0].fd, (struct sockaddr*)0, (socklen_t*)0);
                if (msgsock == -1)
                    perror("accept");
                else {
                    for (i = 1; i < _POSIX_OPEN_MAX; ++i) {
                        if (client[i].fd == -1) {
                            client[i].fd = msgsock;
                            activeClients += 1;

                            unsigned char initMessage[] = {255, 251, 1}; //IAC WILL ECHO
                            size_t expected_len = sizeof(initMessage) / sizeof(char);
                            ssize_t write_len = write(msgsock, initMessage, expected_len);

                            if (expected_len != write_len) syserr("write to socket");

                            initMessage[2] = 3;//IAC WILL SUPPRESS GO AHEAD
                            write_len = write(msgsock, initMessage, expected_len);
                            if (expected_len != write_len) syserr("write to socket");

                            changeUi.store(true);

                            break;
                        }
                    }
                    if (i >= _POSIX_OPEN_MAX) {
                        fprintf(stderr, "Too many clients\n");
                        if (close(msgsock) < 0)
                            perror("close");
                    }
                }
            }
            for (i = 1; i < _POSIX_OPEN_MAX; ++i) {
                if (client[i].fd != -1
                    && (client[i].revents & (POLLIN | POLLERR))) {
                    rval = read(client[i].fd, buf, sizeof(buf));
                    if (rval < 0) {
//                        perror("Reading stream message");
                        if (close(client[i].fd) < 0)
                            perror("close");
                        client[i].fd = -1;
                        activeClients -= 1;
                    } else if (rval == 0) {
//                        fprintf(stderr, "Ending connection\n");
                        if (close(client[i].fd) < 0)
                            perror("close");
                        client[i].fd = -1;
                        activeClients -= 1;
                    } else {
                        if (isUp(rval, buf)) {
                            if (stationSelection >= 1) {
                                stationSelection--;
                                changeUi.store(true);
                                mainSession++;
                            }
                        } else if (isDown(rval, buf)) {
                            if (stationSelection < currentStations.size() - 1) {
                                stationSelection++;
                                changeUi.store(true);
                                mainSession++;
                            }
                        }
                    }
                }
            }
        }

    } while (!finish);

    return 0;
}

