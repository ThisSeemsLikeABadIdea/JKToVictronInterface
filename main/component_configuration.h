// component_config.h
#ifndef COMPONENT_CONFIG_H
#define COMPONENT_CONFIG_H
#include "esp_http_server.h"

void configure_mqtt();
void configure_influxdb();
//static esp_err_t my_get_handler(httpd_req_t *req);
void serve_mqtt_config_page(httpd_req_t *req);
void serve_influxdb_config_page(httpd_req_t *req);

#endif