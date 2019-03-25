#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "CuTest.h"

#include "bodyPop3ParserTest.h"

#include "bodyPop3Parser.h"
#include "buffer.h"


void testSkipBody(CuTest * tc) {
    bodyPop3Parser parser;
    bufferADT src;
    bufferADT dest;
    size_t size;
    bool errored = false;
    uint8_t * ptr; 

    char * testBody1        = "Hola.\r\n..Como va\r\nYo bien\r\n..\r\n...\r\n.\r\n";
    char * testBody1result  = "Hola.\r\n.Como va\r\nYo bien\r\n.\r\n..\r\n";
    char * testBody2        = "..\r\n...\r\n..Hola debe andar\r\n";    
    char * testBody2result  = ".\r\n..\r\n.Hola debe andar\r\n";
    char * testBody3        = "Return-Path: <>\r\nX-Original-To: franciscosanguineti@ubuntu\r\nDelivered-To: franciscosanguineti@ubuntu\r\nReceived: by ubuntu.localdomain (Postfix)\r\nid 6A879123A69; Sun, 16 Sep 2018 12:21:11 -0300 (-03)\r\nDate: Sun, 16 Sep 2018 12:21:11 -0300 (-03)\r\nFrom: MAILER-DAEMON@ubuntu.localdomain (Mail Delivery System)\r\nSubject: Undelivered Mail Returned to Sender\r\nTo: franciscosanguineti@ubuntu\r\nAuto-Submitted: auto-replied\r\nMIME-Version: 1.0\r\nContent-Type: multipart/report; report-type=delivery-status;\r\nboundary=62328123A66.1537111271/ubuntu.localdomain\r\nMessage-Id: <20180916152111.6A879123A69@ubuntu.localdomain>\r\n";
    char * testBody3result  = "Return-Path: <>\r\nX-Original-To: franciscosanguineti@ubuntu\r\nDelivered-To: franciscosanguineti@ubuntu\r\nReceived: by ubuntu.localdomain (Postfix)\r\nid 6A879123A69; Sun, 16 Sep 2018 12:21:11 -0300 (-03)\r\nDate: Sun, 16 Sep 2018 12:21:11 -0300 (-03)\r\nFrom: MAILER-DAEMON@ubuntu.localdomain (Mail Delivery System)\r\nSubject: Undelivered Mail Returned to Sender\r\nTo: franciscosanguineti@ubuntu\r\nAuto-Submitted: auto-replied\r\nMIME-Version: 1.0\r\nContent-Type: multipart/report; report-type=delivery-status;\r\nboundary=62328123A66.1537111271/ubuntu.localdomain\r\nMessage-Id: <20180916152111.6A879123A69@ubuntu.localdomain>\r\n";  
    
    
    //TestBody1
    bodyPop3ParserInit(&parser);
    src  = createBuffer(strlen(testBody1)); 
    dest = createBuffer(strlen(testBody1)); 
    ptr  = getWritePtr(src, &size);
    memcpy(ptr, testBody1, size);
    updateWritePtr(src, size);

    bodyPop3ParserConsume(&parser, src, dest, true, &errored);
    CuAssertIntEquals(tc, false, errored);    
    CuAssertIntEquals(tc, false, canProcess(src));
    CuAssertIntEquals(tc, true, canRead(dest));
    
    writeAByte(dest, 0);
    ptr  = getReadPtr(dest, &size);
    CuAssertStrEquals(tc, testBody1result, (char *) ptr);
    deleteBuffer(src);
    deleteBuffer(dest);

    //TestBody2
    bodyPop3ParserInit(&parser);
    src  = createBuffer(strlen(testBody2)); 
    dest = createBuffer(strlen(testBody2)); 
    ptr  = getWritePtr(src, &size);
    memcpy(ptr, testBody2, size);
    updateWritePtr(src, size);

    bodyPop3ParserConsume(&parser, src, dest, true, &errored);
    CuAssertIntEquals(tc, false, errored);    
    CuAssertIntEquals(tc, false, canProcess(src));
    CuAssertIntEquals(tc, true, canRead(dest));
    
    writeAByte(dest, 0);
    ptr  = getReadPtr(dest, &size);
    CuAssertStrEquals(tc, testBody2result, (char *) ptr);
    deleteBuffer(src);
    deleteBuffer(dest);


    //TestBody3
    bodyPop3ParserInit(&parser);
    src  = createBuffer(strlen(testBody3)); 
    dest = createBuffer(strlen(testBody3) + 1); 
    ptr  = getWritePtr(src, &size);
    memcpy(ptr, testBody3, size);
    updateWritePtr(src, size);

    bodyPop3ParserConsume(&parser, src, dest, true, &errored);
    CuAssertIntEquals(tc, false, errored);    
    CuAssertIntEquals(tc, false, canProcess(src));
    CuAssertIntEquals(tc, true, canRead(dest));
    
    writeAByte(dest, 0);
    ptr  = getReadPtr(dest, &size);
    CuAssertStrEquals(tc, testBody3result, (char *) ptr);
    deleteBuffer(src);
    deleteBuffer(dest);
}

