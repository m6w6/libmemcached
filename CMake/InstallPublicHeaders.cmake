function(install_public_headers DIRECTORY)
    # validate current directory
    string(FIND ${CMAKE_CURRENT_SOURCE_DIR} /include/ INCDIR REVERSE)
    string(FIND ${CMAKE_CURRENT_SOURCE_DIR} /src/ SRCDIR REVERSE)
    if((INCDIR GREATER_EQUAL 0) OR (SRCDIR GREATER_EQUAL 0))
        if(INCDIR GREATER_EQUAL 0)
            math(EXPR POSITION "${INCDIR} + 9")
        else()
            math(EXPR POSITION "${SRCDIR} + 5")
        endif()
        string(SUBSTRING ${CMAKE_CURRENT_SOURCE_DIR} ${POSITION} -1 CHKDIR)
        if(NOT CHKDIR STREQUAL DIRECTORY)
            message(SEND_ERROR "install_public_headers() directories do not match: ${CHKDIR} != ${DIRECTORY}")
            set(ENV{INVALID_CONFIGURATION} 1)
        endif()
    endif()

    # validate public interface version
    get_filename_component(TOP ${DIRECTORY} NAME)
    string(FIND ${TOP} "-" DASH)
    if(DASH GREATER 0)
        string(SUBSTRING ${TOP} 0 ${DASH} LIBRARY)
        string(TOUPPER ${LIBRARY} LIBRARY_UCASE)
        math(EXPR DASH "${DASH} + 1")
        string(SUBSTRING ${TOP} ${DASH} -1 VERSION)
        if(NOT ${LIBRARY_UCASE}_VERSION_MAJOR VERSION_EQUAL ${VERSION})
            message(SEND_ERROR "${LIBRARY} public include directory version ${VERSION} != " ${${LIBRARY_UCASE}_VERSION_MAJOR})
            set(ENV{INVALID_CONFIGURATION} 1)
        endif()
    endif()

    # change local includes to system includes
    foreach(HEADER IN LISTS ARGN)
        if(HEADER MATCHES "^@")
            string(SUBSTRING ${HEADER} 1 -1 HEADER)
            configure_file(${HEADER}.in ${HEADER})
            set(HEADER "${CMAKE_CURRENT_BINARY_DIR}/${HEADER}")
        endif()
        install(FILES ${HEADER}
                COMPONENT dev
                DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${DIRECTORY}
                )
    endforeach()
endfunction()