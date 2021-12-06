CC = gcc
CCFLAGS = -lm -pthread
FLAGS = -g
INCLUDE = -I ./include
SRC = ./src
BIN = ./bin

all: main1 main2

main1: $(SRC)/travelMonitorClient.c
	$(CC) -g $(INCLUDE) -o $(BIN)/travelMonitorClient $(SRC)/travelMonitorClient.c $(SRC)/bloomfilter.c $(SRC)/CountryReferenceList.c $(SRC)/date.c $(SRC)/requests.c $(CCFLAGS)

main2: $(SRC)/monitorServer.c
	$(CC) $(INCLUDE) -o $(BIN)/monitorServer $(SRC)/monitorServer.c $(SRC)/bloomfilter.c $(SRC)/BST.c $(SRC)/date.c $(SRC)/countryTree.c $(SRC)/linkedList.c $(SRC)/skipList.c $(SRC)/Virus.c $(SRC)/cyclicBuffer.c $(CCFLAGS)

clean:
	rm $(BIN)/vaccineMonitorClient $(BIN)/monitorServer