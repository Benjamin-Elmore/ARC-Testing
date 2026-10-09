idf_component_register(
    SRCS 
        "main.cpp"
    
    INCLUDE_DIRS
        "../lib"
    
    REQUIRES
        freertos
        driver
        esp_driver_i2c
)
