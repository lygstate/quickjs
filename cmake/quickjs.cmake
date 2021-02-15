
# Read the version off the VERSION file
file(READ ${QUICKJS_SOURCE_DIR}/VERSION CONFIG_VERSION)
message(STATUS "${QUICKJS_SOURCE_DIR}/VERSION ${CONFIG_VERSION}")

string(REGEX REPLACE "\n$" "" CONFIG_VERSION "${CONFIG_VERSION}")

function(qjs_setup_common_flags target)
  target_compile_definitions(${target} PRIVATE
    CONFIG_VERSION="${CONFIG_VERSION}"
    CONFIG_BIGNUM="${CONFIG_BIGNUM}"
  )
  if (UNIX)
    target_compile_definitions(${target} PRIVATE _GNU_SOURCE)
  endif()
  if (NOT ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC"))
    target_compile_options(${target} PRIVATE -fPIC)
  endif()
  if (MSVC_VERSION GREATER 1927)
    target_compile_features(${target} PUBLIC c_std_11)
  endif()
endfunction()

# Define the target
add_library(quickjs STATIC
  ${QUICKJS_SOURCE_DIR}/cutils.c
  ${QUICKJS_SOURCE_DIR}/libbf.c
  ${QUICKJS_SOURCE_DIR}/libregexp.c
  ${QUICKJS_SOURCE_DIR}/libunicode.c
  ${QUICKJS_SOURCE_DIR}/quickjs.c
  ${QUICKJS_SOURCE_DIR}/quickjs-libc.c
  ${QUICKJS_SOURCE_DIR}/quickjs-port.c
)
qjs_setup_common_flags(quickjs)
if("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
  target_compile_options(quickjs PRIVATE -wd4244 -wd4996 -wd4018 -wd4146 -wd4013)
endif()
add_library(quickjs::quickjs ALIAS quickjs)
set_target_properties(quickjs PROPERTIES
  VERSION ${CONFIG_VERSION}
)

set(QUICKJS_LINK_LIBRARIES quickjs)
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  list(APPEND QUICKJS_LINK_LIBRARIES m dl pthread)
endif()
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  list(APPEND QUICKJS_LINK_LIBRARIES Ws2_32)
endif()
add_executable(qjsc ${QUICKJS_SOURCE_DIR}/qjsc.c)
qjs_setup_common_flags(qjsc)
target_link_libraries(qjsc PRIVATE ${QUICKJS_LINK_LIBRARIES})

add_executable(unicode_gen
  ${QUICKJS_SOURCE_DIR}/unicode_gen.c
  ${QUICKJS_SOURCE_DIR}/cutils.c
)

add_executable(run-test262
  ${QUICKJS_SOURCE_DIR}/run-test262.c
)
qjs_setup_common_flags(run-test262)
target_link_libraries(run-test262 PRIVATE ${QUICKJS_LINK_LIBRARIES})

add_executable(regexp_test
  ${QUICKJS_SOURCE_DIR}/libregexp.c
  ${QUICKJS_SOURCE_DIR}/libunicode.c
  ${QUICKJS_SOURCE_DIR}/cutils.c
)
target_compile_definitions(regexp_test PRIVATE -DTEST)