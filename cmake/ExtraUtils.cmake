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

        set(CLANG_TIDY_STYLE -*)
        set(CLANG_TIDY_STYLE ${CLANG_TIDY_STYLE},clang-analyzer-cplusplus*)
        set(CLANG_TIDY_STYLE ${CLANG_TIDY_STYLE},performance-*)
        set(CLANG_TIDY_STYLE ${CLANG_TIDY_STYLE},portability-*)
        set(CLANG_TIDY_STYLE ${CLANG_TIDY_STYLE},modernize-*)
        set(CLANG_TIDY_STYLE ${CLANG_TIDY_STYLE},cppcoreguidelines-*)
        set(CLANG_TIDY_STYLE ${CLANG_TIDY_STYLE},bugprone-*)
        set(CLANG_TIDY_STYLE ${CLANG_TIDY_STYLE},concurrency-*)

        
        set(CLANG_TIDY_STYLE ${CLANG_TIDY_STYLE},-cppcoreguidelines-owning-memory)                     # smart pointers are nice, but deps not always use them
        set(CLANG_TIDY_STYLE ${CLANG_TIDY_STYLE},-modernize-use-trailing-return-type)                  # never argue about style
        set(CLANG_TIDY_STYLE ${CLANG_TIDY_STYLE},-cppcoreguidelines-pro-bounds-pointer-arithmetic)     # we are close to hardware and need pointer magic
        set(CLANG_TIDY_STYLE ${CLANG_TIDY_STYLE},-cppcoreguidelines-pro-type-reinterpret-cast)         # reinterpret_cast is necessary
        set(CLANG_TIDY_STYLE ${CLANG_TIDY_STYLE},-cppcoreguidelines-pro-type-const-cast)               # const_cast is useful
        set(CLANG_TIDY_STYLE ${CLANG_TIDY_STYLE},-cppcoreguidelines-pro-bounds-array-to-pointer-decay) # this is just annoying
        set(CLANG_TIDY_STYLE ${CLANG_TIDY_STYLE},-cppcoreguidelines-avoid-magic-numbers)               # we take care of magic numbers in our way

        if(CLANG_TIDY_EXECUTABLE)
            message(STATUS "Using Clang-Tidy ${CLANG_TIDY_EXECUTABLE}")
            set(CLANG_TIDY_ARGS
                "--checks=${CLANG_TIDY_STYLE}"
                "--quiet")
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

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.18)
    include(CheckLinkerFlag)
    set(NEW_RPATH_FLAGS "-Wl,--enable-new-dtags")
    check_linker_flag(CXX "${NEW_RPATH_FLAGS}" HAS_NEW_RPATH_FLAGS)
endif()

if(CMAKE_VERSION VERSION_GREATER 3.9)
    if(IG_USE_LTO)
        include(CheckLinkerFlag)
        set(CUSTOM_LTO_FLAGS "-flto=auto") # Minor workaround until https://gitlab.kitware.com/cmake/cmake/-/issues/26353 gets through
        check_linker_flag(CXX "${CUSTOM_LTO_FLAGS}" HAS_CUSTOM_LTO_FLAGS)

        if(HAS_CUSTOM_LTO_FLAGS)
            set(LTO_ON TRUE)
        else()
            include(CheckIPOSupported)
            check_ipo_supported(RESULT supported OUTPUT error)

            if(supported)
                set(LTO_ON TRUE)
            else()
                message(WARNING "IPO / LTO not available: ${error}")
            endif()
        endif()

        if(LTO_ON)
            message(STATUS "IPO / LTO enabled")
        endif()
    endif()
endif()

function(ig_add_extra_options TARGET)
    set(options NO_DEFAULT_CONSOLE NO_LTO NO_CHECKS)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "${options}" "" "")

    get_target_property(target_type ${TARGET} TYPE)

    if(HAS_NEW_RPATH_FLAGS)
        target_link_options(${TARGET} PRIVATE ${NEW_RPATH_FLAGS})
    endif()

    if(target_type STREQUAL "EXECUTABLE")
        if(MSVC)
            target_link_options(${TARGET} PRIVATE "/STACK:16777216")

            if(ARG_NO_DEFAULT_CONSOLE)
                # Do not open up a console but keep main()
                target_link_options(${TARGET} PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
            endif()
        endif()
    endif()

    if(NOT NO_LTO)
        if(LTO_ON)
            if(NOT target_type STREQUAL "STATIC_LIBRARY")
                if(HAS_CUSTOM_LTO_FLAGS)
                    target_link_options(${TARGET} PRIVATE ${CUSTOM_LTO_FLAGS})
                else()
                    set_property(TARGET ${TARGET} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
                endif()
            endif()
        endif()
    endif()

    if(NOT NO_CHECKS)
        if(IG_USE_IWYU AND IWYU_EXECUTABLE)
            set_property(TARGET ${TARGET} PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_EXECUTABLE} ${IWYU_ARGS})
        endif()

        if(IG_USE_CLANG_TIDY AND CLANG_TIDY_EXECUTABLE)
            set_property(TARGET ${TARGET} PROPERTY CXX_CLANG_TIDY ${CLANG_TIDY_EXECUTABLE} ${CLANG_TIDY_ARGS})
        endif()

        if(IG_USE_CPPLINT AND CPPLINT_EXECUTABLE)
            set_property(TARGET ${TARGET} PROPERTY CXX_CPPLINT ${CPPLINT_EXECUTABLE} ${CPPLINT_ARGS})
        endif()

        if(IG_USE_CPPCHECK AND CPPCHECK_EXECUTABLE)
            set_property(TARGET ${TARGET} PROPERTY CXX_CPPCHECK ${CPPCHECK_EXECUTABLE} ${CPPCHECK_ARGS})
        endif()
    endif()
endfunction()
