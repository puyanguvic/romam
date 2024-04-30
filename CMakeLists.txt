set (source_files
    model/datapath/database.cc
    model/datapath/dgr-headers.cc
    model/datapath/global-lsdb-manager.cc
    model/datapath/lsa.cc
    model/datapath/lsdb.cc
    model/datapath/tsdb.cc
    model/datapath/arm-value-db.cc
    # model/datapath/ospf-headers.cc
    # model/datapath/ospf-FSM.cc
    # model/datapath/ospf-headers.cc
    model/datapath/romam-tags.cc
    # model/datapath/state-machine.cc
    
    model/priority_manage/dgr-queue-disc.cc
    model/priority_manage/ddr-queue-disc.cc

    model/routing_algorithm/routing-algorithm.cc
    model/routing_algorithm/route-info-entry.cc
    model/routing_algorithm/route-candidate-queue.cc
    model/routing_algorithm/dijkstra-route-info-entry.cc
    model/routing_algorithm/dijkstra-algorithm.cc
    model/routing_algorithm/spf-algorithm.cc
    model/routing_algorithm/spf-route-info-entry.cc
    
    model/utility/romam-router.cc
    model/utility/route-manager.cc
    model/utility/ospf-router.cc
    model/utility/dgr-router.cc
    model/utility/ddr-router.cc
    # model/utility/octopus-router.cc

    model/romam-routing.cc
    model/ospf-routing.cc
    model/dgr-routing.cc
    model/ddr-routing.cc
    model/octopus-routing.cc

    model/applications/romam-tcp-application.cc
    model/applications/romam-udp-application.cc
    model/applications/romam-sink.cc

    helper/romam-application-helper.cc
    helper/romam-tcp-application-helper.cc
    helper/romam-sink-helper.cc
    helper/romam-routing-helper.cc
    helper/ospf-helper.cc
    helper/dgr-helper.cc
    helper/ddr-helper.cc
    # helper/octopus-helper.cc
)
set (header_files
    model/datapath/database.h
    model/datapath/dgr-headers.h
    model/datapath/global-lsdb-manager.h
    model/datapath/lsa.h
    model/datapath/lsdb.h
    model/datapath/tsdb.h
    model/datapath/arm-value-db.h
    # model/datapath/ospf-headers.h
    # model/datapath/ospf-FSM.h
    # model/datapath/ospf-headers.h
    model/datapath/romam-tags.h
    # model/datapath/state-machine.h

    model/priority_manage/dgr-queue-disc.h
    model/priority_manage/ddr-queue-disc.h
    
    model/routing_algorithm/routing-algorithm.h
    model/routing_algorithm/route-info-entry.h
    model/routing_algorithm/route-candidate-queue.h
    model/routing_algorithm/dijkstra-route-info-entry.h
    model/routing_algorithm/dijkstra-algorithm.h
    model/routing_algorithm/spf-algorithm.h
    model/routing_algorithm/spf-route-info-entry.h

    model/utility/romam-router.h
    model/utility/route-manager.h
    model/utility/ospf-router.h
    model/utility/dgr-router.h
    model/utility/ddr-router.h
    # model/utility/octopus-router.h

    model/romam-routing.h
    model/ospf-routing.h
    model/dgr-routing.h
    model/ddr-routing.h
    model/octopus-routing.h

    model/applications/romam-tcp-application.h
    model/applications/romam-udp-application.h
    model/applications/romam-sink.h

    helper/romam-application-helper.h
    helper/romam-tcp-application-helper.h
    helper/romam-sink-helper.h
    helper/romam-routing-helper.h
    helper/ospf-helper.h
    helper/dgr-helper.h
    helper/ddr-helper.h
    # helper/octopus-helper.cc
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
