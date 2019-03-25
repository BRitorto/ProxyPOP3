#include <stdlib.h>
#include <string.h>
#include "CuTest.h"
#include "buffer.h"

typedef struct bufferCDT {
    uint8_t * dataPtr;
    uint8_t * limitPtr;
    uint8_t * readPtr;
    uint8_t * processedPtr;
    uint8_t * writePtr;
} bufferCDT;

void testBufferMisc(CuTest* tc) {
    bufferADT buffer = createBuffer(6);
    CuAssertPtrNotNull(tc, buffer);
    CuAssertIntEquals(tc, true,  canWrite(buffer));
    CuAssertIntEquals(tc, false, canRead(buffer));

    size_t wbytes = 0, rbytes = 0;
    uint8_t *ptr = getWritePtr(buffer, &wbytes);
    CuAssertIntEquals(tc, 6, wbytes);
    // escribo 4 bytes
    uint8_t firstWrite [] = {
        'H', 'O', 'L', 'A',
    };
    memcpy(ptr, firstWrite, sizeof(firstWrite));
    updateWriteAndProcessPtr(buffer, sizeof(firstWrite));

    // quedan 2 libres para escribir
    getWritePtr(buffer, &wbytes);
    CuAssertIntEquals(tc, 2, wbytes);

    // tengo por leer
    getReadPtr(buffer, &rbytes);
    CuAssertIntEquals(tc, 4, rbytes);

    // leo 3 del buffer
    CuAssertIntEquals(tc, 'H', readAByte(buffer));
    CuAssertIntEquals(tc, 'O', readAByte(buffer));
    CuAssertIntEquals(tc, 'L', readAByte(buffer));

    // queda 1 por leer
    getReadPtr(buffer, &rbytes);
    CuAssertIntEquals(tc, 1, rbytes);

    // quiero escribir..tendria que seguir habiendo 2 libres
    ptr = getWritePtr(buffer, &wbytes);
    CuAssertIntEquals(tc, 2, wbytes);

    uint8_t secondWrite [] = {
        ' ', 'M',
    };
    memcpy(ptr, secondWrite, sizeof(secondWrite));
    updateWriteAndProcessPtr(buffer, sizeof(secondWrite));

    CuAssertIntEquals(tc, false, canWrite(buffer));
    getWritePtr(buffer, &wbytes);
    CuAssertIntEquals(tc, 0, wbytes);

    // tiene que haber 2 + 1 para leer
    ptr = getReadPtr(buffer, &rbytes);
    CuAssertIntEquals(tc, 3, rbytes);
    CuAssertTrue(tc, (ptr - buffer->dataPtr) != 0);

    compact(buffer);
    CuAssertPtrEquals(tc, buffer->dataPtr, getReadPtr(buffer, &rbytes));
    CuAssertIntEquals(tc, 3, rbytes);
    CuAssertPtrEquals(tc, buffer->dataPtr + 3, getWritePtr(buffer, &wbytes));
    CuAssertIntEquals(tc, 3, wbytes);

    uint8_t thirdWrite [] = {
        'U', 'N', 'D',
    };
    memcpy(ptr, thirdWrite, sizeof(thirdWrite));
    updateWriteAndProcessPtr(buffer, sizeof(thirdWrite));

    getWritePtr(buffer, &wbytes);
    CuAssertIntEquals(tc, 0, wbytes);
    CuAssertPtrEquals(tc, buffer->dataPtr, getReadPtr(buffer, &rbytes));
    updateReadPtr(buffer, rbytes);
    getReadPtr(buffer, &rbytes);
    CuAssertIntEquals(tc, 0, rbytes);
    CuAssertPtrEquals(tc, buffer->dataPtr, getWritePtr(buffer, &wbytes));
    CuAssertIntEquals(tc, 6, wbytes);

    compact(buffer);
    getReadPtr(buffer, &rbytes);
    CuAssertIntEquals(tc, 0, rbytes);
    getWritePtr(buffer, &wbytes);
    CuAssertIntEquals(tc, 6, wbytes);

}

