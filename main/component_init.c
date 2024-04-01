// component_init.c

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H
#include "wifi_manager.h"
#include "http_app.h"
#endif // WIFI_MANAGER_H
#include "nvs_flash.h"

#define TAG "VictronCanIntegration_wifiInit"
bool Wifi_Connected = false;



typedef struct {
    char mqtt_host[64];
    int mqtt_port;
    char mqtt_username[64];
    char mqtt_password[64];
    char PackName[64];
    char TopicName[64];
} mqtt_config_t;

bool isWifiConnected()
{
    return Wifi_Connected;
}

esp_err_t save_mqtt_config(const mqtt_config_t *config) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("mqtt_config", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(nvs_handle, "config", config, sizeof(mqtt_config_t));
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    return nvs_commit(nvs_handle);
}

esp_err_t load_mqtt_config(mqtt_config_t *config) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("mqtt_config", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t required_size;
    err = nvs_get_blob(nvs_handle, "config", NULL, &required_size);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    if (required_size != sizeof(mqtt_config_t)) {
        nvs_close(nvs_handle);
        return ESP_ERR_NVS_INVALID_LENGTH;
    }

    err = nvs_get_blob(nvs_handle, "config", config, &required_size);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

// Callback for when WiFi is successfully connected
void wifi_connected_callback(void *pvParameter) {
    ESP_LOGI(TAG, "Connected to WiFi!");
    Wifi_Connected = true;
    
    // Implement actions to be taken when connected to WiFi
}

// Callback for when WiFi is disconnected
void wifi_disconnected_callback(void *pvParameter) {
    ESP_LOGI(TAG, "WiFi Disconnected.");
    Wifi_Connected = false;
    // Implement actions to be taken when WiFi is disconnected
}

// static esp_err_t restart_mqtt_get_handler(httpd_req_t *req){

	
// }

static esp_err_t http_get_handler(httpd_req_t *req){

    ESP_LOGI(TAG, "The following URI has been received: %s", req->uri);
      /* Check if the request is for the restart endpoint */
    if(strcmp(req->uri, "/restart") == 0){

        ESP_LOGI(TAG, "Restarting the device");

        /* Add code here to trigger device restart */
        esp_restart();

        /* We won't reach here after restarting */
        return ESP_OK;
    }

    if(strcmp(req->uri, "/mqtt_config") == 0)
    {
        
        ESP_LOGI(TAG, "Serving MQTT configuration page");

        // Load MQTT configuration from NVS
        mqtt_config_t mqtt_config;
        esp_err_t err = load_mqtt_config(&mqtt_config);
        if (err != ESP_OK) {
            // Handle error if MQTT configuration does not exist
            ESP_LOGW(TAG, "Failed to load MQTT configuration from NVS (%d)", err);
            // Set default values or handle as appropriate
            memset(&mqtt_config, 0, sizeof(mqtt_config));
        }
        
    // const char* form = "<html><body>"
    //                 "<h1>MQTT Configuration</h1>"
    //                 "<form action='/save_mqtt_config' method='post' id='mqtt_config_form'>"
    //                 "<label for='mqtt_host'>Host:</label><br>"
    //                 "<input type='text' id='mqtt_host' name='mqtt_host' value='%s'><br>"
    //                 "<label for='mqtt_port'>Port:</label><br>"
    //                 "<input type='text' id='mqtt_port' name='mqtt_port' value='%d'><br>"
    //                 "<label for='mqtt_username'>Username:</label><br>"
    //                 "<input type='text' id='mqtt_username' name='mqtt_username' value='%s'><br>"
    //                 "<label for='mqtt_password'>Password:</label><br>"
    //                 "<input type='password' id='mqtt_password' name='mqtt_password' value='%s'><br>"
    //                 "<input type='submit' value='Submit'>"
    //                 "</form>"
    //                 "<button onclick='restartDevice()'>Restart Device</button>"
    //                 "</body>"
    //                 "<script>"
    //                 "function submitForm(event) {"
    //                 "event.preventDefault();"
    //                 "var form = document.getElementById('mqtt_config_form');"
    //                 "var formData = new FormData(form);"
    //                 "var payload = '';"
    //                 "for (var pair of formData.entries()) {"
    //                 "payload += encodeURIComponent(pair[0]) + '=' + encodeURIComponent(pair[1]) + '&';"
    //                 "}"
    //                 "payload = payload.slice(0, -1);" 
    //                 "fetch('/save_mqtt_config', {"
    //                 "method: 'POST',"
    //                 "headers: {"
    //                 "'Content-Type': 'application/x-www-form-urlencoded'" 
    //                 "},"
    //                 "body: payload"
    //                 "})"
    //                 ".then(response => {"
    //                 "if (!response.ok) {"
    //                 "throw new Error('Network response was not ok');"
    //                 "}"
    //                 "return response.text();"
    //                 "})"
    //                 ".then(data => {"
    //                 "console.log(data);"
    //                 "})"
    //                 ".catch(error => {"
    //                 "console.error('There was a problem with the fetch operation:', error);"
    //                 "});"
    //                 "}"
    //                 "document.getElementById('mqtt_config_form').addEventListener('submit', submitForm);"
    //                 "function restartDevice() {"
    //                 "fetch('/restart', {"
    //                 "method: 'POST'"
    //                 "})"
    //                 ".then(response => {"
    //                 "if (!response.ok) {"
    //                 "throw new Error('Network response was not ok');"
    //                 "}"
    //                 "return response.text();"
    //                 "})"
    //                 ".then(data => {"
    //                 "console.log(data);"
    //                 "})"
    //                 ".catch(error => {"
    //                 "console.error('There was a problem with the fetch operation:', error);"
    //                 "});"
    //                 "}"
    //                 "</script> </html>";
       const char* form = "<html><body>"
                    "<h1>MQTT Configuration</h1>"
                    "<form action='/save_mqtt_config' method='post' id='mqtt_config_form'>"
                    "<label for='mqtt_host'>Host:</label><br>"
                    "<input type='text' id='mqtt_host' name='mqtt_host' value='%s'><br>"
                    "<label for='mqtt_port'>Port:</label><br>"
                    "<input type='text' id='mqtt_port' name='mqtt_port' value='%d'><br>"
                    "<label for='mqtt_username'>Username:</label><br>"
                    "<input type='text' id='mqtt_username' name='mqtt_username' value='%s'><br>"
                    "<label for='mqtt_password'>Password:</label><br>"
                    "<input type='password' id='mqtt_password' name='mqtt_password' value='%s'><br>"
                    "<label for='pack_name'>Pack Name:</label><br>"
                    "<input type='text' id='pack_name' name='pack_name' value='%s'><br>"
                    "<label for='topic_name'>Topic Name:</label><br>"
                    "<input type='text' id='topic_name' name='topic_name' value='%s'><br>"
                    "<input type='submit' value='Submit'>"
                    "</form>"
                    "<button onclick='restartDevice()'>Restart Device</button>"
                    "</body>"
                    "<script>"
                    "function submitForm(event) {"
                    "event.preventDefault();"
                    "var form = document.getElementById('mqtt_config_form');"
                    "var formData = new FormData(form);"
                    "var payload = '';"
                    "for (var pair of formData.entries()) {"
                    "payload += encodeURIComponent(pair[0]) + '=' + encodeURIComponent(pair[1]) + '&';"
                    "}"
                    "payload = payload.slice(0, -1);" 
                    "fetch('/save_mqtt_config', {"
                    "method: 'POST',"
                    "headers: {"
                    "'Content-Type': 'application/x-www-form-urlencoded'" 
                    "},"
                    "body: payload"
                    "})"
                    ".then(response => {"
                    "if (!response.ok) {"
                    "throw new Error('Network response was not ok');"
                    "}"
                    "return response.text();"
                    "})"
                    ".then(data => {"
                    "console.log(data);"
                    "})"
                    ".catch(error => {"
                    "console.error('There was a problem with the fetch operation:', error);"
                    "});"
                    "}"
                    "document.getElementById('mqtt_config_form').addEventListener('submit', submitForm);"
                    "function restartDevice() {"
                    "fetch('/restart', {"
                    "method: 'POST'"
                    "})"
                    ".then(response => {"
                    "if (!response.ok) {"
                    "throw new Error('Network response was not ok');"
                    "}"
                    "return response.text();"
                    "})"
                    ".then(data => {"
                    "console.log(data);"
                    "})"
                    ".catch(error => {"
                    "console.error('There was a problem with the fetch operation:', error);"
                    "});"
                    "}"
                    "</script> </html>";

        int host_len = strlen(mqtt_config.mqtt_host);
        int username_len = strlen(mqtt_config.mqtt_username);
        int password_len = strlen(mqtt_config.mqtt_password);

        // Calculate the required buffer size
        int buffer_size = strlen(form) + host_len + username_len + password_len + 1; // +1 for the null terminator

        char response[buffer_size];

        int formatted_size = sprintf(response, form, mqtt_config.mqtt_host, mqtt_config.mqtt_port, mqtt_config.mqtt_username, mqtt_config.mqtt_password, mqtt_config.PackName, mqtt_config.TopicName);;//snprintf(response, buffer_size, form, mqtt_config.mqtt_host, mqtt_config.mqtt_port, mqtt_config.mqtt_username, mqtt_config.mqtt_password);

        // Check for errors
        if (formatted_size < 0) {
            // Handle error, formatting failed
        }

        httpd_resp_set_status(req, "200 OK");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, response, strlen(response));
    
    return ESP_OK;

    }
	/* our custom page sits at /helloworld in this example */
	if(strcmp(req->uri, "/helloworld") == 0){

		ESP_LOGI(TAG, "Serving page /helloworld");

		const char* response = "<html><body><h1>Hello World!</h1></body></html>";

		httpd_resp_set_status(req, "200 OK");
		httpd_resp_set_type(req, "text/html");
		httpd_resp_send(req, response, strlen(response));
	}
	else{
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}

// static esp_err_t mqtt_config_get_handler(httpd_req_t *req) {
    
// }


static esp_err_t save_mqtt_config_post_handler(httpd_req_t *req) {
    char buf[100];
    int ret, remaining = req->content_len;
    ESP_LOGI(TAG, "Save Called");

    // Make a copy of the received payload
    char payload[remaining + 1];
    memset(payload, 0, sizeof(payload));

    // Receive entire payload
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        strncat(payload, buf, ret); // Append received data to payload
        remaining -= ret;
    }
    ESP_LOGI(TAG, "Received payload: %s", payload);

    // Parse received payload and update MQTT configuration structure
    mqtt_config_t mqtt_config;

    // Iterate over keys in payload
    char *start = payload;
    while (*start != '\0') {
        // Find end of key
        char *end = strchr(start, '=');
        if (end == NULL) {
            break;
        }

        // Extract key and value strings
        size_t key_len = end - start;
        char key[key_len + 1];
        strncpy(key, start, key_len);
        key[key_len] = '\0';
        ++end;

        // Find end of value
        char *next_delimiter = strchr(end, '&');
        size_t value_len = (next_delimiter != NULL) ? (next_delimiter - end) : strlen(end);
        char value[value_len + 1];
        strncpy(value, end, value_len);
        value[value_len] = '\0';

        // Move start position to after the next delimiter
        start = (next_delimiter != NULL) ? (next_delimiter + 1) : strchr(start, '\0');

        // Handle each key-value pair
        if (strcmp(key, "mqtt_host") == 0) {
            strncpy(mqtt_config.mqtt_host, value, sizeof(mqtt_config.mqtt_host));
        } else if (strcmp(key, "mqtt_port") == 0) {
            ESP_LOGI(TAG, "Received payload for mqtt_port: %s", value);
            mqtt_config.mqtt_port = atoi(value);
            ESP_LOGI(TAG, "Parsed mqtt_port value: %d", mqtt_config.mqtt_port);
        } else if (strcmp(key, "mqtt_username") == 0) {
            strncpy(mqtt_config.mqtt_username, value, sizeof(mqtt_config.mqtt_username));
        } else if (strcmp(key, "mqtt_password") == 0) {
            strncpy(mqtt_config.mqtt_password, value, sizeof(mqtt_config.mqtt_password));
        }
    }

    // Save MQTT configuration to NVS
    esp_err_t err = save_mqtt_config(&mqtt_config);
    if (err != ESP_OK) {
        // Handle error if saving MQTT configuration fails
        ESP_LOGE(TAG, "Failed to save MQTT configuration to NVS (%d)", err);
        // Respond with error message or handle as appropriate
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Respond to the client with a simple message
    const char* response = "<html><body><h1>Configuration Saved!</h1></body></html>";
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}


void initialize_wifi_manager() {
    // Initialize the WiFi manager and set up event callbacks
    ESP_LOGI(TAG, "*************** Starting Wifi Manager ***************");
    wifi_manager_start();
    ESP_LOGI(TAG, "*************** Wifi Manager Started ***************");

    http_app_set_handler_hook(HTTP_GET, &http_get_handler); // you cannot register multiple handlers. You must use a conditional argument to deal with routing

    //http_app_set_handler_hook(HTTP_GET, &mqtt_config_get_handler);
    http_app_set_handler_hook(HTTP_POST, &save_mqtt_config_post_handler);
    //http_app_set_handler_hook(HTTP_GET, &restart_mqtt_get_handler);
    // // Register event callbacks as needed
    wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &wifi_connected_callback);
    wifi_manager_set_callback(WM_EVENT_STA_DISCONNECTED, &wifi_disconnected_callback);
}


