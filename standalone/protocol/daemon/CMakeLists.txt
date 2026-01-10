add_executable(romamd
  src/linux_netlink.cpp
  src/main.cpp
  src/multicast.cpp
)

target_link_libraries(romamd PRIVATE romam_core romam_warnings)

find_package(Threads REQUIRED)
target_link_libraries(romamd PRIVATE Threads::Threads)
