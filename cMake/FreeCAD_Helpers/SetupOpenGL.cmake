macro(SetupOpenGL)
# -------------------------------- OpenGL --------------------------------

    set(OpenGL_GL_PREFERENCE GLVND)

    # On Linux, remove GLX support. There is *zero* reason to keep supporting it,
    # as it has been available even on X11 as far back as 2013, and offers strictly
    # no advantage over EGL considering we do not support indirect rendering contexts.
    # EGL provides proper VRAM accounting (due to the DRI contexts being owned by the
    # calling proces and not the X server).
    if(LINUX)
        set(OPENGL_USE_EGL TRUE CACHE BOOL "Remove GLX support" FORCE)
        set(OPENGL_USE_GLX FALSE CACHE BOOL "Remove GLX support" FORCE)
    endif()

    find_package(OpenGL)
    include(FindPackageMessage)
    if(OPENGL_GLU_FOUND)
        find_package_message(OPENGL_GLU
            "Found OpenGLU: ${OPENGL_glu_LIBRARY}"
            "[${OPENGL_glu_LIBRARY}][${OPENGL_INCLUDE_DIR}]")
    else(OPENGL_GLU_FOUND)
        message(FATAL_ERROR "======================\n"
                            "GLU library not found.\n"
                            "======================\n")
    endif(OPENGL_GLU_FOUND)

    if(LINUX)
        set(OpenGL_GLX_FOUND FALSE CACHE BOOL "Remove GLX support" FORCE)
        set(OPENGL_glx_LIBRARY "" CACHE PATH "Remove GLX support" FORCE)
        alter_opengl_linkage(
            TARGETS GL OpenGL::GL
            REMOVE GLX
            KEEP GL EGL
        )
    endif()

endmacro(SetupOpenGL)
