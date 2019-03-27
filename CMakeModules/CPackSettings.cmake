set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Simply re-written new PEIS")
set(CPACK_PACKAGE_VENDOR "Chittaranjan Swaminathan")
set(CPACK_PACKAGE_CONTACT "Chittaranjan Swaminathan <chitt@aass.oru.se>")

set(CPACK_COMPONENTS_ALL srnp)
set(CPACK_COMPONENT_SRNP_DISPLAY_NAME "SRNP Library")
set(CPACK_COMPONENT_SRNP_REQUIRED ON)

set(CPACK_SOURCE_IGNORE_FILES
    "/.hg"
    "/.vscode"
    "/build/"
    ".pyc$"
    ".pyo$"
    "__pycache__"
    ".so$"
    ".dylib$"
    ".orig$"
    ".log$"
    ".DS_Store"
    "/html/"
    "/bindings/"
    "TODO"
    ".registered$"
    "binding_generator.py$")
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_GENERATOR "TGZ")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(CPACK_GENERATOR "DEB;${CPACK_GENERATOR}")
    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "i686")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
    endif()
    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    endif()
    execute_process(COMMAND "/usr/bin/lsb_release" "-rs"
        OUTPUT_VARIABLE UBUNTU_RELEASE
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}_${PROJECT_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}-Ubuntu${UBUNTU_RELEASE}")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "python${PYTHON_VERSION}, libboost-serialization-dev, libboost-filesystem-dev, libboost-system-dev, libboost-program-options-dev, libboost-test-dev")
endif()

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
endif()

include(CPack)
