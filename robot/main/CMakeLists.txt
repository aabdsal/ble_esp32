idf_component_register(
    SRC_DIRS "src" "."
    INCLUDE_DIRS "include"
    REQUIRES bt esp_driver_i2c console esp_driver_gpio nvs_flash
)