void testAddBody(CuTest * tc) {
    bodyPop3Parser parser;
    bufferADT src;
    bufferADT dest;
    size_t size;
    bool errored = false;
    uint8_t * ptr; 

    char * testBody1        = "Hola.\r\n.Como va\r\nYo bien\r\n.\r\n..\r\n";
    char * testBody1result  = "Hola.\r\n..Como va\r\nYo bien\r\n..\r\n...\r\n.\r\n";  
    char * testBody2        = ".\r\n..\r\n.Hola debe andar\r\n";
    char * testBody2result  = "..\r\n...\r\n..Hola debe andar\r\n.\r\n";  
    char * testBody3        = "Return-Path: <>\r\nX-Original-To: franciscosanguineti@ubuntu\r\nDelivered-To: franciscosanguineti@ubuntu\r\nReceived: by ubuntu.localdomain (Postfix)\r\nid 6A879123A69; Sun, 16 Sep 2018 12:21:11 -0300 (-03)\r\nDate: Sun, 16 Sep 2018 12:21:11 -0300 (-03)\r\nFrom: MAILER-DAEMON@ubuntu.localdomain (Mail Delivery System)\r\nSubject: Undelivered Mail Returned to Sender\r\nTo: franciscosanguineti@ubuntu\r\nAuto-Submitted: auto-replied\r\nMIME-Version: 1.0\r\nContent-Type: multipart/report; report-type=delivery-status;\r\nboundary=62328123A66.1537111271/ubuntu.localdomain\r\nMessage-Id: <20180916152111.6A879123A69@ubuntu.localdomain>\r\n";
    char * testBody3result  = "Return-Path: <>\r\nX-Original-To: franciscosanguineti@ubuntu\r\nDelivered-To: franciscosanguineti@ubuntu\r\nReceived: by ubuntu.localdomain (Postfix)\r\nid 6A879123A69; Sun, 16 Sep 2018 12:21:11 -0300 (-03)\r\nDate: Sun, 16 Sep 2018 12:21:11 -0300 (-03)\r\nFrom: MAILER-DAEMON@ubuntu.localdomain (Mail Delivery System)\r\nSubject: Undelivered Mail Returned to Sender\r\nTo: franciscosanguineti@ubuntu\r\nAuto-Submitted: auto-replied\r\nMIME-Version: 1.0\r\nContent-Type: multipart/report; report-type=delivery-status;\r\nboundary=62328123A66.1537111271/ubuntu.localdomain\r\nMessage-Id: <20180916152111.6A879123A69@ubuntu.localdomain>\r\n.\r\n";  
    
    //TestBody1
    bodyPop3ParserInit(&parser);
    src  = createBuffer(strlen(testBody1)); 
    dest = createBuffer(strlen(testBody1result) +1); 
    ptr  = getWritePtr(src, &size);
    memcpy(ptr, testBody1, size);
    updateWritePtr(src, size);

    bodyPop3ParserConsume(&parser, src, dest, false, &errored);
    CuAssertIntEquals(tc, false, errored);    
    CuAssertIntEquals(tc, false, canProcess(src));
    CuAssertIntEquals(tc, true, canRead(dest));
    
    writeAByte(dest, '.');
    writeAByte(dest, '\r');
    writeAByte(dest, '\n');
    writeAByte(dest, 0);
    ptr  = getReadPtr(dest, &size);
    CuAssertStrEquals(tc, testBody1result, (char *) ptr);
    deleteBuffer(src);
    deleteBuffer(dest);

    //TestBody2
    bodyPop3ParserInit(&parser);
    src  = createBuffer(strlen(testBody2)); 
    dest = createBuffer(strlen(testBody2result) +1); 
    ptr  = getWritePtr(src, &size);
    memcpy(ptr, testBody2, size);
    updateWritePtr(src, size);

    bodyPop3ParserConsume(&parser, src, dest, false, &errored);
    CuAssertIntEquals(tc, false, errored);    
    CuAssertIntEquals(tc, false, canProcess(src));
    CuAssertIntEquals(tc, true, canRead(dest));
    
    writeAByte(dest, '.');
    writeAByte(dest, '\r');
    writeAByte(dest, '\n');
    writeAByte(dest, 0);
    ptr  = getReadPtr(dest, &size);
    CuAssertStrEquals(tc, testBody2result, (char *) ptr);
    deleteBuffer(src);
    deleteBuffer(dest);

    //TestBody3
    bodyPop3ParserInit(&parser);
    src  = createBuffer(strlen(testBody3)); 
    dest = createBuffer(strlen(testBody3result) +1); 
    ptr  = getWritePtr(src, &size);
    memcpy(ptr, testBody3, size);
    updateWritePtr(src, size);

    bodyPop3ParserConsume(&parser, src, dest, false, &errored);
    CuAssertIntEquals(tc, false, errored);    
    CuAssertIntEquals(tc, false, canProcess(src));
    CuAssertIntEquals(tc, true, canRead(dest));
    
    writeAByte(dest, '.');
    writeAByte(dest, '\r');
    writeAByte(dest, '\n');
    writeAByte(dest, 0);
    ptr  = getReadPtr(dest, &size);
    CuAssertStrEquals(tc, testBody3result, (char *) ptr);
    deleteBuffer(src);
    deleteBuffer(dest);
}

CuSuite * getBodyPop3ParserTest(void) {
    CuSuite* suite = CuSuiteNew();
    
    SUITE_ADD_TEST(suite, testSkipBody);
    SUITE_ADD_TEST(suite, testAddBody);

    return suite;
}

