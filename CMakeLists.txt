include_directories( ${CPPUNIT_INCLUDE_DIRS} )

#add_library (eosNamespaceTests SHARED
#      ChangeLogContainerMDSvcTest.cc
#      ChangeLogFileMDSvcTest.cc
#      ChangeLogTest.cc
#      FileSystemViewTest.cc
#      HierarchicalViewTest.cc
#      LogCompactingTest.cc
#      OtherTests.cc
#      TestHelpers.hh
#      TestHelpers.cc
#)

#target_link_libraries( eosNamespaceTests ${CPPUNIT_LIBRARIES} eosNamespace)

add_executable( text-runner TextRunner.cc PathProcessor.hh )
target_link_libraries( text-runner dl cppunit)
