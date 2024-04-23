set (source_files
    model/datapath/database.cc
    # model/datapath/dgr-headers.cc
    model/datapath/global-lsdb-manager.cc
    model/datapath/lsa.cc
    model/datapath/lsdb.cc
    model/datapath/tsdb.cc
    # model/datapath/ospf-headers.cc
    # model/datapath/ospf-FSM.cc
    # model/datapath/ospf-headers.cc
    # model/datapath/romam-tags.cc
    # model/datapath/state-machine.cc
    
    # model/priority_manage/ddr-queue-disc.cc

    model/routing_algorithm/routing-algorithm.cc
    model/routing_algorithm/route-info-entry.cc
    model/routing_algorithm/route-candidate-queue.cc
    model/routing_algorithm/dijkstra-route-info-entry.cc
    model/routing_algorithm/dijkstra-algorithm.cc
    # model/routing_algorithm/spf-algorithm.cc
    # model/routing_algorithm/spf-route-info-entry.cc
    
    model/utility/romam-router.cc
    model/utility/ospf-router.cc
    model/utility/route-manager.cc

    model/romam-routing.cc
    model/ospf-in-romam-routing.cc
    helper/ospf-in-romam-helper.cc
    helper/romam-routing-helper.cc
)
set (header_files
    model/datapath/database.h
    # model/datapath/dgr-headers.h
    model/datapath/global-lsdb-manager.h
    model/datapath/lsa.h
    model/datapath/lsdb.h
    model/datapath/tsdb.h
    # model/datapath/ospf-headers.h
    # model/datapath/ospf-FSM.h
    # model/datapath/ospf-headers.h
    # model/datapath/romam-tags.h
    # model/datapath/state-machine.h

    # model/priority_manage/ddr-queue-disc.h

    model/routing_algorithm/routing-algorithm.h
    model/routing_algorithm/route-info-entry.h
    model/routing_algorithm/route-candidate-queue.h
    model/routing_algorithm/dijkstra-route-info-entry.h
    model/routing_algorithm/dijkstra-algorithm.h
    # model/routing_algorithm/spf-algorithm.h
    # model/routing_algorithm/spf-route-info-entry.h

    model/utility/romam-router.h
    model/utility/ospf-router.h
    model/utility/route-manager.h

    model/romam-routing.h
    model/ospf-in-romam-routing.h
    helper/ospf-in-romam-helper.h
    helper/romam-routing-helper.h
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
