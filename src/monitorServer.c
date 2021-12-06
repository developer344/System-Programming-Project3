#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <netdb.h>

#include "linkedList.h"
#include "BST.h"
#include "cyclicBuffer.h"

int acceptedRequests = 0;
int rejectedRequests = 0;

char **divideStr(char *str, int buffer_size, int n)
{

    int str_size = strlen(str);
    char **str_parts = malloc(sizeof(char *) * n);
    int index = 0;
    for (int i = 0; i < n; i++)
    {
        if (strlen(str + (i * buffer_size)) < buffer_size)
        {
            str_parts[i] = malloc(sizeof(char) * strlen(str + (i * buffer_size)));
            strncpy(str_parts[i], str + (i * buffer_size), strlen(str + (i * buffer_size)));
        }
        else
        {
            str_parts[i] = malloc(sizeof(char) * buffer_size);
            strncpy(str_parts[i], str + (i * buffer_size), buffer_size);
        }
    }
    return str_parts;
}

pthread_mutex_t cycBufferMutex;
pthread_mutex_t countryTreeMutex;
pthread_mutex_t citizenTreeMutex;
pthread_mutex_t virusListMutex;
pthread_mutex_t monitorBloomMutex;
pthread_mutex_t general_mutex;

//Struct that allowes the main thread to pass the arguments to the other threads
struct thread_input
{
    cyclicBufferPtr buffer;
    CTPtr countryTree;
    BSTPtr citizenTree;
    linkedListPtr virusList;
    bloomFilterPtr monitorBloom;
    int bloomSize;
} thread_input;

