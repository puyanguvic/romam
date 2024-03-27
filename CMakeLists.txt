check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/open-routing-examples-test-suite.cc
        )
endif()

build_lib(
    LIBNAME open-routing
    SOURCE_FILES model/state-machine.cc
                #  model/ospf-header.cc
#                 model/open-routing.cc
#                 helper/open-routing-helper.cc
    HEADER_FILES model/state-machine.h
                #  model/ospf-header.h
#                 model/open-routing.h    
#                 helper/open-routing-helper.h
    LIBRARIES_TO_LINK ${libcore}
    TEST_SOURCES test/open-routing-test-suite.cc
                 ${examples_as_tests_sources}
)

