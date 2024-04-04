set (source_files
    model/datapath/database.cc
    model/datapath/dgr-headers.cc
    model/datapath/global-lsdb-manager.cc
    model/datapath/lsa.cc
    model/datapath/lsdb.cc
    # model/datapath/ospf-headers.cc
    # model/datapath/ospf-FSM.cc
    # model/datapath/ospf-headers.cc
    model/datapath/romam-tags.cc
    # model/datapath/state-machine.cc
    model/datapath/tsdb.cc
    
    model/priority_manage/ddr-queue-disc.cc
    model/routing_algorithm/ipv4-dgr-routing-table-entry.cc
    model/routing_algorithm/ipv4-route-info-entry.cc
    model/routing_algorithm/route-candidate-queue.cc
    model/routing_algorithm/route-info-entry.cc
    # model/routing_algorithm/route-manager-impl.cc    
    
    model/utility/romam-router.cc
    model/utility/router-manager.cc

    model/romam-routing.cc
)
set (header_files
    model/datapath/database.cc
    model/datapath/dgr-headers.cc
    model/datapath/global-lsdb-manager.cc
    model/datapath/lsa.cc
    model/datapath/lsdb.cc
    # model/datapath/ospf-headers.cc
    # model/datapath/ospf-FSM.cc
    # model/datapath/ospf-headers.cc
    model/datapath/romam-tags.cc
    # model/datapath/state-machine.cc
    model/datapath/tsdb.cc

    model/priority_manage/ddr-queue-disc.cc
    model/routing_algorithm/ipv4-dgr-routing-table-entry.cc
    model/routing_algorithm/ipv4-route-info-entry.cc
    model/routing_algorithm/route-candidate-queue.cc
    model/routing_algorithm/route-info-entry.cc
    # model/routing_algorithm/route-manager-impl.cc    

    model/utility/romam-router.cc
    model/utility/router-manager.cc

    model/romam-routing.cc
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
