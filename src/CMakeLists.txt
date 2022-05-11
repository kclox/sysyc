include_directories(include)

add_subdirectory(comm)
add_subdirectory(ir)
add_subdirectory(frontend)
add_subdirectory(opt)
add_subdirectory(backend)

add_executable(sysycc main.cc)
target_link_libraries(sysycc frontend opt backend)
