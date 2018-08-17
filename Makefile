CC = g++
CFLAGS = -Wall -g -std=c++14 -pthread

all: sikradio-sender sikradio-receiver

#common

err.o: common/err.cpp
	$(CC) $(CFLAGS) -o err.o -c common/err.cpp

helper.o: common/helper.cpp
	$(CC) $(CFLAGS) -o helper.o -c common/helper.cpp

logger.o: common/logger.cpp
	$(CC) $(CFLAGS) -o logger.o -c common/logger.cpp

#sender

sender.o: sender/sender.cpp
	$(CC) $(CFLAGS) -o sender.o -c sender/sender.cpp

sikradio-sender: err.o helper.o logger.o sender.o
	$(CC) $(CFLAGS) -o sikradio-sender err.o helper.o logger.o sender.o

#receiver

discover.o: receiver/discover.cpp
	$(CC) $(CFLAGS) -o discover.o -c receiver/discover.cpp

outUploader.o: receiver/outUploader.cpp
	$(CC) $(CFLAGS) -o outUploader.o -c receiver/outUploader.cpp

retransmit.o: receiver/retransmit.cpp
	$(CC) $(CFLAGS) -o retransmit.o -c receiver/retransmit.cpp

telnet.o: receiver/telnet.cpp
	$(CC) $(CFLAGS) -o telnet.o -c receiver/telnet.cpp

receiver.o: receiver/receiver.cpp
	$(CC) $(CFLAGS) -o receiver.o -c receiver/receiver.cpp

sikradio-receiver: err.o helper.o logger.o discover.o outUploader.o retransmit.o telnet.o receiver.o
	$(CC) $(CFLAGS) -o sikradio-receiver err.o helper.o logger.o discover.o outUploader.o retransmit.o telnet.o receiver.o

clean:
	rm err.o
	rm helper.o
	rm logger.o
	rm sender.o
	rm discover.o
	rm outUploader.o
	rm retransmit.o
	rm telnet.o
	rm receiver.o