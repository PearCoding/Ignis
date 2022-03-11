if(CMAKE_VERSION VERSION_GREATER 3.6)
    # Add iwyu if available
    option(IG_USE_IWYU "Use include-what-you-use while building" OFF)
    IF(IG_USE_IWYU)
        find_program(IWYU_EXECUTABLE NAMES include-what-you-use iwyu)
        IF(IWYU_EXECUTABLE)
            get_filename_component(IWYU_DIR ${IWYU_EXECUTABLE} DIRECTORY)
            file(TO_NATIVE_PATH "${IWYU_DIR}/../share/include-what-you-use" IWYU_SHARE_DIR)
            set(IWYU_MAPPINGS_DIR "${IWYU_SHARE_DIR}"
                CACHE PATH "Directory containing default mappings for iwyu")

            set(IWYU_ARGS
            "-Xiwyu" "--no_comments"
            "-Xiwyu" "--mapping_file=${CMAKE_CURRENT_SOURCE_DIR}/tools/iwyu.imp")

            if(EXISTS "${IWYU_MAPPINGS_DIR}")
                set(IWYU_ARGS ${IWYU_ARGS}
                "-Xiwyu" "--mapping_file=${IWYU_MAPPINGS_DIR}/iwyu.gcc.imp"
                "-Xiwyu" "--mapping_file=${IWYU_MAPPINGS_DIR}/libcxx.imp"
                "-Xiwyu" "--mapping_file=${IWYU_MAPPINGS_DIR}/boost-all.imp"
                "-Xiwyu" "--mapping_file=${IWYU_MAPPINGS_DIR}/qt5_4.imp")
            else()
                message(WARNING "IWYU default mappings could not be added!")
            endif()

            message(STATUS "Using IWYU ${IWYU_EXECUTABLE}")
        ENDIF()
    ENDIF()

    # Add clang-tidy if available
    option(IG_USE_CLANG_TIDY "Use Clang-Tidy" OFF)

    if(IG_USE_CLANG_TIDY)
        find_program(
            CLANG_TIDY_EXECUTABLE
            NAMES "clang-tidy"
            DOC "Path to clang-tidy executable"
        )

        if(CLANG_TIDY_EXECUTABLE)
            message(STATUS "Using Clang-Tidy ${CLANG_TIDY_EXECUTABLE}")
        endif()
    endif()
endif()

if(CMAKE_VERSION VERSION_GREATER 3.8)
    # Add cpplint if available
    option(IG_USE_CPPLINT "Do style check with cpplint." OFF)
    if(IG_USE_CPPLINT)
        find_program(
            CPPLINT_EXECUTABLE
            NAMES "cpplint"
            DOC "Path to cpplint executable"
        )

        set(IG_CPPLINT_STYLE)
        set(IG_CPPLINT_STYLE ${IG_CPPLINT_STYLE}-whitespace/braces,)
        set(IG_CPPLINT_STYLE ${IG_CPPLINT_STYLE}-whitespace/tab,)
        set(IG_CPPLINT_STYLE ${IG_CPPLINT_STYLE}-whitespace/line_length,)
        set(IG_CPPLINT_STYLE ${IG_CPPLINT_STYLE}-whitespace/comments,)
        set(IG_CPPLINT_STYLE ${IG_CPPLINT_STYLE}-whitespace/indent,)
        #set(IG_CPPLINT_STYLE ${IG_CPPLINT_STYLE}-build/include_order,)
        set(IG_CPPLINT_STYLE ${IG_CPPLINT_STYLE}-build/namespaces,)
        set(IG_CPPLINT_STYLE ${IG_CPPLINT_STYLE}-build/include_what_you_use,)
        set(IG_CPPLINT_STYLE ${IG_CPPLINT_STYLE}-build/include,)
        set(IG_CPPLINT_STYLE ${IG_CPPLINT_STYLE}-legal/copyright,)
        set(IG_CPPLINT_STYLE ${IG_CPPLINT_STYLE}-readability/namespace,)
        set(IG_CPPLINT_STYLE ${IG_CPPLINT_STYLE}-readability/todo,)
        set(IG_CPPLINT_STYLE ${IG_CPPLINT_STYLE}-runtime/references,)

        if(CPPLINT_EXECUTABLE)
            message(STATUS "Using cpplint ${CPPLINT_EXECUTABLE}")
            set(CPPLINT_ARGS 
                "--filter=${IG_CPPLINT_STYLE}"
                "--counting=detailed"
                "--extensions=cpp,h,inl"
                "--headers=h,inl"
                "--quiet")
        endif()
    endif()
endif()

if(CMAKE_VERSION VERSION_GREATER 3.10)
    # Add cppcheck if available
    option(IG_USE_CPPCHECK "Do checks with cppcheck." OFF)
    if(IG_USE_CPPCHECK)
        find_program(
            CPPCHECK_EXECUTABLE
            NAMES "cppcheck"
            DOC "Path to cppcheck executable"
        )

        if(CPPCHECK_EXECUTABLE)
            message(STATUS "Using cppcheck ${CPPCHECK_EXECUTABLE}")
            if(WIN32)
                set(ARCH_ARGS "-DWIN32" "-D_MSC_VER")
            else()
                set(ARCH_ARGS "-Dlinux" "-D__GNUC__")
            endif()
            set(CPPCHECK_ARGS ${ARCH_ARGS}
                "--quiet"
                "--enable=warning,style,performance,portability"
                "--suppress=preprocessorErrorDirective"
                "--library=std"
                "--std=c++17")
        endif()
    endif()
endif()

function(add_checks TARGET)
    if(IWYU_EXECUTABLE)
        set_property(TARGET ${TARGET} PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_EXECUTABLE} ${IWYU_ARGS})
    endif()
    if(CLANG_TIDY_EXECUTABLE)
        set_property(TARGET ${TARGET} PROPERTY CXX_CLANG_TIDY ${CLANG_TIDY_EXECUTABLE})
    endif()
    if(CPPLINT_EXECUTABLE)
        set_property(TARGET ${TARGET} PROPERTY CXX_CPPLINT ${CPPLINT_EXECUTABLE} ${CPPLINT_ARGS})
    endif()
    if(CPPCHECK_EXECUTABLE)
        set_property(TARGET ${TARGET} PROPERTY CXX_CPPCHECK ${CPPCHECK_EXECUTABLE} ${CPPCHECK_ARGS})
    endif()
endfunction()