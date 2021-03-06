
macro(configure_init CONFIG_HEADER_FILE)
    set(CONFIGURE_FILE_IN ${CONFIG_HEADER_FILE}.in)
    file(WRITE ${CONFIGURE_FILE_IN} "#pragma once\n")
    set(CONFIGURE_FILE_OUT ${CONFIG_HEADER_FILE})
endmacro()

macro(configure_append)
    file(APPEND ${CONFIGURE_FILE_IN} ${ARGN})
endmacro()

macro(configure_set VAR VAL)
    set(${VAR} ${VAL})
    configure_append("#cmakedefine ${VAR} 1\n")
endmacro()

macro(configure_define VAR)
    configure_append("#cmakedefine ${VAR} 1\n")
endmacro()
macro(configure_undef VAR)
    configure_append("#undef ${VAR}\n")
endmacro()

macro(configure_define_01 VAR)
    configure_append("#cmakedefine01 ${VAR}\n")
endmacro()

macro(configure_define_literal VAR)
    string(TOUPPER ${VAR} UPPER)
    configure_append("#define ${UPPER} @${VAR}@\n")
endmacro()
macro(configure_define_header VAR)
    string(TOUPPER ${VAR} UPPER)
    configure_append("#define ${UPPER} <@${VAR}@>\n")
endmacro()
macro(configure_define_string VAR)
    string(TOUPPER ${VAR} UPPER)
    configure_append("#define ${UPPER} \"@${VAR}@\"\n")
endmacro()
