cmake_minimum_required(VERSION 3.11 FATAL_ERROR)
cmake_policy(SET CMP0074 NEW) # use package_ROOT env var to find packages
project(k4Overlay LANGUAGES CXX)

# project version
SET( ${PROJECT_NAME}_VERSION_MAJOR 0 )
SET( ${PROJECT_NAME}_VERSION_MINOR 1 )
SET( ${PROJECT_NAME}_VERSION_PATCH 0 )

SET( ${PROJECT_NAME}_VERSION  "${${PROJECT_NAME}_VERSION_MAJOR}.${${PROJECT_NAME}_VERSION_MINOR}.${${PROJECT_NAME}_VERSION_PATCH}" )

find_package(Gaudi CONFIG REQUIRED)
find_package(podio REQUIRED)
find_package(EDM4HEP REQUIRED)
find_package(k4FWCore REQUIRED)

find_package(ROOT REQUIRED)
include(${ROOT_USE_FILE})

add_subdirectory(k4Overlay)

