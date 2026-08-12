# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include("${CMAKE_CURRENT_LIST_DIR}/../../../config/revision_profiles.cmake")
waveshare_configure_revision_profile("${CMAKE_CURRENT_LIST_DIR}")
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(factory_demo VERSION 1.0.0)

# The managed BSP 2.0.0 uses LVGL 9 spellings while Example 12 intentionally
# remains on LVGL 8 for its SquareLine-generated UI assets. Force this private
# compatibility header into the managed BSP only; it must not affect the app or
# any other managed component.
set(lvgl8_managed_bsp_shim
    "${CMAKE_CURRENT_LIST_DIR}/compat/lvgl8_managed_bsp.h"
)
idf_component_get_property(lvgl8_managed_bsp_target
    waveshare__esp32_p4_wifi6_touch_lcd_3_5 COMPONENT_LIB)
if(NOT lvgl8_managed_bsp_target
   OR NOT TARGET "${lvgl8_managed_bsp_target}"
   OR NOT EXISTS "${lvgl8_managed_bsp_shim}")
    message(FATAL_ERROR "LVGL 8 managed BSP compatibility inputs are unavailable")
endif()
target_compile_options("${lvgl8_managed_bsp_target}" PRIVATE
    "SHELL:-include \"${lvgl8_managed_bsp_shim}\"")

# The application provides its own TinyUSB MSC storage callbacks in app_storage.c.
# Keep the MSC class enabled, but do not compile esp_tinyusb's unused IDF 5
# storage helper, whose legacy FATFS registration API is unavailable in IDF 6.
if(IDF_VERSION_MAJOR GREATER_EQUAL 6 AND CONFIG_TINYUSB_MSC_ENABLED)
    idf_component_get_property(esp_tinyusb_lib
        espressif__esp_tinyusb COMPONENT_LIB)
    idf_component_get_property(esp_tinyusb_dir
        espressif__esp_tinyusb COMPONENT_DIR)

    if(NOT esp_tinyusb_lib OR NOT esp_tinyusb_dir)
        message(FATAL_ERROR "esp_tinyusb component properties are unavailable")
    endif()

    set(esp_tinyusb_msc_storage
        "${esp_tinyusb_dir}/tusb_msc_storage.c")

    if(NOT TARGET "${esp_tinyusb_lib}"
       OR NOT EXISTS "${esp_tinyusb_msc_storage}")
        message(FATAL_ERROR "esp_tinyusb 1.7.6~2 MSC compatibility input not found")
    endif()

    get_target_property(esp_tinyusb_sources "${esp_tinyusb_lib}" SOURCES)
    if(NOT esp_tinyusb_sources)
        message(FATAL_ERROR "esp_tinyusb target has no registered sources")
    endif()
    set(esp_tinyusb_msc_storage_registered "")
    foreach(esp_tinyusb_source IN LISTS esp_tinyusb_sources)
        if(IS_ABSOLUTE "${esp_tinyusb_source}")
            set(esp_tinyusb_source_abs "${esp_tinyusb_source}")
        else()
            get_filename_component(esp_tinyusb_source_abs
                "${esp_tinyusb_source}" ABSOLUTE BASE_DIR "${esp_tinyusb_dir}")
        endif()
        if("${esp_tinyusb_source_abs}" STREQUAL "${esp_tinyusb_msc_storage}")
            set(esp_tinyusb_msc_storage_registered "${esp_tinyusb_source}")
            break()
        endif()
    endforeach()

    if(NOT esp_tinyusb_msc_storage_registered)
        message(FATAL_ERROR
            "esp_tinyusb source layout changed; review the MSC compatibility setting")
    endif()

    set_source_files_properties(
        "${esp_tinyusb_msc_storage_registered}"
        TARGET_DIRECTORY "${esp_tinyusb_lib}" PROPERTIES HEADER_FILE_ONLY TRUE)
    message(STATUS "Using application-provided TinyUSB MSC storage callbacks")
endif()

