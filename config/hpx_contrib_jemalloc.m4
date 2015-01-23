# -*- autoconf -*---------------------------------------------------------------
# HPX_CONTRIB_JEMALLOC([path],[prefix],[suffix])
# ------------------------------------------------------------------------------
AC_DEFUN([HPX_CONTRIB_JEMALLOC],
  [ACX_CONFIGURE_DIR([$1], [$1$3],
    ["--disable-valgrind --disable-fill --disable-stats --with-jemalloc-prefix=$2 --without-mangling --with-install-suffix=$3 --with-private-namespace=$2"])
   AC_SUBST(HPX_JEMALLOC_CPPFLAGS, "-I\$(top_builddir)/$1$3/include")
   AC_SUBST(HPX_JEMALLOC_LDFLAGS, "-L\$(top_builddir)/$1$3/lib/")
   AC_SUBST(HPX_JEMALLOC_LIBS, "-ljemalloc$3")
   HPX_JEMALLOC_LDADD="\$(top_builddir)/$1$3/lib/libjemalloc$3.a"])
