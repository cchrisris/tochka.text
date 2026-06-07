cmake_minimum_required(VERSION 3.16)
project(tochka_backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Boost REQUIRED COMPONENTS json)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

add_executable(tochka_backend
    src/main.cpp
    src/config.cpp
    src/db.cpp
    src/router.cpp
    src/http_server.cpp
    src/auth.cpp
    src/handlers_auth.cpp
    src/handlers_works.cpp
    src/handlers_comments.cpp
    src/handlers_collections.cpp
    src/handlers_users.cpp
    src/handlers_admin.cpp
    src/handlers_uploads.cpp
)

target_include_directories(tochka_backend PRIVATE src)

target_link_libraries(tochka_backend PRIVATE
    Boost::json
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    PkgConfig::PQXX
    PkgConfig::SODIUM
)
