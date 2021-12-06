#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include "CountryReferenceList.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>

#include "bloomfilter.h"
#include "date.h"
#include "requests.h"

#define BASEPORT 9000
#define SADDR struct sockaddr

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

char *curr_date()
{
    //------------------------------------Current date------------------------------------//
    //This function calculates the current date every time its called and returns
    //a string of the date
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char year[5];
    char month[4];
    char day[4];
    char *currDate = malloc(sizeof(char) * 12);
    sprintf(year, "%d", tm.tm_year + 1900);
    sprintf(month, "%d", tm.tm_mon + 1);
    sprintf(day, "%d", tm.tm_mday);
    strcpy(currDate, day);
    strcat(currDate, "-");
    strcat(currDate, month);
    strcat(currDate, "-");
    strcat(currDate, year);
    return currDate;
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
    //Checking the number of arguments
    if (argc != 13)
    {
        perror("ERROR: Wrong number of arguments!");
        exit(1);
    }
    int numMonitors;
    int socketBufferSize;
    int sizeOfBloom;
    int cyclicBufferSize;
    char *bloomSize;
    char *input_dir;
    int numThreads;
    //Reading the number of monitors that the application will create during its run time
    for (int i = 0; i < argc - 1; i++)
    {
        if (!strcmp(argv[i], "-m"))
        {
            numMonitors = atoi(argv[i + 1]);
            break;
        }
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
    //Reading the number of bytes that the bloomfilter bit array will have
    for (int i = 0; i < argc - 1; i++)

        if (!strcmp(argv[i], "-s"))
        {
            //Multipluing by 8 to convert from bytes to bits
            sizeOfBloom = 8 * (atoi(argv[i + 1]));
            bloomSize = malloc(sizeof(char) * (strlen(argv[i + 1]) + 1));
            strcpy(bloomSize, argv[i + 1]);
            break;
        }
    //Reading the input directory from the command line
    for (int i = 0; i < argc - 1; i++)
        if (!strcmp(argv[i], "-i"))
        {
            input_dir = malloc(sizeof(char) * strlen(argv[i + 1]));
            strcpy(input_dir, argv[i + 1]);
            break;
        }
    //Reading the number of threads
    for (int i = 0; i < argc - 1; i++)
        if (!strcmp(argv[i], "-t"))
        {
            numThreads = atoi(argv[i + 1]);
            break;
        }
    printf("Number of monitors: %d\nSocket Buffer size: %d\nSize of cyclic Buffer: %d\nSize of bloomfilter: %d\nInput Directory: %s\nNumber of threads: %d\n", numMonitors, socketBufferSize, cyclicBufferSize, sizeOfBloom, input_dir, numThreads);

    //Opening input directory and sending subdirectories to the monitors

    int numOfCountriesPerMonitor[numMonitors];
    for (int i = 0; i < numMonitors; i++)
        numOfCountriesPerMonitor[i] = 0;

    struct dirent *in_file;
    DIR *inputDir = opendir(input_dir);
    if (inputDir == NULL)
    {
        printf("Could not open current directory");
        exit(1);
    }
    int monitor_index = 0;
    CountryRefListPtr CList = malloc(sizeof(CountryRefList));

    CountryRefListInit(CList);
    while ((in_file = readdir(inputDir)) != NULL)
    {
        //Only the true files will be used
        if (!strcmp(in_file->d_name, "."))
            continue;
        if (!strcmp(in_file->d_name, ".."))
            continue;
        char *countryName = malloc(sizeof(char) * strlen(in_file->d_name));
        strcpy(countryName, in_file->d_name);
        CRLinsertRecord(CList, countryName, monitor_index);
        numOfCountriesPerMonitor[monitor_index]++;
        monitor_index = (monitor_index + 1) % numMonitors;
        free(countryName);
    }

    //Creating monitors
    pid_t pid[numMonitors];
    for (int i = 0; i < numMonitors; i++)
    {

        pid[i] = fork();
        if (pid[i] == -1)
        {
            printf("Error in fork");
        }
        else if (pid[i] == 0)
        {
            char *arguments[12 + numOfCountriesPerMonitor[i]];
            for (int count = 0; count < 11 + numOfCountriesPerMonitor[i]; count++)
            {
                arguments[count] = malloc(sizeof(sizeof(char) * 30));
            }
            strcpy(arguments[0], "./bin/monitorServer");
            strcpy(arguments[1], "-p");
            sprintf(arguments[2], "%d", BASEPORT + i);
            strcpy(arguments[3], "-t");
            sprintf(arguments[4], "%d", numThreads);
            strcpy(arguments[5], "-b");
            sprintf(arguments[6], "%d", socketBufferSize);
            strcpy(arguments[7], "-c");
            sprintf(arguments[8], "%d", cyclicBufferSize);
            strcpy(arguments[9], "-s");
            strcpy(arguments[10], bloomSize);
            CountryRefListNodePtr currNode = CList->FirstNode;
            int index = 11;
            while (currNode != NULL)
            {
                if (currNode->monitorIndex == i)
                {
                    strcpy(arguments[index], input_dir);
                    strcat(arguments[index], "/");
                    strcat(arguments[index], currNode->countryName);
                    index++;
                }
                currNode = currNode->nextNode;
            }

            arguments[index] = NULL;
            //Creating monitors
            execv(arguments[0], arguments);
            for (int count = 0; count < 11 + numOfCountriesPerMonitor[i]; count++)
            {
                free(arguments[count]);
            }
        }
    }
    struct sockaddr_in server_address[numMonitors];
    int server_socket[numMonitors];
    int connection_status[numMonitors];
    //Finding host address
    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);
    struct hostent *h;
    h = gethostbyname(hostname);
    struct in_addr addressin;
    memcpy(&addressin, *h->h_addr_list, sizeof(addressin));
    //Connecting to all monitor servers
    for (int i = 0; i < numMonitors; i++)
    {
        server_socket[i] = socket(AF_INET, SOCK_STREAM, 0);

        server_address[i].sin_family = AF_INET;
        server_address[i].sin_port = htons(BASEPORT + i);
        server_address[i].sin_addr.s_addr = addressin.s_addr;
        while ((connection_status[i] = connect(server_socket[i], (struct sockaddr *)&server_address[i], sizeof(server_address[i]))) < 0)
            ;
    }

    bloomFilterPtr bloomMonitorArray[numMonitors];
    int bloomlen = (sizeOfBloom / 8) + (sizeOfBloom % 8 > 0 ? 1 : 0);
    int n = bloomlen / socketBufferSize;
    int n_plus = bloomlen % socketBufferSize;

    for (monitor_index = 0; monitor_index < numMonitors; monitor_index++)
    {
        bloomMonitorArray[monitor_index] = malloc(sizeof(bloomFilter));
        bloomFilter_init(bloomMonitorArray[monitor_index], NUMOFHASHFUNCTIONS, sizeOfBloom);
        int j;
        for (j = 0; j < n; j++)
        {
            read(server_socket[monitor_index], bloomMonitorArray[monitor_index]->bloomBitArray + (j * socketBufferSize), socketBufferSize);
        }
        if (n_plus)
        {
            read(server_socket[monitor_index], bloomMonitorArray[monitor_index] + (j * socketBufferSize), bloomlen % socketBufferSize);
        }
    }

    closedir(inputDir);
    //This will hold the data from its line that is read
    char line[100];
    //This will hold the data from its line that is read separetly
    char data[10][30];
    //To be used on the separation of the line inyo its compoonents
    char *key;
    //------------------------------------Options------------------------------------//
    int numOfArguments;
    int command;
    CountryRefListNodePtr currNode = NULL;
    requestListPtr RList = malloc(sizeof(requestList));
    Rlist_init(RList);

    //I print the options available
    printf("========\n");
    printf("OPTIONS:\n");
    printf("=================================================================\n");
    printf("/travelRequest citizenID date countryFrom countryTo virusName\n");
    printf("/travelStats virusName date1 date2 [country]\n");
    printf("/addVaccinationRecords country\n");
    printf("/searchVaccinationStatus citizenID\n");
    printf("/exit\n");
    printf("=================================================================\n");

    do
    {
        //I read the users input

        fgets(line, sizeof(line), stdin);

        line[strcspn(line, "\n")] = 0;

        key = strtok(line, " ");

        for (numOfArguments = 0; key != NULL; numOfArguments++)
        {

            strcpy(data[numOfArguments], key);
            key = strtok(NULL, " ");
        }
        if (!strcmp(data[0], "/travelRequest"))
        {
            command = 1;
            if (numOfArguments == 6)
            {
                //Checking if citizen id is within the limits
                if (atoi(data[1]) > 0 && atoi(data[1]) < 10000)
                {
                    //Checking  if date is valid
                    char dateStr[12];
                    strcpy(dateStr, data[2]);
                    DatePtr date = date_init(data[2]);
                    if (date->day > 0 && date->day <= 30 && date->month > 0 && date->month <= 12 && date->year > 1900 && date->year < 3000)
                    {
                        currNode = CList->FirstNode;
                        monitor_index = -1;
                        while (currNode != NULL)
                        {
                            if (!strcmp(data[3], currNode->countryName))
                            {
                                monitor_index = currNode->monitorIndex;
                                break;
                            }
                            currNode = currNode->nextNode;
                        }
                        if (monitor_index != -1)
                        {
                            char *countryName = malloc(sizeof(char) * strlen(data[3]));
                            strcpy(countryName, data[3]);
                            char *virusName = malloc(sizeof(char) * strlen(data[5]));
                            strcpy(virusName, data[5]);
                            if (bloom_checkElement(bloomMonitorArray[monitor_index], data[1]) == 0)
                            {
                                printf("REQUEST REJECTED – YOU ARE NOT VACCINATED\n");
                                char datestr[12];
                                strcpy(datestr, dateStr);
                                DatePtr rejDate = date_init(datestr);
                                Rlist_insertItem(RList, 0, rejDate, countryName, virusName);
                            }
                            else
                            {
                                writeint(server_socket[monitor_index], command, socketBufferSize);

                                //Sending citizen id
                                writestr(server_socket[monitor_index], data[1], socketBufferSize);

                                //Sending dates
                                writestr(server_socket[monitor_index], dateStr, socketBufferSize);

                                //Sending virusName
                                writestr(server_socket[monitor_index], data[5], socketBufferSize);

                                //Reading message
                                char *message = readstr(server_socket[monitor_index], socketBufferSize);

                                char *dateOfVaccination;
                                if (!strcmp(message, "YES"))
                                {
                                    dateOfVaccination = readstr(server_socket[monitor_index], socketBufferSize);
                                }
                                if (!strcmp(message, "YES"))
                                {
                                    DatePtr dateVac = date_init(dateOfVaccination);

                                    if (check_dates(dateVac, date))
                                    {
                                        printf("REQUEST ACCEPTED – HAPPY TRAVELS\n");
                                        char datestr[12];
                                        strcpy(datestr, dateStr);
                                        DatePtr accDate = date_init(datestr);
                                        Rlist_insertItem(RList, 0, accDate, countryName, virusName);
                                    }
                                    else
                                    {
                                        printf("REQUEST REJECTED – YOU WILL NEED ANOTHER VACCINATIONBEFORE TRAVEL DATE\n");
                                        char datestr[12];
                                        strcpy(datestr, dateStr);
                                        DatePtr rejDate = date_init(datestr);
                                        Rlist_insertItem(RList, 0, rejDate, countryName, virusName);
                                    }
                                }
                                else
                                {
                                    printf("REQUEST REJECTED – YOU ARE NOT VACCINATED\n");
                                    char datestr[12];
                                    strcpy(datestr, dateStr);
                                    DatePtr rejDate = date_init(datestr);
                                    Rlist_insertItem(RList, 0, rejDate, countryName, virusName);
                                }
                                if (!strcmp(message, "YES"))
                                {
                                    free(dateOfVaccination);
                                }
                                free(message);
                            }
                        }
                        else
                        {
                            printf("ERROR! Country does not exist!\n");
                        }
                    }
                    else
                        printf("ERROR! Date is invalid please enter a valid date!\n");
                    dateDestructor(date);
                }
                else
                    printf("ERROR! Citizen id is out of limits\n");
            }
            else
                printf("ERROR! Wrong number of arguments!\n");
        }
        else if (!strcmp(data[0], "/travelStats"))
        {
            command = 2;
            //Checking if the dates are valid
            DatePtr date1 = date_init(data[2]);
            DatePtr date2 = date_init(data[3]);
            if (date1->day > 0 && date1->day <= 30 && date1->month > 0 && date1->month <= 12 && date1->year > 1900 && date1->year < 3000)
                if (date2->day > 0 && date2->day <= 30 && date2->month > 0 && date2->month <= 12 && date2->year > 1900 && date2->year < 3000)
                {
                    if (numOfArguments == 4)
                    {
                        int accReq = 0, rejReq = 0;
                        requestNodePtr currnode = RList->Begining;
                        while (currnode != NULL)
                        {
                            if (datecmp(currnode->date, date1) > 0 && datecmp(currnode->date, date2) < 0)
                            {
                                if (currnode->typeOfRequest)
                                {
                                    accReq++;
                                }
                                else
                                {
                                    rejReq++;
                                }
                            }
                            currnode = currnode->nextNode;
                        }
                        printf("TOTAL REQUESTS %d\n", accReq + rejReq);
                        printf("ACCEPTED %d\n", accReq);
                        printf("REJECTED %d\n", rejReq);
                    }
                    else if (numOfArguments == 5)
                    {
                        int accReq = 0, rejReq = 0;
                        requestNodePtr currnode = RList->Begining;
                        while (currnode != NULL)
                        {
                            if (!strcmp(currnode->countryName, data[4]) && !strcmp(currnode->virusName, data[1]))
                            {
                                if (datecmp(currnode->date, date1) > 0 && datecmp(currnode->date, date2) < 0)
                                {
                                    if (currnode->typeOfRequest)
                                    {
                                        accReq++;
                                    }
                                    else
                                    {
                                        rejReq++;
                                    }
                                }
                            }
                            currnode = currnode->nextNode;
                        }
                        printf("TOTAL REQUESTS %d\n", accReq + rejReq);
                        printf("ACCEPTED %d\n", accReq);
                        printf("REJECTED %d\n", rejReq);
                    }
                    else
                        printf("ERROR! Wrong number of arguments!\n");
                }
                else
                    printf("ERROR! First date is invalid please enter a valid date!\n");
            else
                printf("ERROR! Second date is invalid please enter a valid date!\n");
            dateDestructor(date1);
            dateDestructor(date2);
        }
        else if (!strcmp(data[0], "/addVaccinationRecords"))
        {
            command = 3;
            if (numOfArguments == 2)
            {
                currNode = CList->FirstNode;
                monitor_index = -1;
                while (currNode != NULL)
                {
                    if (!strcmp(currNode->countryName, data[1]))
                    {
                        monitor_index = currNode->monitorIndex;
                        break;
                    }
                    currNode = currNode->nextNode;
                }
                if (monitor_index >= 0)
                {
                    //Sending command
                    writeint(server_socket[monitor_index], command, socketBufferSize);
                    //Receiving flag
                    int newFilesFlag = readint(server_socket[monitor_index], socketBufferSize);
                    //If flag is 1 the travel monitor client knows to read the new bloom filter from monitor server
                    if (newFilesFlag)
                    {
                        int j;
                        for (j = 0; j < n; j++)
                        {
                            read(server_socket[monitor_index], bloomMonitorArray[monitor_index]->bloomBitArray + (j * socketBufferSize), socketBufferSize);
                        }
                        if (n_plus)
                        {
                            read(server_socket[monitor_index], bloomMonitorArray[monitor_index] + (j * socketBufferSize), bloomlen % socketBufferSize);
                        }
                    }
                }
                else
                    printf("There is no country with name %s\n", data[1]);
            }
            else
                printf("Wrong number of arguments!\n");
        }
        else if (!strcmp(data[0], "/searchVaccinationStatus"))
        {
            command = 4;
            int ctznid = atoi(data[1]);
            //Checking if citizen id is within the limits
            if (atoi(data[1]) > 0 && atoi(data[1]) < 10000)
            {
                int message = 0;
                for (monitor_index = 0; monitor_index < numMonitors; monitor_index++)
                {
                    writeint(server_socket[monitor_index], command, socketBufferSize);
                    //Sending citizen id to all monitors
                    writestr(server_socket[monitor_index], data[1], socketBufferSize);

                    //Receiving message if the citizen is in that monitor
                    message = readint(server_socket[monitor_index], socketBufferSize);
                    if (message)
                        break;
                }
                if (message)
                {

                    //Reading name
                    char *name = readstr(server_socket[monitor_index], socketBufferSize);

                    //Reading surname
                    char *surname = readstr(server_socket[monitor_index], socketBufferSize);

                    //Reading age
                    int age = readint(server_socket[monitor_index], socketBufferSize);

                    //Reading country
                    char *country = readstr(server_socket[monitor_index], socketBufferSize);

                    printf("%s %s %s %s\n", data[1], name, surname, country);
                    printf("AGE %d\n", age);
                    //Reading number of viruses
                    int numOfViruses = readint(server_socket[monitor_index], socketBufferSize);
                    for (int i = 0; i < numOfViruses; i++)
                    {
                        char *virusName = readstr(server_socket[monitor_index], socketBufferSize);
                        printf("VirusName %d\n", n);
                        char *yesno = readstr(server_socket[monitor_index], socketBufferSize);
                        if (!strcmp(yesno, "YES"))
                        {
                            char *date = readstr(server_socket[monitor_index], socketBufferSize);
                            printf("%s ", virusName);
                            printf("VACCINATED ON %s\n", date);
                        }
                        else
                        {
                            printf("%s ", virusName);
                            printf("NOT YET VACCINATED\n");
                        }
                        free(virusName);
                        free(yesno);
                    }
                    free(name);
                    free(surname);
                    free(country);
                }
                else
                    printf("This citizen id does not exist: %d\n", ctznid);
            }
            else
                printf("ERROR! Citizen id is out of limits\n");
        }
        else if (strcmp(data[0], "/exit"))
            printf("There is no command: %s\n", data[0]);

    } while (strcmp(data[0], "/exit"));

    command = 0;
    //Destruction of data structures and freeing of memory
    for (monitor_index = 0; monitor_index < numMonitors; monitor_index++)
    {
        writeint(server_socket[monitor_index], command, socketBufferSize);
        bloom_destructor(bloomMonitorArray[monitor_index]);
        close(server_socket[monitor_index]);
    }
    //Logfile creation
    char logfilename[25];
    sprintf(logfilename, "log_files/log_file.%d", getpid());
    printf("logfile: %s\n", logfilename);
    FILE *log_file = fopen(logfilename, "w");
    currNode = CList->FirstNode;
    while (currNode != NULL)
    {
        fprintf(log_file, "%s\n", currNode->countryName);
        currNode = currNode->nextNode;
    }
    int accReq = 0, rejReq = 0;
    requestNodePtr currnode = RList->Begining;
    while (currnode != NULL)
    {
        if (currnode->typeOfRequest)
        {
            accReq++;
        }
        else
        {
            rejReq++;
        }
        currnode = currnode->nextNode;
    }
    fprintf(log_file, "TOTAL TRAVEL REQUESTS %d\nACCEPTED %d\nREJECTED %d\n", accReq + rejReq, accReq, rejReq);
    fclose(log_file);

    //Closing data bases
    CountryRefList_Delete(CList);
    // Rlist_deleteList(RList);
    free(input_dir);
    free(bloomSize);
    waitpid(-1, NULL, 0);
    return 0;
}