macro(SetupGSL)
# -------------------------------- SetupGSL --------------------------------

    if (EXISTS "${CMAKE_SOURCE_DIR}/src/3rdParty/GSL/include")
        add_library(Microsoft.GSL::GSL INTERFACE IMPORTED)
        set_target_properties(Microsoft.GSL::GSL PROPERTIES
            INTERFACE_COMPILE_FEATURES "cxx_std_20"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/src/3rdParty/GSL/include"
        )
    else()
        find_package(Microsoft.GSL)
        if (Microsoft.GSL_FOUND)
            message(STATUS "Found Microsoft.GSL: version ${Microsoft.GSL_VERSION}")
        else()
            message(SEND_ERROR "The C++ Guidelines Support Library (GSL) submodule is not available. Please run git submodule update --init")
        endif()
    endif()

endmacro(SetupGSL)