void *routine(void *input)
{
    CTPtr countryTree = ((struct thread_input *)input)->countryTree;
    BSTPtr citizenTree = (((struct thread_input *)input))->citizenTree;
    linkedListPtr virusList = ((struct thread_input *)input)->virusList;
    bloomFilterPtr monitorBloom = ((struct thread_input *)input)->monitorBloom;
    int bloomsize = ((struct thread_input *)input)->bloomSize;
    cyclicBufferPtr cyclicBuff = ((struct thread_input *)input)->buffer;
    virusPtr currVirus;
    citizenRecordPtr record;
    citizenRecordPtr duplicate;
    cTreeNodePtr countryNode;
    int numOfFiles = 0;

    while (1)
    {
        pthread_mutex_lock(&cycBufferMutex);
        int isEmpty = cycBuffer_checkEmpty(cyclicBuff);
        if (isEmpty)
        {
            break;
        }
        char *path = cycBuffer_popItem(cyclicBuff);
        pthread_mutex_unlock(&cycBufferMutex);
        printf("Inserting File: %s\n", path);
        FILE *inputFile = NULL;
        inputFile = fopen(path, "r");

        if (inputFile == NULL)
        {
            printf("errno = %d\n", errno);
            perror("fopen");
        }
        //This will hold the data from its line that is read
        char line[100];
        //This will hold the data from its line that is read separetly
        char data[10][30];
        //To be used on the separation of the line inyo its compoonents
        char *key;

        //------------------------------------Reading File------------------------------------//
        pthread_mutex_lock(&general_mutex);
        while (fgets(line, sizeof(line), inputFile))
        {
            // printf("gg:Date:%s\n", line);

            //Reading line by line from the file given
            line[strcspn(line, "\n")] = 0;
            //Separating the lines into the usefull info
            key = strtok(line, " ");
            for (int i = 0; key != NULL; i++)
            {
                strcpy(data[i], key);
                key = strtok(NULL, " ");
            }
            pthread_mutex_unlock(&general_mutex);
            //Checking if citizen id is within the limits

            if (atoi(data[0]) > 0 && atoi(data[0]) < 10000)
            {
                //Checking if age is valid
                if (atoi(data[4]) > 0)
                {
                    pthread_mutex_lock(&general_mutex);
                    //Checking  if date is valid
                    char dateStr[10];
                    strcpy(dateStr, data[7]);
                    DatePtr date = date_init(data[7]);

                    if ((date->day > 0 && date->day <= 30 && date->month > 0 && date->month <= 12 && date->year > 1900 && date->year < 3000 && !strcmp(data[6], "YES")) || !strcmp(data[6], "NO"))
                    {
                        //Checking if a citizen is already in the citizen tree
                        pthread_mutex_lock(&citizenTreeMutex);
                        duplicate = findcitizenRecord(citizenTree, atoi(data[0]));
                        if (duplicate == NULL)
                        {
                            //Here I check if the country of the current citizen has been added to the country
                            //tree if not I create a new node in the country tree for the new country
                            pthread_mutex_lock(&countryTreeMutex);
                            if ((countryNode = countryTree_findCountry(countryTree, data[3], atoi(data[4]))) == NULL)
                                countryNode = countryTree_insertCountry(countryTree, data[3], atoi(data[4]));

                            //Creating new citizen and inserting the unique elements only so
                            //that country, virus, date and yes/no will not be duplicated
                            record = malloc(sizeof(citizenRecord));
                            record->citizenId = atoi(data[0]);
                            record->name = malloc(sizeof(char) * strlen(data[1]));
                            strcpy(record->name, data[1]);
                            record->surname = malloc(sizeof(char) * strlen(data[2]));
                            strcpy(record->surname, data[2]);
                            //For the country each citizen has a pointer to their country
                            //Citizens from the same country point to the same country node
                            record->country = countryNode;
                            record->age = atoi(data[4]);
                            //Inserting the new citizen to the tree that contains all the citizens of every country
                            if (insertcitizenRecord(citizenTree, record) < 0)
                            {
                                printf("Error: couldn't insert record in citizen tree\n");
                            }
                            //I search in the list of viruses if the virus insetred already exists or not
                            pthread_mutex_unlock(&countryTreeMutex);
                            pthread_mutex_lock(&virusListMutex);
                            currVirus = list_searchElement(virusList, data[5]);

                            if (currVirus != NULL)
                            {
                                //Then I insert the citizen to the virus
                                virus_insert(currVirus, record, data[0], data[6], dateStr);
                                if (!strcmp(data[6], "YES"))
                                {
                                    pthread_mutex_lock(&monitorBloomMutex);
                                    bloom_insertElement(monitorBloom, data[0]);
                                    pthread_mutex_unlock(&monitorBloomMutex);
                                }
                            }
                            else
                            {
                                //If the inserted virus doesn't exist in the virus list I create it and I insert it in the virus list
                                currVirus = malloc(sizeof(virus));

                                virus_init(currVirus, data[5], NUMOFHASHFUNCTIONS, bloomsize);
                                //I insert the new citizen to the newly created virus
                                virus_insert(currVirus, record, data[0], data[6], dateStr);
                                if (!strcmp(data[6], "YES"))
                                {
                                    pthread_mutex_lock(&monitorBloomMutex);
                                    bloom_insertElement(monitorBloom, data[0]);
                                    pthread_mutex_unlock(&monitorBloomMutex);
                                }
                                //I insert the new virus to the virus list
                                list_insertItem(virusList, currVirus);
                            }
                            pthread_mutex_unlock(&virusListMutex);
                        }
                        else
                        {
                            //If the inserted citizen already exists in the citizen tree then I take the citizen
                            //and insert them in the virus that was given
                            pthread_mutex_lock(&virusListMutex);
                            currVirus = list_searchElement(virusList, data[5]);
                            if (currVirus != NULL)
                            {
                                //If the virus given already exists then I check if the citizen has already has been
                                //inserted in thatvirus
                                int flag = find_inVirus(currVirus, duplicate->citizenId);
                                if (flag > 0)
                                {
                                    //If the citizen is not in that virus I insert them there
                                    virus_insert(currVirus, duplicate, data[0], data[6], dateStr);
                                    if (!strcmp(data[6], "YES"))
                                    {
                                        pthread_mutex_lock(&monitorBloomMutex);
                                        bloom_insertElement(monitorBloom, data[0]);
                                        pthread_mutex_unlock(&monitorBloomMutex);
                                    }
                                }
                                else
                                    //If the citizen is already in the virus I print error
                                    printf("ERROR IN RECORD %s\n", line);
                            }
                            else
                            {
                                //If the virus given doesn't exist I create it
                                currVirus = malloc(sizeof(virus));
                                virus_init(currVirus, data[5], NUMOFHASHFUNCTIONS, bloomsize);
                                //I insert the citizen to the newly created virus
                                virus_insert(currVirus, duplicate, data[0], data[6], dateStr);
                                if (!strcmp(data[6], "YES"))
                                {
                                    pthread_mutex_lock(&monitorBloomMutex);
                                    bloom_insertElement(monitorBloom, data[0]);
                                    pthread_mutex_unlock(&monitorBloomMutex);
                                }
                                //I insert the new virus to the virus list
                                list_insertItem(virusList, currVirus);
                            }
                            pthread_mutex_unlock(&virusListMutex);
                        }
                        pthread_mutex_unlock(&citizenTreeMutex);
                    }
                    else
                    {
                        printf("ERROR! Date is invalid please enter a valid date %s in record %s\n", dateStr, line);
                    }
                    dateDestructor(date);

                    pthread_mutex_unlock(&general_mutex);
                }
                else
                    printf("ERROR! Age is a negatice number in record %s\n", line);
            }
            else
                printf("ERROR! Citizen id is out of limits in record %s\n", line);
            pthread_mutex_lock(&general_mutex);
        }
        pthread_mutex_unlock(&general_mutex);
        fclose(inputFile);
        free(path);
    }
    pthread_mutex_unlock(&cycBufferMutex);
}

