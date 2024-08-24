# List the source files
set(SRCS alpha.cpp
         clusterfit.cpp
         colourblock.cpp
         colourfit.cpp
         colourset.cpp
         maths.cpp
         rangefit.cpp
         singlecolourfit.cpp
         squish.cpp
)

# List the include paths
include_directories(.)

# Declaration of the library
add_library(squish STATIC ${SRCS})

# Compilation settings
if (NOT WIN32)
    target_compile_options(squish PRIVATE -w -fPIC)
else()
    target_compile_options(squish PRIVATE /W0)
endif()
