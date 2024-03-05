#ifndef httpFlashServer_h
#define httpFlashServer_h

// #include <ESPmDNS.h>
#include "index_html.h"
#include "global.h"

void setNTPTime();
void handleRoot();
void handleDownloadFile();
void handleDelete();
void handleFileUpload();
void handleFormat();
void  handleReset();
void startServer();
void serverHandleClient();

#endif
