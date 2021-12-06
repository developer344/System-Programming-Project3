#ifndef _CYCLIC_BUFFER_H_
#define _CYCLIC_BUFFER_H_

//Defining struct cyclicBuffer node and pointer cyclicBuffer node
typedef struct cycbufnode cyclicBufferNode;
typedef struct cycbufnode *cyclicBufferNodePtr;

typedef struct cycbufnode
{
    char *path;
    cyclicBufferNodePtr nextNode;
    cyclicBufferNodePtr prevNode;
} cyclicBufferNode;

//Defining struct cyclicBuffer and pointer cyclicBuffer
typedef struct cycBuffer cyclicBuffer;
typedef struct cycBuffer *cyclicBufferPtr;

typedef struct cycBuffer
{
    cyclicBufferNodePtr Begining;
    cyclicBufferNodePtr End;
    int numOfElements;
    int maxElements;
} cyclicBuffer;

void cycBuffer_init(cyclicBufferPtr cyclicBuffer, int maxItems);
int cycBuffer_pushItem(cyclicBufferPtr cyclicBuffer, char *pathToBePushed);
char *cycBuffer_popItem(cyclicBufferPtr cyclicBuffer);
int cycBuffer_checkFull(cyclicBufferPtr cyclicBuffer);
int cycBuffer_checkEmpty(cyclicBufferPtr cyclicBuffer);

void cycBuffer_nodeDestructor(cyclicBufferNodePtr cyclicBufferNode);
void cycBuffer_Destructor(cyclicBufferPtr cyclicBuffer);
#endif