# Suppress warnings only for esp-dl component
idf_component_get_property(esp_dl_lib espressif__esp-dl COMPONENT_LIB)
if(esp_dl_lib)
    target_compile_options(${esp_dl_lib} PRIVATE
        -Wno-unused-variable
        -Wno-unused-but-set-variable
    )
    if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
        # Keep the ESP-DL 3.1 model API while backporting IDF 6 compatibility fixes.
        idf_component_get_property(esp_dl_dir espressif__esp-dl COMPONENT_DIR)
        set(esp_dl_fbs_loader "${esp_dl_dir}/fbs_loader/src/fbs_loader.cpp")
        set(esp_dl_fbs_loader_backport
            "${CMAKE_CURRENT_LIST_DIR}/idf6_compat/esp_dl_fbs_loader.cpp"
        )
        set(esp_dl_image_process "${esp_dl_dir}/vision/image/dl_image_process.cpp")
        set(esp_dl_image_process_backport
            "${CMAKE_CURRENT_BINARY_DIR}/idf6_compat/dl_image_process.cpp"
        )

        if(NOT TARGET ${esp_dl_lib}
           OR NOT EXISTS "${esp_dl_fbs_loader}"
           OR NOT EXISTS "${esp_dl_fbs_loader_backport}"
           OR NOT EXISTS "${esp_dl_image_process}")
            message(FATAL_ERROR "ESP-DL 3.1.3 compatibility inputs not found")
        endif()

        get_target_property(esp_dl_sources ${esp_dl_lib} SOURCES)
        set(esp_dl_fbs_loader_registered "")
        set(esp_dl_image_process_registered "")
        foreach(esp_dl_source IN LISTS esp_dl_sources)
            if(IS_ABSOLUTE "${esp_dl_source}")
                set(esp_dl_source_abs "${esp_dl_source}")
            else()
                get_filename_component(esp_dl_source_abs "${esp_dl_source}" ABSOLUTE
                                       BASE_DIR "${esp_dl_dir}")
            endif()
            if("${esp_dl_source_abs}" STREQUAL "${esp_dl_fbs_loader}")
                set(esp_dl_fbs_loader_registered "${esp_dl_source}")
            elseif("${esp_dl_source_abs}" STREQUAL "${esp_dl_image_process}")
                set(esp_dl_image_process_registered "${esp_dl_source}")
            endif()
            if(esp_dl_fbs_loader_registered AND esp_dl_image_process_registered)
                break()
            endif()
        endforeach()

        if(NOT esp_dl_fbs_loader_registered OR NOT esp_dl_image_process_registered)
            message(FATAL_ERROR "ESP-DL source layout changed; review the compatibility backports")
        endif()

        # ESP-DL 3.1.3 passes float pointers to modf(), which GCC 15 rejects.
        # Generate a guarded build-tree copy that uses the float overload instead.
        file(READ "${esp_dl_image_process}" esp_dl_image_process_contents)
        string(REGEX MATCHALL "modf\\(" esp_dl_modf_matches
               "${esp_dl_image_process_contents}")
        list(LENGTH esp_dl_modf_matches esp_dl_modf_count)
        if(NOT esp_dl_modf_count EQUAL 4)
            message(FATAL_ERROR "ESP-DL image compatibility patch no longer matches 3.1.3")
        endif()
        string(REPLACE "modf(" "modff(" esp_dl_image_process_contents
               "${esp_dl_image_process_contents}")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/idf6_compat")
        file(WRITE "${esp_dl_image_process_backport}"
             "${esp_dl_image_process_contents}")

        set_source_files_properties(
            "${esp_dl_fbs_loader_registered}"
            "${esp_dl_image_process_registered}"
            TARGET_DIRECTORY ${esp_dl_lib} PROPERTIES HEADER_FILE_ONLY TRUE)
        target_sources(${esp_dl_lib} PRIVATE
            "${esp_dl_fbs_loader_backport}"
            "${esp_dl_image_process_backport}"
        )

        # ESP-DL 3.1.3 uses IDF 5 TCM aliases that IDF 6 maps to SPM.
        target_compile_options(${esp_dl_lib} PRIVATE -Wno-error)
    endif()
    message(STATUS "Applied ESP-DL 3.1.3 compatibility settings")
endif()
