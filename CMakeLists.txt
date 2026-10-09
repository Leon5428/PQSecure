cmake_minimum_required(VERSION 3.20)
project(PQSecure LANGUAGES CXX)

find_package(liboqs CONFIG REQUIRED)

add_executable(ml_kem_768_demo experiments/ml_kem_768_demo.cpp)
target_compile_features(ml_kem_768_demo PRIVATE cxx_std_17)
target_link_libraries(ml_kem_768_demo PRIVATE OQS::oqs)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME ml_kem_768_roundtrip_and_tampering COMMAND ml_kem_768_demo)
endif()
