

add_library(opt
  optmgr.cc
  constant.cc
  build_dtree.cc
  mem2reg.cc
  init_bb_ptr.cc
  gvn.cc
  zero_one_elimination.cc
)


add_executable(opt_tool
  opt_tool.cc
)
# target_include_directories(opt_tool BEFORE PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../ir/)
target_link_libraries(opt_tool opt ir_parser)
