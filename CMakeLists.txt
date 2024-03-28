check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/romam-examples-test-suite.cc
        )
endif()

build_lib(
    LIBNAME romam
    SOURCE_FILES model/routing_algorithm/route-info-entry.cc
                    # model/romam.cc
                #  helper/romam-helper.cc
    HEADER_FILES model/routing_algorithm/route-info-entry.h
                #  model/romam.h
                #  helper/romam-helper.h
    LIBRARIES_TO_LINK ${libcore}
    TEST_SOURCES test/romam-test-suite.cc
                 ${examples_as_tests_sources}
)

