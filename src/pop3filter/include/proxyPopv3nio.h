#ifndef PROXY_POPV3_NIO_H
#define PROXY_POPV3_NIO_H

#include "multiplexor.h"

#define VERSION_NUMBER "1.0"
#define TIMEOUT 120.0
#define BUFFER_SIZE 4000


typedef struct conf {
    bool                 filterActivated;
    char *               stdErrorFilePath;
    char *               replaceMsg;
    char *               filterCommand;
    const char *         stringServer;
    char *               mediaRange;
    char *               listenPop3Address;
    char *               listenAdminAddress;
    size_t               replaceMsgSize; 
    size_t               bufferSize;
    size_t               messageCount;
    bool                 mediaRangeAdminChanged;
    bool                 filterCommanAdminChanged;
    bool                 replaceMsgAdminChanged;
    bool                 stdErrorFilePathAdminChanged;
    char *               credential;
    int *                etags;
} conf;


typedef struct metrics {
    /** Para saber los bytes copiados hacia el cliente, origin y filter */
    unsigned long long   totalBytesToOrigin;
    unsigned long long   totalBytesToClient;
    unsigned long long   totalBytesToFilter;

    /** Para el total de bytes escritos en los buffers */
    unsigned long long   bytesReadBuffer;
    unsigned long long   bytesWriteBuffer;
    unsigned long long   bytesFilterBuffer;

    /** Para poder sacar el average de uso de los buffers */
    unsigned long long   writesQtyReadBuffer;    
    unsigned long long   writesQtyWriteBuffer;
    unsigned long long   writesQtyFilterBuffer;

    unsigned long long   readsQtyReadBuffer;    
    unsigned long long   readsQtyWriteBuffer;
    unsigned long long   readsQtyFilterBuffer;

    unsigned long long   totalConnections;
    unsigned long long   activeConnections;

    unsigned long long   commandsFilteredQty;
} metrics;


typedef enum etagIndex {
    filterEtag              = 0,
    stdErrorFilePathEtag    = 1,
    replaceMsgEtag          = 2,
    transformCommandEtag    = 3,
    stringServerEtag        = 4,
    mediaRangeEtag          = 5,
    listenPop3AddressEtag   = 6,
    listenAdminAddressEtag  = 7,
    replaceMsgSizeEtag      = 8,
    credentialEtag          = 9,
    bufferSizeEtag          = 10,
} etagIndex;


conf proxyConf;
metrics proxyMetrics;

void poolProxyPopv3Destroy(void);
void proxyPopv3PassiveAccept(MultiplexorKey key);

#endif

