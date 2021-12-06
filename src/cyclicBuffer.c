#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cyclicBuffer.h"

void cycBuffer_init(cyclicBufferPtr cyclicBuff, int maxItems)
{
    cyclicBuff->Begining = NULL;
    cyclicBuff->End = NULL;
    cyclicBuff->numOfElements = 0;
    cyclicBuff->maxElements = maxItems;
}

int cycBuffer_pushItem(cyclicBufferPtr cyclicBuffer, char *pathToBePushed)
{
    if (!cycBuffer_checkFull(cyclicBuffer))
        if (cycBuffer_checkEmpty(cyclicBuffer))
        {
            cyclicBuffer->Begining = malloc(sizeof(cyclicBuffer));
            cyclicBuffer->End = cyclicBuffer->Begining;
            cyclicBuffer->End->path = malloc(sizeof(char) * strlen(pathToBePushed));
            strcpy(cyclicBuffer->End->path, pathToBePushed);
            cyclicBuffer->End->nextNode = NULL;
            cyclicBuffer->End->prevNode = NULL;
            cyclicBuffer->numOfElements++;
            return 0;
        }
        else
        {
            cyclicBufferNodePtr currNode = malloc(sizeof(cyclicBuffer));
            cyclicBuffer->Begining->prevNode = currNode;
            currNode->nextNode = cyclicBuffer->Begining;
            currNode->prevNode = NULL;
            currNode->path = malloc(sizeof(char) * strlen(pathToBePushed));
            strcpy(currNode->path, pathToBePushed);
            cyclicBuffer->Begining = currNode;
            cyclicBuffer->numOfElements++;
            return 0;
        }
    return -1;
}

char *cycBuffer_popItem(cyclicBufferPtr cyclicBuffer)
{
    if (cycBuffer_checkEmpty(cyclicBuffer))
        return NULL;
    cyclicBufferNodePtr currNode = cyclicBuffer->End;
    if (cyclicBuffer->numOfElements == 1)
    {
        cyclicBuffer->Begining, cyclicBuffer->End = NULL;
    }
    else
    {
        cyclicBuffer->End = currNode->prevNode;
        cyclicBuffer->End->nextNode = NULL;
    }
    char *path = malloc(sizeof(char) * strlen(currNode->path));
    strcpy(path, currNode->path);
    cycBuffer_nodeDestructor(currNode);
    cyclicBuffer->numOfElements--;
    return path;
}

int cycBuffer_checkFull(cyclicBufferPtr cyclicBuffer)
{
    return cyclicBuffer->maxElements == cyclicBuffer->numOfElements;
}

int cycBuffer_checkEmpty(cyclicBufferPtr cyclicBuffer)
{
    return cyclicBuffer->numOfElements == 0;
}

void cycBuffer_nodeDestructor(cyclicBufferNodePtr cyclicBufferNode)
{
    free(cyclicBufferNode->path);
    free(cyclicBufferNode);
}

void cycBuffer_Destructor(cyclicBufferPtr cyclicBuffer)
{
    cyclicBufferNodePtr nextNode, currNode = cyclicBuffer->Begining;
    while (currNode != NULL)
    {
        nextNode = currNode->nextNode;
        cycBuffer_nodeDestructor(currNode);
        currNode = nextNode;
    }
    free(cyclicBuffer);
}