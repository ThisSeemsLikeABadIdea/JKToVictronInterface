# Add C++ file to the project


idf_component_register(SRCS "webpage_handler.c" "mqtt_client.c" "wifi_prov_mgr.c" "main.c" "jkbmsInterface.cpp" "JKBMSWrapper.cpp" 
                    INCLUDE_DIRS ".")
