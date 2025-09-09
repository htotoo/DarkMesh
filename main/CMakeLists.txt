idf_component_register(SRCS "main.cpp" "parson.c" "TMAttack.cpp"
                      REQUIRES spi_flash esp_wifi spiffs  mdns nvs_flash esp_http_server app_update EspMeshtasticCompact
                    INCLUDE_DIRS "" EMBED_FILES ../data/web/index.html )
