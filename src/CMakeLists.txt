idf_component_register(
    SRCS 
        "main.cpp"
        "sensor_compile_validation.cpp"
    
    INCLUDE_DIRS
        "../lib"
    
    REQUIRES
        freertos
        driver
        esp_driver_i2c
)
