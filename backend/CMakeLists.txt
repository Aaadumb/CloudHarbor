cmake_minimum_required(VERSION 3.20)
project(CloudHarbor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Drogon CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

file(GLOB_RECURSE CLOUDHARBOR_SOURCES CONFIGURE_DEPENDS "src/*.cc")
add_executable(cloudharbor ${CLOUDHARBOR_SOURCES})
target_compile_features(cloudharbor PRIVATE cxx_std_20)

target_include_directories(cloudharbor PRIVATE 
    include
    include/cloudharbor/common
    include/cloudharbor/http
    include/cloudharbor/infrastructure/mail
    include/cloudharbor/infrastructure/security
    include/cloudharbor/infrastructure/storage
    include/cloudharbor/infrastructure/threadpool
    include/cloudharbor/modules/files
    include/cloudharbor/modules/account
)
target_link_libraries(cloudharbor PRIVATE
    Drogon::Drogon
    CURL::libcurl
    PkgConfig::SODIUM
    Threads::Threads
)

set_target_properties(cloudharbor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)