AC_DEFUN([GP_CHECK_BLITZMAX],
[
    GP_ARG_DISABLE(Blitzmax, yes)
    GP_STATUS_PLUGIN_ADD([Blitzmax], [$enable_blitzmax])
    AC_CONFIG_FILES([
        blitzmax/Makefile
        blitzmax/src/Makefile
    ])
])
