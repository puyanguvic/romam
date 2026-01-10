add_executable(romam-traffic
  src/romam_traffic.cpp
)

target_link_libraries(romam-traffic PRIVATE romam_warnings)

