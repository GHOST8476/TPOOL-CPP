cmake_minimum_required(VERSION 3.14)

# Download GTest
include(FetchContent)
FetchContent_Declare (
    googletest
    # Specify the commit you depend on and update it regularly.
    URL https://github.com/google/googletest/archive/e2239ee6043f73722e7aa812a459f54a28552929.zip
)

# We dont need to install google test
set(install_gtest OFF CACHE BOOL "" FORCE)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# -----------------------------------------------------------------------------
# Create tests
# -----------------------------------------------------------------------------

function(CreateTest TestName)
    add_executable(${TestName} ${TestName}.cpp)
    target_link_libraries(${TestName} gtest_main)
    target_include_directories(${TestName} PRIVATE "../include")
    add_test(NAME ${TestName} COMMAND ${TestName})
endfunction(CreateTest)

# Fixed Thread Pool Tests
CreateTest(FixedThreadPool)
CreateTest(FixedThreadPoolMovable)

# Worker Tests
CreateTest(Worker)
CreateTest(WorkerMovable)

# Dynamic Thread Pool Tests
CreateTest(DynamicThreadPool)
CreateTest(DynamicThreadPoolMovable)