cmake_minimum_required(VERSION 3.16...3.21)

project(obs-pulseaudio-app-capture VERSION 0.2.0)
add_library(${CMAKE_PROJECT_NAME} MODULE)

set(PLUGIN_AUTHOR "Joshua Wong")

set(MACOS_BUNDLEID "com.jbwong05.${CMAKE_PROJECT_NAME}")

# Replace `me@contoso.com` with the maintainer email address you want to put in Linux packages
set(LINUX_MAINTAINER_EMAIL "jbwong05@gmail.com")

# Add your custom source files here - header files are optional and only required for visibility
# e.g. in Xcode or Visual Studio
target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/pulse-app-capture.c src/pulse-app-input.cpp
                                             src/pulse-wrapper.c)

# Import libobs as main plugin dependency
find_package(libobs REQUIRED)
include(cmake/ObsPluginHelpers.cmake)

find_package(obs-frontend-api REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)

find_qt(COMPONENTS Widgets Core)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets ${PULSEAUDIO_LIBRARY})
set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON)

find_package(PulseAudio REQUIRED)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${PULSEAUDIO_INCLUDE_DIR})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${PULSEAUDIO_LIBRARY})

configure_file(src/plugin-macros.h.in ${CMAKE_SOURCE_DIR}/src/plugin-macros.generated.h)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-macros.generated.h src/pulse-wrapper.h)

# /!\ TAKE NOTE: No need to edit things past this point /!\

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

# --- End of section ---
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall)

setup_plugin_target(${CMAKE_PROJECT_NAME})
