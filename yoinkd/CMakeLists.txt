
find_library(PCRE2_LIBRARY pcre2-8 REQUIRED)
find_path(PCRE2_INCLUDE_DIR pcre2.h REQUIRED)


file(GLOB_RECURSE sources "${CMAKE_CURRENT_SOURCE_DIR}/src/*.[ch]pp")

add_executable(yoinkd ${sources})
target_link_libraries(yoinkd PUBLIC "${PCRE2_LIBRARY}" yaml-cpp::yaml-cpp PRIVATE Boost::asio Boost::json Boost::process CLI11::CLI11)
target_include_directories(yoinkd PRIVATE "${PCRE2_INCLUDE_DIR}" "${JOURNALD_INCLUDE_DIR}")
target_compile_definitions(yoinkd PRIVATE PCRE2_CODE_UNIT_WIDTH=8)
target_compile_features(yoinkd PRIVATE cxx_std_23)
