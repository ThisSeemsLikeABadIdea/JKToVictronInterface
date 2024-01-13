// JKBMSWrapper.h
#ifndef JKBMSWRAPPER_H
#define JKBMSWRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

// Define a structure representing the JKBMS object
typedef struct JKBMSWrapper JKBMSWrapper;

// Function declarations to create, use, and destroy the JKBMS object
JKBMSWrapper* JKBMS_Create();
void JKBMS_Destroy(JKBMSWrapper* jkInstance);
void JKBMS_Start(JKBMSWrapper* jkInstance);
void JKBMS_Request_JK_Battery_485_Status_Frame(JKBMSWrapper* jkInstance);
// Add more functions to access necessary data from the JKBMS object...

#ifdef __cplusplus
}
#endif

#endif /* JKBMSWRAPPER_H */