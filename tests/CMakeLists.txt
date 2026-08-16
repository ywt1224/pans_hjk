file(GLOB PANS_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"    
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
)

list(SORT PANS_TEST_SOURCES)

find_package(Threads REQUIRED)

set(PANS_TEST_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/tests")

#完成的任务就是遍历所有测试文件，都形成独自的可执行文件
#然后放到设定的目录下
foreach(TEST_SOURCE IN LISTS PANS_TEST_SOURCES)
    #获取不包含目录也不包含最长扩展名的文件名字
    get_filename_component(TEST_NAME "${TEST_SOURCE}" NAME_WE)

    #如果已存在同名的文件，就停止，由 FATAL_ERROR 决定的行为
    if(TARGET "${TEST_NAME}")
        message(FATAL_ERROR "Cannot create test target '${TEST_NAME}': a target with that name already exists")
    endif()

    add_executable("${TEST_NAME}" "${TEST_SOURCE}")

    # pans 的 PUBLIC 依赖会让测试同时继承头文件路径和 pans_options。
    target_link_libraries("${TEST_NAME}" PRIVATE pans Threads::Threads)
    
    # 为单配置生成器单独配置输出目录（因为需要设置的宏不一样）
    set_target_properties("${TEST_NAME}" PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PANS_TEST_OUTPUT_DIRECTORY}"
    )
    
    # 多配置生成器的可用生成类型，默认会追加 Debug/Release 子目录，这里保持输出路径一致。
    foreach(CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${CONFIG}" CONFIG_UPPER)
        #设置所有的可用生成类型的输出目录都为原来指定的那个
        set_target_properties("${TEST_NAME}" PROPERTIES
            "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}" "${PANS_TEST_OUTPUT_DIRECTORY}"
            # RUNTIME_OUTPUT_DIRECTORY "${PANS_TEST_OUTPUT_DIRECTORY}"
        )   
    endforeach()
    
    #添加一个指定名字的测试，测试命令就是 COMMAND 后的，
    #可执行对象会自动替换为构建时创建的可执行文件的位置
    add_test(NAME "${TEST_NAME}" COMMAND "${TEST_NAME}")
endforeach()
