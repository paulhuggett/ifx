cmake_minimum_required (VERSION 3.16)
project (ifx CXX)

include (${CMAKE_INSTALL_PREFIX}/lib/pstore/pstore.cmake)

add_executable (ifx ifx.cpp cs_alloc.hpp)
target_link_libraries (ifx PUBLIC pstore::pstore-adt pstore::pstore-mcrepo)

set_target_properties (ifx PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED Yes
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options (ifx PRIVATE
        -Weverything
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-padded
        -Wno-poison-system-directories
        -Wno-unused-lambda-capture
    )
 elseif (CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options (ifx PRIVATE
        -Wall
        -Wextra
        -pedantic
    )
elseif (MSVC)
    target_compile_options (ifx PRIVATE -W4)
endif ()

