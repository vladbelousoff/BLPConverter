cmake_minimum_required(VERSION 3.5)

project(BLPCONVERTER)

# Set the C++ standard to C++14
set(CMAKE_CXX_STANDARD 14)

# Ensure that the standard is strictly enforced
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional: Disable compiler-specific extensions
set(CMAKE_CXX_EXTENSIONS OFF)

# Suppress all warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Remove any existing /W flags (like /W3)
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/w" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    # Add the /w flag to suppress all warnings
    add_compile_options(/w)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GCC")
    add_compile_options(-w)
endif()

##########################################################################################
# Options

option(WITH_LIBRARY "Compile library" ON)


##########################################################################################
# CMake-related settings

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BLPCONVERTER_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${BLPCONVERTER_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${BLPCONVERTER_BINARY_DIR}/bin")


##########################################################################################
# Dependencies

add_subdirectory(dependencies)

include_directories("${BLPCONVERTER_SOURCE_DIR}/dependencies/include/"
                    "${BLPCONVERTER_SOURCE_DIR}/dependencies/squish/"
)


set(EXECUTABLE_SRCS main.cpp)
set(LIBRARY_SRCS    blp.cpp)
set(LIBRARY_HEADERS blp.h blp_internal.h)


##########################################################################################
# libblp static library

if (WITH_LIBRARY)
    add_library(blp STATIC ${LIBRARY_SRCS} ${LIBRARY_HEADERS})
    target_link_libraries(blp squish)

    set_target_properties(blp PROPERTIES BUILD_WITH_INSTALL_RPATH ON
                                         INSTALL_NAME_DIR "@rpath"
    )

    install(TARGETS blp RUNTIME DESTINATION bin
                        LIBRARY DESTINATION lib
                        ARCHIVE DESTINATION lib)
    install(FILES blp.h DESTINATION include)
endif()


##########################################################################################
# BLPConverter executable

if (WITH_LIBRARY)
    add_executable(BLPConverter ${EXECUTABLE_SRCS})
    target_link_libraries(BLPConverter blp)

    if (APPLE)
        set_target_properties(BLPConverter PROPERTIES LINK_FLAGS "-Wl,-rpath,@loader_path/.")
    else()
        set_target_properties(BLPConverter PROPERTIES INSTALL_RPATH ".")
    endif()
else()
    add_executable(BLPConverter ${EXECUTABLE_SRCS} ${LIBRARY_SRCS} ${LIBRARY_HEADERS})
    target_link_libraries(BLPConverter squish)
endif()

set_target_properties(BLPConverter PROPERTIES COMPILE_DEFINITIONS "_CRT_SECURE_NO_WARNINGS")

install(TARGETS BLPConverter RUNTIME DESTINATION bin)
