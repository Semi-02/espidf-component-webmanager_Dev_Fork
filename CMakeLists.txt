idf_component_register(
    INCLUDE_DIRS "cpp" "generated"
    SRCS "cpp/webmanager.cc"
    REQUIRES "flatbuffers-25.1.21" "mdns" "common"  "errorcodes" "esp_http_server" "esp_https_server" "spiffs"  "app_update" "esp_wifi"
    )