int readint(int socket, int socketBufferSize)
{
    int returnInt = 0;
    if (socketBufferSize >= 4)
        read(socket, &returnInt, 4);
    else
    {
        short firsthalf, lasthalf;
        read(socket, &lasthalf, 2);
        read(socket, &firsthalf, 2);
        returnInt = (firsthalf << 16) | lasthalf;
    }
    return returnInt;
}

void writeint(int socket, int integer, int socketBufferSize)
{
    if (socketBufferSize >= 4)
        write(socket, &integer, 4);
    else
    {
        short lasthalf = integer & 0xFFFF;
        short firsthalf = integer >> 16;
        write(socket, &lasthalf, 2);
        write(socket, &firsthalf, 2);
    }
}

char *readstr(int socket, int socketBufferSize)
{
    int strln = readint(socket, socketBufferSize);
    char *returnStr = malloc(sizeof(char) * (strln + 1));
    memset(returnStr, 0, strln + 1);
    int n = strln / socketBufferSize;
    int n_plus = 0;
    n_plus = strln % socketBufferSize > 0 ? 1 : 0;
    int j = 0;
    for (j = 0; j < n; j++)
    {
        read(socket, returnStr + (j * socketBufferSize), socketBufferSize);
    }
    if (n_plus)
    {
        read(socket, returnStr + (j * socketBufferSize), strln % socketBufferSize);
        returnStr[strln] = '\0';
    }
    return returnStr;
}

void writestr(int socket, char *str, int socketBufferSize)
{
    char *vstr = malloc(sizeof(char) * strlen(str));
    strcpy(vstr, str);
    int strln = strlen(vstr);
    writeint(socket, strln, socketBufferSize);
    int n = strln / socketBufferSize;
    int n_plus = strln % socketBufferSize > 0 ? 1 : 0;
    char **splitMessage = divideStr(vstr, socketBufferSize, n + n_plus);

    int i = 0;
    for (i = 0; i < n; i++)
    {
        write(socket, splitMessage[i], socketBufferSize);
    }
    if (n_plus)
    {
        write(socket, splitMessage[i], strln % socketBufferSize);
    }
    for (int i = 0; i < n; i++)
    {
        free(splitMessage[i]);
    }
    free(splitMessage);
    free(vstr);
}

