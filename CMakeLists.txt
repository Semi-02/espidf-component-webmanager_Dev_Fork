idf_component_register(
    INCLUDE_DIRS "cpp"
    SRCS "cpp/webmanager.cc"
    REQUIRES "main" "flatbuffers-25.1.21" "mdns" "common"  "errorcodes" "esp_http_server" "esp_https_server" "spiffs"  "app_update" "esp_wifi"
    )