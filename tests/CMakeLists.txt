enable_testing()

include(ExternalProject)
ExternalProject_Add(test262
    DOWNLOAD_COMMAND ""
    CONFIGURE_COMMAND ""
    INSTALL_COMMAND ""
    TEST_COMMAND ""
    BUILD_COMMAND ${CMAKE_COMMAND}
        "-DGIT_WORKING_DIRECTORY=${QUICKJS_SOURCE_DIR}"
        -DGIT_CLONE_DIR=test262
        -DGIT_URL=https://github.com/tc39/test262.git
        -DGIT_VERSION=31126581e7290f9233c29cefd93f66c6ac78f1c9
        -P ${QUICKJS_SOURCE_DIR}/cmake/git-clone.cmake
    COMMAND ${CMAKE_COMMAND}
        "-DGIT_WORKING_DIRECTORY=${QUICKJS_SOURCE_DIR}/test262"
        "-DGIT_APPLY_PATCH_PATH=${QUICKJS_SOURCE_DIR}/tests/test262.patch"
        -P ${QUICKJS_SOURCE_DIR}/cmake/git-apply.cmake
    BUILD_IN_SOURCE TRUE
)

if(QUICKJS_ENABLE_SLOW_TEST)
    add_test(
        NAME run-test262
        COMMAND $<TARGET_FILE:run-test262> -m -c test262.conf -a
        WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
    )
    set_tests_properties(run-test262 PROPERTIES DEPENDS "test262")

    add_test(
        NAME microbench
        COMMAND $<TARGET_FILE:qjs> tests/microbench.js
        WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
    )

    set(BENCH_V8_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bench-v8)
    set(BENCH_V8_FILES
        ${BENCH_V8_DIR}/base.js
        ${BENCH_V8_DIR}/richards.js
        ${BENCH_V8_DIR}/deltablue.js
        ${BENCH_V8_DIR}/crypto.js
        ${BENCH_V8_DIR}/raytrace.js
        ${BENCH_V8_DIR}/earley-boyer.js
        ${BENCH_V8_DIR}/regexp.js
        ${BENCH_V8_DIR}/splay.js
        ${BENCH_V8_DIR}/navier-stokes.js
        ${BENCH_V8_DIR}/run_harness.js
    )
    add_custom_target(bench_v8_combined)
    add_custom_command(
            TARGET bench_v8_combined
            COMMAND ${CMAKE_COMMAND}
                "\"-DCOMBINE_INPUT_FILES=${BENCH_V8_FILES}\""
                -DCOMBINE_OUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/combined.js
                -P ${QUICKJS_SOURCE_DIR}/cmake/combine-files.cmake
            DEPENDS ${BENCH_V8_FILES}
            WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
            BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/combined.js)

    add_dependencies(qjs bench_v8_combined)
    add_test(
        NAME qjs-bench-v8
        COMMAND $<TARGET_FILE:qjs> -d ${CMAKE_CURRENT_BINARY_DIR}/combined.js
        WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
    )
endif()

add_test(
    NAME run-test262-failed
    COMMAND $<TARGET_FILE:run-test262> -m -c test262.conf -E -a
    WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
)
set_tests_properties(run-test262-failed PROPERTIES DEPENDS "test262")

add_test(
    NAME test_closure
    COMMAND $<TARGET_FILE:qjs> tests/test_closure.js
    WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
)

add_test(
    NAME test_language
    COMMAND $<TARGET_FILE:qjs> tests/test_language.js
    WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
)

add_test(
    NAME test_builtin
    COMMAND $<TARGET_FILE:qjs> tests/test_builtin.js
    WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
)


add_test(
    NAME test_loop
    COMMAND $<TARGET_FILE:qjs> tests/test_loop.js
    WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
)

add_test(
    NAME test_std
    COMMAND $<TARGET_FILE:qjs> tests/test_std.js
    WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
)

add_test(
    NAME test_worker
    COMMAND $<TARGET_FILE:qjs> tests/test_worker.js
    WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
)

add_library(bjson SHARED
    bjson.c
)
target_link_libraries(bjson PRIVATE ${QUICKJS_LINK_LIBRARIES})
target_compile_options(bjson PRIVATE -DJS_SHARED_LIBRARY)
qjs_setup_common_flags(bjson)
add_custom_target(test_bjson_copy)
add_custom_command(
        TARGET test_bjson_copy
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/test_bjson.js
            ${CMAKE_CURRENT_BINARY_DIR}/test_bjson.js
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_bjson.js
        BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/test_bjson.js)
add_dependencies(qjs test_bjson_copy)
add_test(
    NAME test_bjson_direct
    COMMAND $<TARGET_FILE:qjs> ${CMAKE_CURRENT_BINARY_DIR}/test_bjson.js
    WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
)
if("${CONFIG_BIGNUM}" STREQUAL "1")
    add_test(
        NAME test_bjson_bignum
        COMMAND $<TARGET_FILE:qjs> --bignum ${CMAKE_CURRENT_BINARY_DIR}/test_bjson.js
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/test_bjson.js
        WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
    )
endif()

add_library(point SHARED
    ${QUICKJS_SOURCE_DIR}/examples/point.c
)
target_link_libraries(point PRIVATE ${QUICKJS_LINK_LIBRARIES})
target_compile_options(point PRIVATE -DJS_SHARED_LIBRARY)
qjs_setup_common_flags(point)
add_custom_target(test_point_copy)
add_custom_command(
        TARGET test_point_copy
        COMMAND ${CMAKE_COMMAND} -E copy
            ${QUICKJS_SOURCE_DIR}/examples/test_point.js
            ${CMAKE_CURRENT_BINARY_DIR}/test_point.js
        DEPENDS ${QUICKJS_SOURCE_DIR}/examples/test_point.js
        BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/test_point.js)
add_dependencies(qjs test_point_copy)
add_test(
    NAME test_point
    COMMAND $<TARGET_FILE:qjs> ${CMAKE_CURRENT_BINARY_DIR}/test_point.js
    WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
)

set_target_properties(
  bjson point
  PROPERTIES PREFIX ""
)

if("${CONFIG_BIGNUM}" STREQUAL "1")
    add_test(
        NAME test_op_overloading
        COMMAND $<TARGET_FILE:qjs> --bignum tests/test_op_overloading.js
        WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
    )

    add_test(
        NAME test_bignum
        COMMAND $<TARGET_FILE:qjs> --bignum tests/test_bignum.js
        WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
    )

    add_test(
        NAME test_qjscalc
        COMMAND $<TARGET_FILE:qjs> --bignum --qjscalc tests/test_qjscalc.js
        WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
    )
endif()

add_test(
    NAME qjs_stats
    COMMAND $<TARGET_FILE:qjs> -qd
    WORKING_DIRECTORY ${QUICKJS_SOURCE_DIR}
)
