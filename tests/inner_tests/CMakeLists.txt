set(INNER_TEST_NAME test_log_buffer)
set(INNER_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/test_log_buffer.cpp")

add_executable("${INNER_TEST_NAME}" "${INNER_TEST_SOURCE}")

target_link_libraries("${INNER_TEST_NAME}" PRIVATE pans Threads::Threads)

target_include_directories("${INNER_TEST_NAME}" PRIVATE "${PROJECT_SOURCE_DIR}/pans/src")

#后面都是设置编译的二进制产物的输出路径
set_target_properties("${INNER_TEST_NAME}" PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PANS_TEST_OUTPUT_DIRECTORY}"#这个宏是上层定义的
)

foreach(CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${CONFIG}" CONFIG_UPPER)
    set_target_properties("${INNER_TEST_NAME}" PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}" "${PANS_TEST_OUTPUT_DIRECTORY}"
    )
endforeach()
#添加一个指定名字的测试，测试命令就是 COMMAND 后的，
#可执行对象会自动替换为构建时创建的可执行文件的位置
add_test(NAME "${INNER_TEST_NAME}" COMMAND "${INNER_TEST_NAME}")