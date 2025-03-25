idf_component_register(
    SRCS 
    "main.c"
    "./app_lib/app_lib.c"
                    
    INCLUDE_DIRS 
    "."
    "./app_lib/include"
    )

lvgl_port_create_c_image("images/esp_logo.png" "images/" "ARGB8888" "NONE")
lvgl_port_add_images(${COMPONENT_LIB} "images/")

set_source_files_properties(
    PROPERTIES COMPILE_OPTIONS
    "-DLV_LVGL_H_INCLUDE_SIMPLE;-Wno-format;"
    )

