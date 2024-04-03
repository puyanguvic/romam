set (source_files
    model/datapath/headers-dgr.cc
    model/datapath/database.cc
    model/datapath/tsdb.cc
    # model/datapath/dgr-router-interface.cc
    model/datapath/romam-tags.cc
    model/datapath/lsa.cc
    model/utility/romam-router.cc
    model/routing_algorithm/ipv4-dgr-routing-table-entry.cc
    model/ipv4-dgr-routing.cc
    model/routing_algorithm/dgr-route-manager.cc
    model/routing_algorithm/route-manager-impl.cc
    model/routing_algorithm/dgr-candidate-queue.cc
    model/priority_manage/dgrv2-queue-disc.cc
    
    # model/datapath
    # model/datapath
    # model/datapath
    # model/datapath/database.cc
    # model/datapath/lsa.cc
    # model/datapath/lsdb.cc
    # model/datapath/tsdb.cc
    # model/datapath/dgr-header.cc
    # model/datapath/dgr-router-interface.cc
    # model/datapath/neighbor-status-database.cc
    # model/datapath/ospf-header.cc
    # model/priority_manage/dgrv2-queue-disc.cc
    # model/routing_algorithm/route-info-entry.cc
    # model/routing_algorithm/ipv4-route-info-entry.cc
    # model/routing_algorithm/dgr-candidate-queue.cc
    # model/routing_algorithm/dgr-route-manager-impl.cc    
    # model/routing_algorithm/dgr-route-manager.cc
    # model/routing_algorithm/ipv4-dgr-routing-table-entry.cc
    # model/dgr-tags.cc
    # model/ipv4-dgr-routing.cc

)
set (header_files
    model/datapath/headers-dgr.h
    model/datapath/database.h
    model/datapath/tsdb.h
    # model/datapath/dgr-router-interface.h
    model/datapath/romam-tags.h
    model/datapath/lsa.h
    model/utility/romam-router.h
    model/routing_algorithm/ipv4-dgr-routing-table-entry.h
    model/ipv4-dgr-routing.h
    model/routing_algorithm/dgr-route-manager.h
    model/routing_algorithm/route-manager-impl.h
    model/routing_algorithm/dgr-candidate-queue.h
    model/priority_manage/dgrv2-queue-disc.h

    # model/routing_algorithm/route-info-entry.h
    # model/routing_algorithm/ipv4-route-info-entry.h
    # model/datapath/database.h
    # model/datapath/lsa.h
    # model/datapath/lsdb.h
    # model/datapath/tsdb.h
    # model/datapath/dgr-header.h
    # model/datapath/dgr-router-interface.h
    # model/datapath/neighbor-status-database.h
    # model/datapath/ospf-header.h
    # model/priority_manage/dgrv2-queue-disc.h
    # model/routing_algorithm/dgr-candidate-queue.h
    # model/routing_algorithm/dgr-route-manager-impl.h
    # model/routing_algorithm/dgr-route-manager.h
    # model/routing_algorithm/ipv4-dgr-routing-table-entry.h
    # model/dgr-tags.h
    # model/ipv4-dgr-routing.h
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
