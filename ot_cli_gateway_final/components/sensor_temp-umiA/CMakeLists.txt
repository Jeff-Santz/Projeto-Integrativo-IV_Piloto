

idf_component_register(
    SRCS "sensor_temp-umiA.c"
    INCLUDE_DIRS "."
    REQUIRES aht i2cdev
)
