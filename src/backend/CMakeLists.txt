
add_library(backend 
  backend.cc
  instr_select.cc
  reg_alloca.cc
)
target_link_libraries(backend dbg)

include(GoogleTest)

add_executable(backend_test backend_test.cc)
target_link_libraries(backend_test gtest_main backend)
gtest_discover_tests(backend_test)
