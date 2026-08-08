set(APP_DIR app)

# Collect app core source files
file(GLOB APP_CORE_C_SOURCES ${APP_DIR}/*.c)

list(FILTER APP_CORE_C_SOURCES EXCLUDE REGEX ".*app_qma6100\\.c$")

# Collect AI module source files
file(GLOB APP_AI_CPP_SOURCES ${APP_DIR}/AI/*.cpp)
file(GLOB APP_AI_C_SOURCES ${APP_DIR}/AI/*.c)

# Collect Video module source files  
file(GLOB APP_VIDEO_C_SOURCES ${APP_DIR}/Video/*.c)

# Combine all app source files
set(APP_ALL_SOURCES 
    ${APP_CORE_C_SOURCES}
    ${APP_AI_CPP_SOURCES} 
    ${APP_AI_C_SOURCES}
    ${APP_VIDEO_C_SOURCES}
)

set(LV_UI_DIR ui)
file(GLOB_RECURSE LV_UI_SOURCES ${LV_UI_DIR}/*.c)

idf_component_register(SRCS "main.c" ${LV_UI_SOURCES} ${APP_ALL_SOURCES}
                    INCLUDE_DIRS "." ${LV_UI_DIR} ${APP_DIR} ${APP_DIR}/AI ${APP_DIR}/Video
                    REQUIRES esp_driver_ppa nvs_flash human_face_detect pedestrian_detect coco_detect esp_painter esp32_p4_wifi6_touch_lcd_35 bsp_extra esp_video)

target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>
)
