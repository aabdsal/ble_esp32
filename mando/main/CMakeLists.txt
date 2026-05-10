idf_component_register(SRCS "main.c" "src/bluetooth.c" 
                    REQUIRES bt 
                    INCLUDE_DIRS "." "include" "src")