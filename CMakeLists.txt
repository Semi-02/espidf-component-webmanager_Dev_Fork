idf_component_register(
                    SRCS "./cpp/webmanager.cc"
                    INCLUDE_DIRS "./cpp"
                    EMBED_FILES "web/dist/index.compressed.br"
                    EMBED_TXTFILES "certificates/esp32.pem.crt"
                                    "certificates/esp32.pem.key"
                                    "certificates/rootCA.pem.crt"
                    REQUIRES main flatbuffers-23.5.26 esp_http_server app_update common esp_partition esp_timer)