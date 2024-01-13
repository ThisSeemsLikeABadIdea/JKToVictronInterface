// JKBMSWrapper.cpp
#include "JKBMSWrapper.h"
#include "jkbmsInterface.h" // Include your original JKBMS class here

extern "C" {

struct JKBMSWrapper {
    JKBMS* jkInstance;
};

JKBMSWrapper* JKBMS_Create() {
    JKBMSWrapper* wrapper = new JKBMSWrapper();
    wrapper->jkInstance = new JKBMS(); // Instantiate your original JKBMS class here
    return wrapper;
}

void JKBMS_Destroy(JKBMSWrapper* jkInstance) {
    delete jkInstance->jkInstance;
    delete jkInstance;
}

void JKBMS_Start(JKBMSWrapper* jkInstance) {
    jkInstance->jkInstance->start();
}

void JKBMS_Request_JK_Battery_485_Status_Frame(JKBMSWrapper* jkInstance) {
    //jkInstance->jkInstance->Request_JK_Battery_485_Status_Frame();
}

// Implement more wrapper functions to access data from the JKBMS object...

} // extern "C"