int main(int argc, char **argv)
{
    int cyclicBufferSize;
    int socketBufferSize;
    int numThreads;
    int port;
    int bloomsize;
    //Reading port
    for (int i = 0; i < argc - 1; i++)
        if (!strcmp(argv[i], "-p"))
        {
            //Multipluing by 8 to convert from bytes to bits
            port = atoi(argv[i + 1]);
            break;
        }
    //Reading the number of bytes that the bloomfilter bit array will have
    for (int i = 0; i < argc - 1; i++)
        if (!strcmp(argv[i], "-s"))
        {
            //Multipluing by 8 to convert from bytes to bits
            bloomsize = 8 * (atoi(argv[i + 1]));
            break;
        }
    //Reading the size of the socket buffer
    for (int i = 0; i < argc - 1; i++)
        if (!strcmp(argv[i], "-b"))
        {
            socketBufferSize = atoi(argv[i + 1]);
            break;
        }
    //Reading the size of the cyclic buffer
    for (int i = 0; i < argc - 1; i++)
        if (!strcmp(argv[i], "-c"))
        {
            cyclicBufferSize = atoi(argv[i + 1]);
            break;
        }
    //Reading the number of threads
    for (int i = 0; i < argc - 1; i++)
        if (!strcmp(argv[i], "-t"))
        {
            numThreads = atoi(argv[i + 1]);
            break;
        }

    char server_message[256] = "HELLO FROM SERVER WHAT CAN I DO FOR YOU";
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    //Finding Host address
    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);
    struct hostent *hoststuct;
    hoststuct = gethostbyname(hostname);
    struct in_addr address;
    memcpy(&address, *hoststuct->h_addr_list, sizeof(address));
    //Creating server info
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = address.s_addr;

    //Binding and listening
    bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    listen(server_socket, 100);
    int client_socket = accept(server_socket, NULL, NULL);
    int numberOfPaths = argc - 11;
    char path[numberOfPaths][20];
    for (int i = 0; i < numberOfPaths; i++)
        strcpy(path[i], argv[i + 11]);
    //Creating data structures
    CTPtr countryTree = malloc(sizeof(CT));
    countryTree_init(countryTree);

    BSTPtr citizenTree = malloc(sizeof(BST));
    BST_init(citizenTree);

    linkedListPtr virusList = malloc(sizeof(linkedList));
    list_init(virusList);

    bloomFilterPtr monitorBloom = malloc(sizeof(bloomFilter));
    bloomFilter_init(monitorBloom, NUMOFHASHFUNCTIONS, bloomsize);
    cyclicBufferPtr cycBuffer = malloc(sizeof(cyclicBuffer));
    cycBuffer_init(cycBuffer, cyclicBufferSize);
    /////////////////////////////////////
    struct dirent *in_file;
    int numOfFilesPerDir = 0;
    char *currpath = path[0];
    DIR *input_dir = opendir(currpath);
    while ((in_file = readdir(input_dir)) != NULL)
    {
        if (!strcmp(in_file->d_name, "."))
            continue;
        if (!strcmp(in_file->d_name, ".."))
            continue;
        numOfFilesPerDir++;
    }
    //Increasing numOfFiles because every subdirectory has n or n+1 files so to be sure there is enough space for all
    //Iincrease numOfFilesPerDir by 1
    numOfFilesPerDir++;
    char **pathToNCountry = malloc(sizeof(char *) * (numOfFilesPerDir * numberOfPaths));
    int numOfPathsToNCountry = 0;
    for (int j = 0; j < numberOfPaths; j++)
    {
        currpath = path[j];
        DIR *input_dir = opendir(currpath);
        while ((in_file = readdir(input_dir)) != NULL)
        {
            if (!strcmp(in_file->d_name, "."))
                continue;
            if (!strcmp(in_file->d_name, ".."))
                continue;

            pathToNCountry[numOfPathsToNCountry] = malloc(sizeof(char) * (strlen(in_file->d_name) + strlen(currpath) + 1));
            strcpy(pathToNCountry[numOfPathsToNCountry], currpath);
            strcat(pathToNCountry[numOfPathsToNCountry], "/");
            strcat(pathToNCountry[numOfPathsToNCountry], in_file->d_name);
            numOfPathsToNCountry++;
        }
    }
    if (numOfPathsToNCountry < (numOfFilesPerDir * numberOfPaths))
    {
        for (int i = numOfPathsToNCountry; i < (numOfFilesPerDir * numberOfPaths); i++)
        {
            free(pathToNCountry[i]);
        }
    }
    //Initializing mutexes
    pthread_mutex_init(&cycBufferMutex, NULL);
    pthread_mutex_init(&countryTreeMutex, NULL);
    pthread_mutex_init(&citizenTreeMutex, NULL);
    pthread_mutex_init(&virusListMutex, NULL);
    pthread_mutex_init(&monitorBloomMutex, NULL);
    pthread_mutex_init(&general_mutex, NULL);
    /////////////////////////////////////
    int index = 0;
    while (!cycBuffer_checkFull(cycBuffer) && index < numOfPathsToNCountry)
    {
        cycBuffer_pushItem(cycBuffer, pathToNCountry[index]);
        index++;
    }
    pthread_t t[numThreads];
    struct thread_input *input = malloc(sizeof(struct thread_input));
    input->buffer = cycBuffer;
    input->citizenTree = citizenTree;
    input->countryTree = countryTree;
    input->monitorBloom = monitorBloom;
    input->virusList = virusList;
    input->bloomSize = bloomsize;

    for (int i = 0; i < numThreads; i++)
        if (pthread_create(&t[i], NULL, &routine, input) != 0)
            perror("Failed to create thread");
    while (index < numOfPathsToNCountry)
    {
        pthread_mutex_lock(&cycBufferMutex);
        if (!cycBuffer_checkFull(cycBuffer))
        {
            cycBuffer_pushItem(cycBuffer, pathToNCountry[index]);
            index++;
        }
        pthread_mutex_unlock(&cycBufferMutex);
    }
    for (int i = 0; i < numThreads; i++)
        if (pthread_join(t[i], NULL) != 0)
            perror("Failed to join thread");
    printf("Insertion of files has finished\n");

    //-------------------------------------------------------------------------------------//
    //Sending BloomFilter
    int bloomlen = (bloomsize / 8) + (bloomsize % 8 > 0 ? 1 : 0);
    int n = bloomlen / socketBufferSize;
    int n_plus = bloomlen % socketBufferSize;
    int j;
    for (j = 0; j < n; j++)
    {
        write(client_socket, monitorBloom->bloomBitArray + (j * socketBufferSize), socketBufferSize);
    }
    if (n_plus)
    {
        write(client_socket, monitorBloom->bloomBitArray + (j * socketBufferSize), bloomlen % socketBufferSize);
    }

    //Application
    int endState = 1;
    int command = 0;
    virusPtr currVirus = NULL;
    n = 0;
    slNodePtr Node;
    while (endState > 0)
    {
        command = readint(client_socket, socketBufferSize);
        if (command == 1)
        {
            //Reading citizen id
            char *citizenId = readstr(client_socket, socketBufferSize);
            //Reading date
            char *date = readstr(client_socket, socketBufferSize);

            //Reading virusName
            char *virusName = readstr(client_socket, socketBufferSize);
            char message[5];
            if ((currVirus = list_searchElement(virusList, virusName)) != NULL)
            {
                if (virus_checkIfVaccinatedBloom(currVirus, citizenId))
                {
                    if ((Node = skipList_findRecord(currVirus->vaccinatedVirusSL, atoi(citizenId))) != NULL)
                    {
                        strcpy(message, "YES");
                        sprintf(date, "%d-%d-%d", Node->date->day, Node->date->month, Node->date->year);
                        printf("Date Of vaccination: %s\n", date);
                        acceptedRequests++;
                    }
                    else
                    {
                        strcpy(message, "NO");
                        rejectedRequests++;
                    }
                }
                else
                {
                    strcpy(message, "NO");
                    rejectedRequests++;
                }
            }
            else
            {
                strcpy(message, "NO");
                rejectedRequests++;
            }

            //Sending back answer
            writestr(client_socket, message, socketBufferSize);
            //Sending date if message is YES
            if (!strcmp(message, "YES"))
            {
                writestr(client_socket, date, socketBufferSize);
            }
            free(citizenId);
            free(date);
            free(virusName);
        }
        else if (command == 3)
        {
            int numberOfNewFiles = 0;
            //Checking how many new Files are thre
            for (int j = 0; j < numberOfPaths; j++)
            {
                currpath = path[j];
                DIR *input_dir = opendir(currpath);
                while ((in_file = readdir(input_dir)) != NULL)
                {
                    if (!strcmp(in_file->d_name, "."))
                        continue;
                    if (!strcmp(in_file->d_name, ".."))
                        continue;
                    int flag = 1;
                    for (int index = 0; index < numOfPathsToNCountry; index++)
                    {
                        char *ccpath = malloc(sizeof(char) * (strlen(in_file->d_name) + strlen(currpath) + 1));
                        strcpy(ccpath, currpath);
                        strcat(ccpath, "/");
                        strcat(ccpath, in_file->d_name);
                        if (!strcmp(pathToNCountry[index], ccpath))
                        {
                            flag = 0;
                            free(ccpath);
                            break;
                        }
                        free(ccpath);
                    }
                    if (flag)
                        numberOfNewFiles++;
                }
            }
            //If there are more than 0 I procces them
            if (numberOfNewFiles)
            {
                //Sending 1 if there are new files to add so travelMonitor client can know
                //to expect bloomfilter to be resent
                n = 1;
                writeint(client_socket, n, socketBufferSize);
                char *newFilesPaths[numberOfNewFiles];
                int newFilesIndex = 0;
                //Finding all paths from new files
                for (int j = 0; j < numberOfPaths; j++)
                {
                    currpath = path[j];
                    DIR *input_dir = opendir(currpath);
                    while ((in_file = readdir(input_dir)) != NULL)
                    {
                        if (!strcmp(in_file->d_name, "."))
                            continue;
                        if (!strcmp(in_file->d_name, ".."))
                            continue;

                        int flag = 1;
                        for (int index = 0; index < numOfPathsToNCountry; index++)
                        {
                            char *ccpath = malloc(sizeof(char) * (strlen(in_file->d_name) + strlen(currpath) + 1));
                            strcpy(ccpath, currpath);
                            strcat(ccpath, "/");
                            strcat(ccpath, in_file->d_name);
                            if (!strcmp(pathToNCountry[index], ccpath))
                            {
                                flag = 0;
                                break;
                            }
                        }
                        if (flag)
                        {
                            newFilesPaths[newFilesIndex] = malloc(sizeof(char) * (strlen(in_file->d_name) + strlen(currpath) + 1));
                            strcpy(newFilesPaths[newFilesIndex], currpath);
                            strcat(newFilesPaths[newFilesIndex], "/");
                            strcat(newFilesPaths[newFilesIndex], in_file->d_name);
                            newFilesIndex++;
                        }
                    }
                }
                //Inserting the new paths into the cyclic buffer
                int index = 0;
                while (!cycBuffer_checkFull(cycBuffer) && index < numberOfNewFiles)
                {
                    cycBuffer_pushItem(cycBuffer, newFilesPaths[index]);
                    index++;
                }
                for (int i = 0; i < numThreads; i++)
                    if (pthread_create(&t[i], NULL, &routine, input) != 0)
                        perror("Failed to create thread");
                //Inserting the remaining paths to the buffer while the threads are processing the rest of the files
                while (index < numberOfNewFiles)
                {
                    pthread_mutex_lock(&cycBufferMutex);
                    if (!cycBuffer_checkFull(cycBuffer))
                    {
                        cycBuffer_pushItem(cycBuffer, newFilesPaths[index]);
                        index++;
                    }
                    pthread_mutex_unlock(&cycBufferMutex);
                }
                //Joining threads so the main thread will wait for the rest of the threads to finish
                for (int i = 0; i < numThreads; i++)
                    if (pthread_join(t[i], NULL) != 0)
                        perror("Failed to join thread");
                //Sending back bloomFilters
                bloomlen = (bloomsize / 8) + (bloomsize % 8 > 0 ? 1 : 0);
                n = bloomlen / socketBufferSize;
                n_plus = bloomlen % socketBufferSize;
                for (j = 0; j < n; j++)
                {
                    write(client_socket, monitorBloom->bloomBitArray + (j * socketBufferSize), socketBufferSize);
                }
                if (n_plus)
                {
                    write(client_socket, monitorBloom->bloomBitArray + (j * socketBufferSize), bloomlen % socketBufferSize);
                }
                //Adding the new files to the already inserted files array
                char **tempArray = malloc(sizeof(char *) * (numOfPathsToNCountry + numberOfNewFiles));
                for (int i = 0; i < numOfPathsToNCountry; i++)
                {
                    tempArray[i] = malloc(sizeof(char) * strlen(pathToNCountry[i]));
                    strcpy(tempArray[i], pathToNCountry[i]);
                    free(pathToNCountry[i]);
                }
                free(pathToNCountry);
                for (int i = 0; i < numberOfNewFiles; i++)
                {
                    tempArray[i + numOfPathsToNCountry] = malloc(sizeof(char) * strlen(newFilesPaths[i]));
                    strcpy(tempArray[i + numOfPathsToNCountry], newFilesPaths[i]);
                    free(newFilesPaths[i]);
                }
                numOfPathsToNCountry = numOfPathsToNCountry + numberOfNewFiles;
                for (int i = 0; i < numOfPathsToNCountry; i++)
                {
                    pathToNCountry[i] = malloc(sizeof(char) * strlen(tempArray[i]));
                    strcpy(pathToNCountry[i], tempArray[i]);
                    free(tempArray[i]);
                }
                free(tempArray);
            }
            else
            {
                //Sending 0 if there are no new files to add to travelMonitor client can know
                //not to expect bloomfilter to be resent
                n = 0;
                writeint(client_socket, n, socketBufferSize);
                printf("There are no new files to add\n");
            }
        }
        else if (command == 4)
        {
            //Recieving citizen ID
            char *citizenId = readstr(client_socket, socketBufferSize);
            //Searching if citizen id is in the citizen tree
            int message = 0;
            citizenRecordPtr record = NULL;
            if ((record = findcitizenRecord(citizenTree, atoi(citizenId))) != NULL)
            {
                message = 1;
            }

            //Sending back to the travel monitor the results
            writeint(client_socket, message, socketBufferSize);
            if (message)
            {
                //Sending citizen name
                writestr(client_socket, record->name, socketBufferSize);

                //Sending citizen surname
                writestr(client_socket, record->surname, socketBufferSize);
                //Sending citizen age
                writeint(client_socket, record->age, socketBufferSize);

                //Sending citizen country
                char *ccountry = malloc(sizeof(char) * strlen(record->country->countryName));
                strcpy(ccountry, record->country->countryName);
                writestr(client_socket, ccountry, socketBufferSize);
                free(ccountry);
                char vacToViruses[virusList->numOfElements][20];
                char dates[virusList->numOfElements][12];
                int vacIndex = 0;
                char nonVacToViruses[virusList->numOfElements][20];
                int nonVacIndex = 0;
                listNodePtr currVi = virusList->Begining;
                while (currVi != NULL)
                {
                    if (find_inVirus(currVi->virus, record->citizenId) < 0)
                    {
                        if ((Node = skipList_findRecord(currVi->virus->vaccinatedVirusSL, record->citizenId)) != NULL)
                        {

                            sprintf(dates[vacIndex], "%d-%d-%d", Node->date->day, Node->date->month, Node->date->year);
                            strcpy(vacToViruses[vacIndex], currVi->virus->name);
                            vacIndex++;
                        }
                        else if (skipList_findRecord(currVi->virus->notVaccinatedVirusSL, record->citizenId) != NULL)
                        {
                            strcpy(nonVacToViruses[nonVacIndex], currVi->virus->name);
                            nonVacIndex++;
                        }
                    }

                    currVi = currVi->nextNode;
                }
                int totalViruses = vacIndex + nonVacIndex;
                writeint(client_socket, totalViruses, socketBufferSize);
                for (int ii = 0; ii < vacIndex; ii++)
                {
                    //Sending virus name
                    writestr(client_socket, vacToViruses[ii], socketBufferSize);
                    //Sending yes vaccination
                    writestr(client_socket, "YES", socketBufferSize);

                    //Sending date of vaccination
                    writestr(client_socket, dates[ii], socketBufferSize);
                }
                for (int ii = 0; ii < nonVacIndex; ii++)
                {
                    //Sending virus name
                    writestr(client_socket, nonVacToViruses[ii], socketBufferSize);

                    //Sending no vaccination
                    writestr(client_socket, "NO", socketBufferSize);
                }
            }
        }
        endState = command;
    }
    //Logfile creation
    char logfilename[25];
    sprintf(logfilename, "log_files/log_file.%d", getpid());
    printf("logfile: %s\n", logfilename);
    FILE *log_file = fopen(logfilename, "w");
    char data[2][20];
    for (int i = 0; i < numberOfPaths; i++)
    {
        char *key = strtok(path[i], "/");
        for (int i = 0; key != NULL; i++)
        {
            strcpy(data[i], key);
            key = strtok(NULL, "/");
        }
        fprintf(log_file, "%s\n", data[1]);
    }
    fprintf(log_file, "TOTAL TRAVEL REQUESTS %d\nACCEPTED %d\nREJECTED %d\n", acceptedRequests + rejectedRequests, acceptedRequests, rejectedRequests);
    fclose(log_file);
    //Destructions of data structures
    countryTree_destructor(countryTree);
    list_deleteList(virusList);
    BST_destructor(citizenTree);
    pthread_mutex_destroy(&cycBufferMutex);
    pthread_mutex_destroy(&countryTreeMutex);
    pthread_mutex_destroy(&citizenTreeMutex);
    pthread_mutex_destroy(&virusListMutex);
    pthread_mutex_destroy(&monitorBloomMutex);
    pthread_mutex_destroy(&general_mutex);
    close(client_socket);
    close(server_socket);
    for (int i = 0; i < numOfPathsToNCountry; i++)
    {
        free(pathToNCountry[i]);
    }
    free(pathToNCountry);
    printf("Goodbye from monitor\n");
    return 0;
}