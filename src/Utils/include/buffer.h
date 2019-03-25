#ifndef BUFFER_ADT_H
#define BUFFER_ADT_H

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct bufferCDT * bufferADT;

bufferADT createBuffer(const size_t size);

bufferADT createBackUpBuffer(bufferADT buffer);

void reset(bufferADT buffer);

bool canWrite(bufferADT buffer);

bool canProcess(bufferADT buffer);

bool canRead(bufferADT buffer);

uint8_t * getWritePtr(bufferADT buffer, size_t * availableSize);

uint8_t * getProcessPtr(bufferADT buffer, size_t * availableSizeToProcess);

uint8_t * getReadPtr(bufferADT buffer, size_t * availableSizeToRead);

void updateWritePtr(bufferADT buffer, ssize_t writedBytes);

void updateProcessPtr(bufferADT buffer, ssize_t processedBytes);

void updateReadPtr(bufferADT buffer, ssize_t readBytes);

void updateWriteAndProcessPtr(bufferADT buffer, ssize_t writedBytes);

void compact(bufferADT buffer);

uint8_t readAByte(bufferADT buffer);

uint8_t processAByte(bufferADT buffer);

void writeAByte(bufferADT buffer, uint8_t byte);

void writeAndProcessAByte(bufferADT buffer, uint8_t byte);

void deleteBuffer(bufferADT buffer);


#endif

