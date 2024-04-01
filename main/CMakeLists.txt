# Add C++ file to the project
set(EXTRA_COMPONENT_DIRS components/esp32-wifi-manager)


<<<<<<< HEAD
idf_component_register(SRCS "main.c" "component_init.c" "component_configuration.c" "jkbmsInterface.cpp" "JKBMSWrapper.cpp" 
                     INCLUDE_DIRS "../components/esp32-wifi-manager/src" "."  # Reversed order
                     REQUIRES esp32-wifi-manager esp_http_server esp_wifi mqtt
                     
)
=======
idf_component_register(SRCS "webpage_handler.c" "mqtt_client.c" "wifi_prov_mgr.c" "main.c" "jkbmsInterface.cpp" "JKBMSWrapper.cpp" 
                    INCLUDE_DIRS ".")
>>>>>>> e6332569fbe825b3440c749eb31c61a9c664c713
