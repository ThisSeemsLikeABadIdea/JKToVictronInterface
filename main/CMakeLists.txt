# Add C++ file to the project
set(EXTRA_COMPONENT_DIRS components/esp32-wifi-manager)


idf_component_register(SRCS "main.c" "component_init.c" "component_configuration.c" "jkbmsInterface.cpp" "JKBMSWrapper.cpp" 
                     INCLUDE_DIRS "../components/esp32-wifi-manager/src" "."  # Reversed order
                     REQUIRES esp32-wifi-manager esp_http_server esp_wifi mqtt
                     
)