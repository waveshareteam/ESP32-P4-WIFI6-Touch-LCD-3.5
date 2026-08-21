# Central ESP32-P4 silicon-revision defaults for every first-party ESP-IDF app.
function(waveshare_configure_revision_profile project_dir)
    set(WAVESHARE_REVISION_PROFILE "rev3_x" CACHE STRING
        "ESP32-P4 revision profile (rev1_3 or rev3_x)")
    set_property(CACHE WAVESHARE_REVISION_PROFILE PROPERTY STRINGS rev1_3 rev3_x)

    if(NOT WAVESHARE_REVISION_PROFILE STREQUAL "rev1_3" AND
       NOT WAVESHARE_REVISION_PROFILE STREQUAL "rev3_x")
        message(FATAL_ERROR "Unknown WAVESHARE_REVISION_PROFILE: ${WAVESHARE_REVISION_PROFILE}")
    endif()

    set(profile_defaults "${project_dir}/sdkconfig.defaults.${WAVESHARE_REVISION_PROFILE}")
    if(NOT EXISTS "${profile_defaults}")
        message(FATAL_ERROR "Missing revision profile defaults: ${profile_defaults}")
    endif()

    set(defaults "")
    if(EXISTS "${project_dir}/sdkconfig.defaults")
        list(APPEND defaults "${project_dir}/sdkconfig.defaults")
    endif()
    list(APPEND defaults "${profile_defaults}")
    set(SDKCONFIG_DEFAULTS "${defaults}" CACHE STRING
        "Project sdkconfig defaults including the selected P4 revision profile" FORCE)
    message(STATUS "ESP32-P4 revision profile: ${WAVESHARE_REVISION_PROFILE}")
endfunction()
