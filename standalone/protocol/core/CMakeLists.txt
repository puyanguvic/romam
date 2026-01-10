add_library(romam_core
  src/config.cpp
  src/ip.cpp
  src/lsdb.cpp
  src/routing.cpp
  src/spf.cpp
  src/wire.cpp
)

target_include_directories(romam_core PUBLIC include)
target_link_libraries(romam_core PUBLIC romam_warnings)
