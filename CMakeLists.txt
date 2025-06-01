cmake_minimum_required(VERSION 3.21)
project(subsdk CXX C ASM)

## Error if not using switch toolchain file
if (NOT SWITCH)
  message(FATAL_ERROR "Not targeting switch, make sure to specify -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain.cmake")
endif ()

if (LOGGER_IP)
  add_compile_definitions(LOGGER_IP="${LOGGER_IP}")
endif ()

## Title Id
set(TITLE_ID 0100c17017cbc000)
set(CONFIG_TITLE_ID 0x${TITLE_ID})
add_compile_definitions(EXL_PROGRAM_ID=${CONFIG_TITLE_ID} NNSDK=1)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/module/subsdk9.json.template ${CMAKE_CURRENT_BINARY_DIR}/subsdk9.json)

## subsdk9
set(CMAKE_EXECUTABLE_SUFFIX ".elf")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DISEMU=${ISEMU}")

# Project sources
file(GLOB_RECURSE SKYLINE_SOURCES 
  ${PROJECT_SOURCE_DIR}/vendor/skyline/*.h 
  ${PROJECT_SOURCE_DIR}/vendor/skyline/*.hpp
  ${PROJECT_SOURCE_DIR}/vendor/skyline/*.c
  ${PROJECT_SOURCE_DIR}/vendor/skyline/*.cpp
)

file(GLOB_RECURSE EXLAUNCH_SOURCES_ASM ${PROJECT_SOURCE_DIR}/vendor/exlaunch/*.s)
file(GLOB_RECURSE EXLAUNCH_SOURCES_H ${PROJECT_SOURCE_DIR}/vendor/exlaunch/*.h ${PROJECT_SOURCE_DIR}/vendor/exlaunch/*.hpp)
file(GLOB_RECURSE EXLAUNCH_SOURCES_CXX ${PROJECT_SOURCE_DIR}/vendor/exlaunch/*.cpp ${PROJECT_SOURCE_DIR}/vendor/exlaunch/*.c)
file(GLOB_RECURSE SOURCES_ASM ${PROJECT_SOURCE_DIR}/src/*.s)
file(GLOB_RECURSE SOURCES_H ${PROJECT_SOURCE_DIR}/src/*.h ${PROJECT_SOURCE_DIR}/src/*.hpp)
file(GLOB_RECURSE SOURCES_CXX ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE LOCAL_LIBRARIES ${PROJECT_SOURCE_DIR}/libs/*.a)

add_library(cjson STATIC
  ${PROJECT_SOURCE_DIR}/vendor/cjson/cJSON.c
  ${PROJECT_SOURCE_DIR}/vendor/cjson/cJSON.h
)

set_target_properties(cjson PROPERTIES
  C_STANDARD 90
)

add_executable(subsdk9 
  ${SOURCES_ASM}
  ${SOURCES_C}
  ${SOURCES_H}
  ${SOURCES_CXX}
  ${EXLAUNCH_SOURCES_H}
  ${EXLAUNCH_SOURCES_CXX}
  ${EXLAUNCH_SOURCES_ASM}
  ${SKYLINE_SOURCES}
)

target_link_libraries(subsdk9 cjson)

set_target_properties(subsdk9 PROPERTIES 
C_STANDARD 17
CXX_STANDARD 23 
CXX_STANDARD_REQUIRED YES)

target_include_directories(subsdk9 PUBLIC ${PROJECT_SOURCE_DIR}/vendor/exlaunch)
target_include_directories(subsdk9 PUBLIC ${PROJECT_SOURCE_DIR}/vendor/exlaunch/lib)
target_include_directories(subsdk9 PUBLIC ${PROJECT_SOURCE_DIR}/vendor/exlaunch/nvn)
target_include_directories(subsdk9 PUBLIC ${PROJECT_SOURCE_DIR}/vendor/exlaunch/nn)
target_include_directories(subsdk9 PUBLIC ${PROJECT_SOURCE_DIR}/vendor/exlaunch/rtld)
target_include_directories(subsdk9 PUBLIC ${PROJECT_SOURCE_DIR}/vendor)
target_include_directories(subsdk9 PUBLIC ${PROJECT_SOURCE_DIR}/src/)

## Include nx tools
include(${CMAKE_SOURCE_DIR}/cmake/SwitchTools.cmake)


#set_source_files_properties(${SOURCES_CXX} PROPERTIES LANGUAGE CXX)

add_compile_definitions(EXL_LOAD_KIND=Module EXL_LOAD_KIND_ENUM=EXL_LOAD_KIND_MODULE)
# target_link_libraries(   ${LOCAL_LIBRARIES})

## Prepare subsdk ld
set(SUBSDK_LD_PATH ${CMAKE_CURRENT_SOURCE_DIR}/module/subsdk9.ld)
execute_process(COMMAND uname OUTPUT_VARIABLE uname)
if (uname MATCHES "^MINGW")
  string(REGEX REPLACE "^/([a-zA-Z])/" "\\1:/" SUBSDK_LD_PATH "${SUBSDK_LD_PATH}")
endif ()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/module/subsdk9.specs.template ${CMAKE_CURRENT_BINARY_DIR}/subsdk9.specs)

## Build nso & generate npdm
add_nso_target_subsdk(subsdk9)
set_target_properties(subsdk9 PROPERTIES CONFIG_JSON "${CMAKE_CURRENT_BINARY_DIR}/subsdk9.json")
__generate_npdm(subsdk9)
set_target_properties(subsdk9 PROPERTIES LINK_FLAGS "-specs ${CMAKE_BINARY_DIR}/subsdk9.specs -g3 ${ARCH} -nostartfiles")

add_custom_target(subsdk9_meta DEPENDS create_npdm subsdk9_nso)

install(FILES ${CMAKE_BINARY_DIR}/subsdk9 DESTINATION atmosphere/contents/${TITLE_ID}/exefs)
install(FILES ${CMAKE_BINARY_DIR}/main.npdm DESTINATION atmosphere/contents/${TITLE_ID}/exefs)
install(FILES ${PROJECT_SOURCE_DIR}/src/gamedef.json DESTINATION atmosphere/contents/${TITLE_ID}/romfs/system)
install(FILES ${PROJECT_SOURCE_DIR}/src/gamedef-jp.json DESTINATION atmosphere/contents/${TITLE_ID}/romfs/system)

if (FTP_IP)
  ## Upload to target switch
  if(FTP_USERNAME AND FTP_PASSWORD) # login using a username and password
    install(CODE "
      execute_process(
        COMMAND ncftpput -u ${FTP_USERNAME} -p ${FTP_PASSWORD} -P 5000 ${FTP_IP} /atmosphere/contents/${TITLE_ID}/exefs ${CMAKE_BINARY_DIR}/main.npdm ${CMAKE_BINARY_DIR}/subsdk9
      )
    ")
  else()
    install(CODE "
        execute_process(
          COMMAND ncftpput -u crafty -p boss -P 5000 ${FTP_IP} /atmosphere/contents/${TITLE_ID}/exefs ${CMAKE_BINARY_DIR}/main.npdm ${CMAKE_BINARY_DIR}/subsdk9
        )
      ")
  endif()
endif ()
