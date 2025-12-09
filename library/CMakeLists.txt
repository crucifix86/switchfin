cmake_minimum_required(VERSION 3.15)

# add lunasvg
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_subdirectory(lunasvg EXCLUDE_FROM_ALL)

# add borealis
add_subdirectory(borealis/library)

if (PLATFORM_PS4)
    list(APPEND APP_PLATFORM_LIB SceNet mbedcrypto mbedtls mbedx509)
    if (LIBJBC)
        add_subdirectory(libjbc)
        list(APPEND APP_PLATFORM_LIB jbc)
    endif()
    set(APP_PLATFORM_LIB ${APP_PLATFORM_LIB} PARENT_SCOPE)
elseif (PLATFORM_PSV)
    list(APPEND APP_PLATFORM_LIB SceVshBridge_stub)
    set(APP_PLATFORM_LIB ${APP_PLATFORM_LIB} PARENT_SCOPE)
endif ()