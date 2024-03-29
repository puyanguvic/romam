set (source_files
    model/routing_algorithm/route-info-entry.cc
    model/routing_algorithm/ipv4-route-info-entry.cc
    model/datapath/database.cc
    model/datapath/lsa.cc
    model/datapath/lsdb.cc
    model/datapath/tsdb.cc
)
set (header_files
    model/routing_algorithm/route-info-entry.h
    model/routing_algorithm/ipv4-route-info-entry.h
    model/datapath/database.h
    model/datapath/lsa.h
    model/datapath/lsdb.h
    model/datapath/tsdb.h

)

build_lib(
    LIBNAME romam
    SOURCE_FILES ${source_files}
    HEADER_FILES ${header_files}
    LIBRARIES_TO_LINK
        ${libnetwork}
        ${libcore}
        ${libbridge}
        ${libtraffic-control}
    TEST_SOURCES ${test_sources}
)
