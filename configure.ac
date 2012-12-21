AC_PREREQ([2.61])
dnl Don't forget when updating version number to do GP_CHECK_GEANY also
AC_INIT([blitzmax-geany-plugin], [1.23])
AM_INIT_AUTOMAKE([1.8 foreign dist-bzip2 tar-ustar])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([build/cache])

AC_PROG_CC
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

dnl i18n
IT_PROG_INTLTOOL([0.35.0])
GP_I18N

dnl common checks
GP_CHECK_GEANY(1.23)
GP_CHECK_UNITTESTS(0.9.4)
GP_CHECK_CPPCHECK
GP_CHECK_CFLAGS

dnl plugin checks
GP_CHECK_BLITZMAX

AC_CONFIG_FILES([
    Makefile
    po/Makefile.in
])
AC_OUTPUT
GP_STATUS_PRINT