void testBufferMiscWithProcess(CuTest* tc) {
    bufferADT buffer = createBuffer(6);
    CuAssertPtrNotNull(tc, buffer);
    CuAssertIntEquals(tc, true,  canWrite(buffer));
    CuAssertIntEquals(tc, false, canProcess(buffer));
    CuAssertIntEquals(tc, false, canRead(buffer));

    size_t wbytes = 0, pbytes = 0, rbytes = 0;
    uint8_t *ptr = getWritePtr(buffer, &wbytes);
    CuAssertIntEquals(tc, 6, wbytes);
    // escribo 4 bytes
    uint8_t firstWrite [] = {
        'H', 'O', 'L', 'A',
    };
    memcpy(ptr, firstWrite, sizeof(firstWrite));
    updateWritePtr(buffer, sizeof(firstWrite));

    // quedan 2 libres para escribir 
    getWritePtr(buffer, &wbytes);
    CuAssertIntEquals(tc, 2, wbytes);

    //tengo por procesar
    getProcessPtr(buffer, &pbytes);
    CuAssertIntEquals(tc, 4, pbytes);

    // tengo por leer
    getReadPtr(buffer, &rbytes);
    CuAssertIntEquals(tc, 0, rbytes);

    // proceso 3 del buffer
    CuAssertIntEquals(tc, 'H', processAByte(buffer));
    CuAssertIntEquals(tc, 'O', processAByte(buffer));
    CuAssertIntEquals(tc, 'L', processAByte(buffer));

    // queda 1 por procesar
    getProcessPtr(buffer, &pbytes);
    CuAssertIntEquals(tc, 1, pbytes);

    // leo 2 del buffer
    CuAssertIntEquals(tc, 'H', readAByte(buffer));
    CuAssertIntEquals(tc, 'O', readAByte(buffer));

    // queda 1 por leer
    getReadPtr(buffer, &rbytes);
    CuAssertIntEquals(tc, 1, rbytes);

    // quiero escribir..tendria que seguir habiendo 2 libres
    ptr = getWritePtr(buffer, &wbytes);
    CuAssertIntEquals(tc, 2, wbytes);

    uint8_t secondWrite [] = {
        ' ', 'M',
    };
    memcpy(ptr, secondWrite, sizeof(secondWrite));
    updateWritePtr(buffer, sizeof(secondWrite));

    CuAssertIntEquals(tc, false, canWrite(buffer));
    getWritePtr(buffer, &wbytes);
    CuAssertIntEquals(tc, 0, wbytes);

    // tiene que haber 2 + 1 para procesar
    ptr = getProcessPtr(buffer, &pbytes);
    CuAssertIntEquals(tc, 3, pbytes);
    CuAssertTrue(tc, (ptr - buffer->dataPtr) != 0);

    // tiene que haber 1 para leer
    ptr = getReadPtr(buffer, &rbytes);
    CuAssertIntEquals(tc, 1, rbytes);
    CuAssertTrue(tc, (ptr - buffer->dataPtr) != 0);

    compact(buffer);
    CuAssertPtrEquals(tc, buffer->dataPtr, getReadPtr(buffer, &rbytes));
    CuAssertIntEquals(tc, 1, rbytes);
    CuAssertPtrEquals(tc, buffer->dataPtr + 1, getProcessPtr(buffer, &pbytes));
    CuAssertIntEquals(tc, 3, pbytes);
    CuAssertPtrEquals(tc, buffer->dataPtr + 4, getWritePtr(buffer, &wbytes));
    CuAssertIntEquals(tc, 2, wbytes);

    uint8_t thirdWrite [] = {
        'U', 'N',
    };
    memcpy(ptr, thirdWrite, sizeof(thirdWrite));
    updateWritePtr(buffer, sizeof(thirdWrite));

    getWritePtr(buffer, &wbytes);
    CuAssertIntEquals(tc, 0, wbytes);    
    CuAssertPtrEquals(tc, buffer->dataPtr + 1, getProcessPtr(buffer, &pbytes));
    updateProcessPtr(buffer, pbytes);    
    getProcessPtr(buffer, &pbytes);
    CuAssertIntEquals(tc, 0, pbytes);
    CuAssertPtrEquals(tc, buffer->dataPtr, getReadPtr(buffer, &rbytes));
    updateReadPtr(buffer, rbytes);
    getReadPtr(buffer, &rbytes);
    CuAssertIntEquals(tc, 0, rbytes);    
    
    CuAssertPtrEquals(tc, buffer->dataPtr, getProcessPtr(buffer, &pbytes));
    CuAssertIntEquals(tc, 0, pbytes);
    CuAssertPtrEquals(tc, buffer->dataPtr, getWritePtr(buffer, &wbytes));
    CuAssertIntEquals(tc, 6, wbytes);

    compact(buffer);
    getReadPtr(buffer, &rbytes);
    CuAssertIntEquals(tc, 0, rbytes);
    getProcessPtr(buffer, &pbytes);
    CuAssertIntEquals(tc, 0, pbytes);
    getWritePtr(buffer, &wbytes);
    CuAssertIntEquals(tc, 6, wbytes);

}

CuSuite * getBufferTest(void) {
    CuSuite* suite = CuSuiteNew();
    
    SUITE_ADD_TEST(suite, testBufferMisc);
    SUITE_ADD_TEST(suite, testBufferMiscWithProcess);
    return suite;
}

