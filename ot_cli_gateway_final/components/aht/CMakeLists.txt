idf_component_register(
    SRCS "aht.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_common i2cdev
)
