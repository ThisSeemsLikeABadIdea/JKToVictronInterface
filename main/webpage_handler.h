#ifndef WEBPAGE_HANDLER_H
#define WEBPAGE_HANDLER_H

// Include necessary libraries
#include "esp_http_server.h"

// Function declarations
void start_provisioning_web_server();
void handle_web_request(httpd_req_t *req);

#endif // WEBPAGE_HANDLER_H
