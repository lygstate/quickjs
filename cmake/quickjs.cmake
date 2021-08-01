
# Read the version off the VERSION file
file(READ ${QUICKJS_SOURCE_DIR}/VERSION CONFIG_VERSION)
message(STATUS "${QUICKJS_SOURCE_DIR}/VERSION ${CONFIG_VERSION}")

string(REGEX REPLACE "\n$" "" CONFIG_VERSION "${CONFIG_VERSION}")

function(qjs_setup_common_flags target)
  set(QJS_IS_ANSI ${ARGV1})
  target_compile_definitions(${target} PRIVATE
    CONFIG_VERSION="${CONFIG_VERSION}"
    CONFIG_BIGNUM="${CONFIG_BIGNUM}"
  )
  if (UNIX)
    target_compile_definitions(${target} PRIVATE _GNU_SOURCE)
  endif()
  if ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(${target} PRIVATE -wd4244 -wd4996 -wd4018 -wd4146 -wd4013 -wd4102)
    # The stack size setting to 16MB
    target_link_options(${target} PRIVATE -STACK:16777216)
  else()
    target_compile_options(${target} PRIVATE -fPIC)
  endif()
  if(MINGW AND "${QJS_IS_ANSI}" STREQUAL "")
    target_link_options(${target} PRIVATE -municode)
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
  ${QUICKJS_SOURCE_DIR}/cwalk.c
  ${QUICKJS_SOURCE_DIR}/pal-port-shared.c
  ${QUICKJS_SOURCE_DIR}/quickjs.c
  ${QUICKJS_SOURCE_DIR}/quickjs-libc.c
  ${QUICKJS_SOURCE_DIR}/quickjs-debugger.c
  ${QUICKJS_SOURCE_DIR}/quickjs-debugger-transport.c
)

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "General")
else()
  target_sources(quickjs PRIVATE
    ${QUICKJS_SOURCE_DIR}/pal-port-hosted.c
  )
  if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    target_sources(quickjs PRIVATE
      ${QUICKJS_SOURCE_DIR}/pal-port-win.c
      ${QUICKJS_SOURCE_DIR}/win/msvc_stdatomic.c
      ${QUICKJS_SOURCE_DIR}/quickjs-debugger-transport-win.c
    )
  else()
    target_sources(quickjs PRIVATE
      ${QUICKJS_SOURCE_DIR}/pal-port-unix.c
      ${QUICKJS_SOURCE_DIR}/quickjs-debugger-transport-unix.c
    )
  endif()
endif()

qjs_setup_common_flags(quickjs)
add_library(quickjs::quickjs ALIAS quickjs)
set_target_properties(quickjs PROPERTIES
  VERSION ${CONFIG_VERSION}
)

set(QUICKJS_LINK_LIBRARIES quickjs)
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  list(APPEND QUICKJS_LINK_LIBRARIES m dl pthread)
endif()
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  if (MINGW)
    list(APPEND QUICKJS_LINK_LIBRARIES pthread)
  endif()
  list(APPEND QUICKJS_LINK_LIBRARIES Ws2_32)
endif()
add_executable(qjsc ${QUICKJS_SOURCE_DIR}/qjsc.c)
qjs_setup_common_flags(qjsc)
target_link_libraries(qjsc PRIVATE ${QUICKJS_LINK_LIBRARIES})

add_executable(unicode_gen
  ${QUICKJS_SOURCE_DIR}/unicode_gen.c
  ${QUICKJS_SOURCE_DIR}/cutils.c
)
qjs_setup_common_flags(unicode_gen ON)

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
