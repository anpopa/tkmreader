set(CPACK_PACKAGE_NAME "tkmreader")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "TaskMonitor reader application")
set(CPACK_VERBATIM_VARIABLES YES)

set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
SET(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/packages")

set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

set(CPACK_PACKAGE_VENDOR "Alin Popa")
set(CPACK_PACKAGE_CONTACT "alin.popa@fxdata.ro")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# RPM
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_DEBUGINFO_PACKAGE OFF)
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_CHANGELOG_FILE "${CMAKE_SOURCE_DIR}/CHANGELOG")
set(CPACK_RPM_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION_SUMMARY})
set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
set(CPACK_RPM_PACKAGE_REQUIRES
  "sqlite >= 3.31, jsoncpp >= 1.9.4, protobuf >= 3.6.1")

# DEB
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_DEBUGINFO_PACKAGE OFF)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Alin Popa")
set(CPACK_DEBIAN_PACKAGE_SECTION "Utilities")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
  "libjsoncpp24 (>= 1.9.4), libsqlite3-0 (>= 3.31.1), libprotobuf17 (>=3.6.1)")

include(CPack)
