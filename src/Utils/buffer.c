#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "buffer.h"
#include "errorslib.h"

typedef struct bufferCDT {
    uint8_t * dataPtr;
    uint8_t * limitPtr;
    uint8_t * readPtr;
    uint8_t * processedPtr;
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

bufferADT createBackUpBuffer(bufferADT buffer)
{
	size_t size, count;
	uint8_t * readPtr;
	
	size = buffer->limitPtr - buffer->dataPtr;
	bufferADT bufferCopy = createBuffer(size);

	readPtr = getReadPtr(buffer, &count);
	memcpy(bufferCopy->writePtr, readPtr, count);
	bufferCopy->processedPtr = buffer->processedPtr;
	bufferCopy->writePtr = buffer->writePtr;
	return bufferCopy;
}

inline void reset(bufferADT buffer)
{
	if(buffer == NULL)
		return;
	buffer->readPtr      = buffer->dataPtr;
	buffer->processedPtr = buffer->dataPtr;
	buffer->writePtr     = buffer->dataPtr;
}

inline bool canWrite(bufferADT buffer)
{
	if(buffer == NULL)
		return false;
	return buffer->limitPtr - buffer->writePtr > 0;
}

inline bool canProcess(bufferADT buffer)
{
	if(buffer == NULL)
		return false;
	return buffer->writePtr - buffer->processedPtr > 0;
}

inline bool canRead(bufferADT buffer)
{
	if(buffer == NULL)
		return false;
	return buffer->processedPtr - buffer->readPtr > 0;
}

inline uint8_t * getWritePtr(bufferADT buffer, size_t * availableSize)
{
	if(buffer == NULL || buffer->writePtr > buffer-> limitPtr)
		fail("Invalid arguments for getWritePtr()");
	
	*availableSize = buffer->limitPtr - buffer->writePtr;
	return buffer->writePtr;
}

inline uint8_t * getProcessPtr(bufferADT buffer, size_t * availableSizeToProcess)
{
	if(buffer == NULL || buffer->processedPtr > buffer->writePtr)
		fail("Invalid arguments for getProcessPtr()");

	*availableSizeToProcess = buffer->writePtr - buffer->processedPtr;
	return buffer->processedPtr;
}

inline uint8_t * getReadPtr(bufferADT buffer, size_t * availableSizeToRead)
{
	if(buffer == NULL || buffer->readPtr > buffer->processedPtr)
		fail("Invalid arguments for getReadPtr()");

	*availableSizeToRead = buffer->processedPtr - buffer->readPtr;
	return buffer->readPtr;
}

inline void updateWritePtr(bufferADT buffer, ssize_t writedBytes)
{
	if(buffer == NULL || writedBytes < 0)
		return;
	if(buffer->writePtr + writedBytes > buffer->limitPtr)
		fail("Invalid arguments for updateWritePtr()");

	buffer->writePtr += (size_t)writedBytes;
}

inline void updateProcessPtr(bufferADT buffer, ssize_t processedBytes)
{
	if(buffer == NULL || processedBytes < 0)
		return;
	if(buffer->processedPtr + processedBytes > buffer->writePtr)
		fail("Invalid arguments for updateProcessPtr()");
		
	buffer->processedPtr += (size_t)processedBytes;
}

inline void updateReadPtr(bufferADT buffer, ssize_t readBytes)
{
	if(buffer == NULL || readBytes < 0)
		return;
	if(buffer->readPtr + readBytes > buffer->processedPtr)
		fail("Invalid arguments for updateReadPtr()");
		
	buffer->readPtr += (size_t)readBytes;
	if(buffer->readPtr == buffer->processedPtr)
		compact(buffer);
}

inline void updateWriteAndProcessPtr(bufferADT buffer, ssize_t writedBytes)
{
	if(buffer == NULL || writedBytes < 0)
		return;
	if(buffer->writePtr + writedBytes > buffer->limitPtr)
		fail("Invalid arguments for updateWritePtr()");

	buffer->writePtr     += (size_t)writedBytes;	
	buffer->processedPtr += (size_t)writedBytes;
}

void compact(bufferADT buffer)
{
	if(buffer == NULL || buffer->dataPtr == buffer->readPtr)
		return;
	if(buffer->readPtr == buffer->writePtr)
		reset(buffer);
	else {
		const size_t size    = buffer->writePtr - buffer->readPtr;
		const size_t psize   = buffer->processedPtr - buffer->readPtr;
        memmove(buffer->dataPtr, buffer->readPtr, size);
        buffer->readPtr      = buffer->dataPtr;
		buffer->processedPtr = buffer->dataPtr + psize;
        buffer->writePtr     = buffer->dataPtr + size;
	}
}

inline uint8_t readAByte(bufferADT buffer)
{
    uint8_t byte;
    if(canRead(buffer)) 
    {
        byte = *buffer->readPtr;
        updateReadPtr(buffer, 1);
    } else {
        byte = 0;
    }
    return byte;
} 

inline uint8_t processAByte(bufferADT buffer)
{
    uint8_t byte;
    if(canProcess(buffer)) 
    {
        byte = *buffer->processedPtr;
        updateProcessPtr(buffer, 1);
    } else {
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

inline void writeAndProcessAByte(bufferADT buffer, uint8_t byte)
{
    if(canWrite(buffer)) 
    {
        *buffer->writePtr = byte;
        updateWritePtr(buffer, 1);
        updateProcessPtr(buffer, 1);
    }
}

void deleteBuffer(bufferADT buffer)
{
	if(buffer == NULL)
		return;
	free(buffer->dataPtr);
	free(buffer);
}

