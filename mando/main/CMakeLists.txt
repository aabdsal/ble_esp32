idf_component_register(
    SRCS 
        "main.c"
        "botones.c"
        "ble_client.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash
)
