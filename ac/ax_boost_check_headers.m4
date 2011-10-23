AC_DEFUN([AX_BOOST_CHECK_HEADERS],[
  CPPFLAGS_SAVED="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"
  export CPPFLAGS

  LDFLAGS_SAVED="$LDFLAGS"
  LDFLAGS="$LDFLAGS $BOOST_LDFLAGS"
  export LDFLAGS

  AC_LANG_PUSH(C++)
  AC_CHECK_HEADERS([$1],[$2],[$3],[$4])
  AC_LANG_POP()

  CPPFLAGS="$CPPFLAGS_SAVED"
  LDFLAGS="$LDFLAGS_SAVED"
])
