set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL "ON")
set(CPACK_NSIS_COMPRESSOR "/SOLID lzma \r\n SetCompressorDictSize 32")
if( CMAKE_CL_64 )
  if(NOT DEFINED CPACK_NSIS_INSTALL_ROOT)
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
  endif()
endif()

if(WIN32 AND NOT UNIX)
  if(NOT DEFINED CPACK_PACKAGE_INSTALL_REGISTRY_KEY)
    set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "IGNIS")
  endif()
endif()
