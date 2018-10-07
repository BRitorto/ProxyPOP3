#ifndef BUFFER_ADT_H
#define BUFFER_ADT_H

#include <stdbool.h>
#include <unistd.h>

typedef struct bufferCDT * bufferADT;

bufferADT createBuffer(const size_t size);

void reset(bufferADT buffer);

bool canWrite(bufferADT buffer);

bool canRead(bufferADT buffer);

uint8_t * getWritePtr(bufferADT buffer, size_t * availableSize);

uint8_t * getReadPtr(bufferADT buffer, size_t * availableSizeToRead);

void updateWritePtr(bufferADT buffer, ssize_t writedBytes);

void updateReadPtr(bufferADT buffer, ssize_t readBytes);

void compact(bufferADT buffer);

uint8_t readAByte(bufferADT buffer);

void writeAByte(bufferADT buffer, uint8_t byte);

void deleteBuffer(bufferADT buffer);

#endif
