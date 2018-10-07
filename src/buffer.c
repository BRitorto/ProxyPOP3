#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "buffer.h"

typedef struct bufferCDT {
    uint8_t * dataPtr;
    uint8_t * limitPtr;
    uint8_t * readPtr;
    uint8_t * writePtr;
} bufferCDT;

bufferADT createBuffer(const size_t size)
{
	bufferADT buffer = malloc(sizeof(bufferCDT));
	buffer->dataPtr = malloc(size * sizeof(uint8_t));
	buffer->limitPtr = buffer->dataPtr + size;
	reset(buffer);
	return buffer;
}

inline void reset(bufferADT buffer)
{
	if(buffer == NULL)
		return;
	buffer->readPtr = buffer->dataPtr;
	buffer->writePtr = buffer->dataPtr;
}

inline bool canWrite(bufferADT buffer)
{
	if(buffer == NULL)
		return false;
	return buffer->limitPtr - buffer->writePtr > 0;
}

inline bool canRead(bufferADT buffer)
{
	if(buffer == NULL)
		return false;
	return buffer->limitPtr - buffer->readPtr > 0;
}

inline uint8_t * getWritePtr(bufferADT buffer, size_t * availableSize)
{
	if(buffer == NULL || buffer->writePtr > buffer-> limitPtr)
	{
		fprintf(stderr, "Invalid arguments for getWritePtr()");
		exit(1);
	}
	*availableSize = buffer->limitPtr - buffer->writePtr;
	return buffer->writePtr;
}

inline uint8_t * getReadPtr(bufferADT buffer, size_t * availableSizeToRead)
{
	if(buffer == NULL || buffer->readPtr > buffer-> writePtr)
	{
		fprintf(stderr, "Invalid arguments for getReadPtr()");
		exit(1);
	}
	*availableSizeToRead = buffer->writePtr - buffer->readPtr;
	return buffer->readPtr;
}

inline void updateWritePtr(bufferADT buffer, ssize_t writedBytes)
{
	if(buffer == NULL || writedBytes < 0)
		return;
	if(buffer->writePtr + writedBytes > buffer->limitPtr)
	{
		fprintf(stderr, "Invalid arguments for updateWritePtr()");
		exit(1);
	}
	buffer->writePtr += (size_t)writedBytes;
}


inline void updateReadPtr(bufferADT buffer, ssize_t readBytes)
{
	if(buffer == NULL || readBytes < 0)
		return;
	if(buffer->readPtr + readBytes > buffer->writePtr)
	{
		fprintf(stderr, "Invalid arguments for updateReadPtr()");
		exit(1);
	}
	buffer->readPtr += (size_t)readBytes;
	if(buffer->readPtr == buffer->writePtr)
		compact(buffer);
}

void compact(bufferADT buffer)
{
	if(buffer == NULL || buffer->dataPtr == buffer->readPtr)
		return;
	if(buffer->readPtr == buffer->writePtr)
		reset(buffer);
	else {
		const size_t size = buffer->writePtr - buffer->readPtr;
        memmove(buffer->dataPtr, buffer->readPtr, size);
        buffer->readPtr  = buffer->dataPtr;
        buffer->writePtr = buffer->dataPtr + size;
	}
}


inline uint8_t readAByte(bufferADT buffer)
{
    uint8_t byte;
    if(canRead(buffer)) 
    {
        byte = *buffer->readPtr;
        updateReadPtr(buffer, 1);
    } 
    else {
        byte = 0;
    }
    return byte;
} 

inline void writeAByte(bufferADT buffer, uint8_t byte)
{
    if(canWrite(buffer)) 
    {
        *buffer->writePtr = byte;
        updateWritePtr(buffer, 1);
    }
}

void deleteBuffer(bufferADT buffer)
{
	if(buffer == NULL)
		return;
	free(buffer->dataPtr);
	free(buffer);
}

