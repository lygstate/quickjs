cmake_minimum_required(VERSION 3.5)
project(quickjs)

if(NOT DEFINED CONFIG_BIGNUM)
  set(CONFIG_BIGNUM 1)
endif()

include(CTest)
set(QUICKJS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
include(cmake/quickjs.cmake)

set(QUICKJS_HOST_DIR ${CMAKE_CURRENT_BINARY_DIR}/quickjs-host/${CMAKE_BUILD_TYPE})
set(UNICODE_GEN_EXECUTABLE ${QUICKJS_HOST_DIR}/install/bin/unicode_gen${CMAKE_EXECUTABLE_SUFFIX})
set(QJSC_EXECUTABLE ${QUICKJS_HOST_DIR}/install/bin/qjsc${CMAKE_EXECUTABLE_SUFFIX})

include(ExternalProject)
ExternalProject_Add(hostquickjs
  PREFIX ${QUICKJS_HOST_DIR}
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/host
  BUILD_IN_SOURCE 0
  BINARY_DIR ${QUICKJS_HOST_DIR}/build
  CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX=${QUICKJS_HOST_DIR}/install
    -DCONFIG_BIGNUM=${CONFIG_BIGNUM}
  BUILD_BYPRODUCTS 
    ${QJSC_EXECUTABLE}
    ${UNICODE_GEN_EXECUTABLE}
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/repl.c
  COMMAND ${QJSC_EXECUTABLE} -c -o ${CMAKE_CURRENT_BINARY_DIR}/repl.c -m ${CMAKE_CURRENT_SOURCE_DIR}/repl.js
  DEPENDS ${QJSC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/repl.js hostquickjs
  COMMENT "Generate repl.c from repl.js"
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/qjscalc.c
  COMMAND ${QJSC_EXECUTABLE} -fbignum -c -o ${CMAKE_CURRENT_BINARY_DIR}/qjscalc.c ${CMAKE_CURRENT_SOURCE_DIR}/qjscalc.js
  DEPENDS ${QJSC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/qjscalc.js hostquickjs
  COMMENT "Generate repl.c from repl.js"
)

add_executable(qjs
  ${CMAKE_CURRENT_BINARY_DIR}/repl.c
  ${CMAKE_CURRENT_BINARY_DIR}/qjscalc.c
  qjs.c
)

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/unicode/UnicodeData.txt)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/libunicode-table.h
    COMMAND ${UNICODE_GEN_EXECUTABLE} unicode ${CMAKE_CURRENT_BINARY_DIR}/libunicode-table.h
    DEPENDS ${UNICODE_GEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/unicode/UnicodeData.txt hostquickjs
    COMMENT "Generate repl.c from repl.js"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
  )
  add_custom_target(unicode_gen_header
    BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/libunicode-table.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/libunicode-table.h ${CMAKE_CURRENT_SOURCE_DIR}/libunicode-table.h
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libunicode-table.h
  )
  add_dependencies(qjs unicode_gen_header)
endif()

qjs_setup_common_flags(qjs)
target_link_libraries(qjs PRIVATE ${QUICKJS_LINK_LIBRARIES})

# Install the target
include(GNUInstallDirs)
install(TARGETS quickjs EXPORT quickjsTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/quickjs COMPONENT quickjs
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/quickjs COMPONENT quickjs
)

# Create and install CMake configs
include(CMakePackageConfigHelpers)

write_basic_package_version_file(quickjsConfigVersion.cmake
  VERSION ${CONFIG_VERSION}
  COMPATIBILITY SameMajorVersion
)
configure_package_config_file(quickjsConfig.cmake.in
  quickjsConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quickjs
)
install(EXPORT quickjsTargets
  FILE quickjsTargets.cmake
  NAMESPACE quickjs::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quickjs
  COMPONENT quickjs
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/quickjsConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/quickjsConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/quickjs
  COMPONENT quickjs
)

# Install includes
install(FILES
  quickjs.h quickjs-libc.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/quickjs
  COMPONENT quickjs
)
target_include_directories(quickjs PUBLIC
  $<BUILD_INTERFACE:${quickjs_BINARY_DIR}>
  $<BUILD_INTERFACE:${quickjs_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

add_subdirectory(tests)
