find_package(Qt6 REQUIRED COMPONENTS Test)

# Adds a Qt Test executable and registers it with CTest. GUI-related tests
# run on the "offscreen" platform so they work without a visible display.
function(visualdsa_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE ${ARGN} Qt6::Test visualdsa_warnings)
    # Keep tests out of the app's bin/ folder: windeployqt puts only the
    # "windows" platform plugin there, and Qt prefers plugins next to the
    # executable, which would hide the "offscreen" plugin the tests need.
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/bin")
    add_test(NAME ${name} COMMAND ${name})
    set_property(TEST ${name} PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    if(WIN32)
        # The offscreen platform does not read system fonts on its own.
        set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT "QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts")
        # Let the test find the Qt DLLs without installing them system-wide.
        set_property(TEST ${name} PROPERTY
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    endif()
endfunction()

visualdsa_add_test(tst_binarysearchtree visualdsa_core)
visualdsa_add_test(tst_treeoperations   visualdsa_core)
visualdsa_add_test(tst_treeinvariants   visualdsa_core)

if(VISUALDSA_BUILD_APP)
    visualdsa_add_test(tst_visualization visualdsa_ui)
endif()
