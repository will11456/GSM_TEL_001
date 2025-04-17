idf_component_register(SRCS "output.c" "tmp102.c" "modem.c" "adc.c" "main.c" "handler.c" "inputs.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver log nvs_flash
                    )
