#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([ttns-deck], [0.1.16-ttns-remote.2], [https://thethursdaynightshow.com])
AM_INIT_AUTOMAKE([subdir-objects foreign])

AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_OBJCXX
AC_PROG_CC

AC_ARG_VAR([TTNS_DEP_PREFIX], [Optional prefix with third-party libraries (macOS 12 builds)])

if test -n "$TTNS_DEP_PREFIX"; then
  CPPFLAGS="-I${TTNS_DEP_PREFIX}/include $CPPFLAGS"
  LDFLAGS="-L${TTNS_DEP_PREFIX}/lib $LDFLAGS"
else
  if test -d /opt/homebrew/include; then
    CPPFLAGS="$CPPFLAGS -I/opt/homebrew/include"
    LDFLAGS="$LDFLAGS -L/opt/homebrew/lib"
  fi
  if test -d /usr/local/lib; then
    LDFLAGS="$LDFLAGS -L/usr/local/lib"
  fi
fi

# Checks for libraries.
AC_CHECK_LIB([ogg], [ogg_stream_init], [],
		[AC_MSG_ERROR([**** Could not find libogg     ****])]
	    )
AC_CHECK_LIB([vorbis], [vorbis_info_init], [],
		[AC_MSG_ERROR([**** Could not find libvorbis     ****])]
	    )
AC_CHECK_LIB([vorbisenc], [vorbis_encode_init], [],
		[AC_MSG_ERROR([**** Could not find libvorbisenc     ****])]
	    )
AC_CHECK_LIB([vorbisfile], [ov_open], [],
		[AC_MSG_ERROR([**** Could not find libvorbisfile     ****])]
	    )
AC_CHECK_LIB([opus], [opus_encoder_create], [],
		[AC_MSG_ERROR([**** Could not find libopus     ****])]
	    )
AC_CHECK_LIB([mp3lame], [lame_init], [],
		[AC_MSG_ERROR([**** Could not find libmp3lame     ****])]

		)
AC_CHECK_LIB([FLAC], [FLAC__stream_encoder_new], [],
		[AC_MSG_ERROR([**** Could not find libFLAC     ****])]
		)
AC_CHECK_LIB([portaudio], [Pa_Initialize], [],
		[AC_MSG_ERROR([**** Could not find libportaudio     ****])]
	    )
AC_CHECK_LIB([pthread], [pthread_create], [],
		[AC_MSG_ERROR([**** Could not find libpthread     ****])]
	    )
AC_CHECK_LIB([samplerate], [src_new], [],
		[AC_MSG_ERROR([**** Could not find samplerate     ****])]
	    )

AC_CHECK_LIB([fdk-aac], [aacEncOpen], [],
		[AC_MSG_ERROR([**** Could not find libfdk-aac     ****])]
	    )

# libcurl with WebSocket support (WAN dial-in via WRX).
# Homebrew curl is skipped when TTNS_DEP_PREFIX is set so macOS 12
# builds do not pick up bottles compiled for the host OS.
if test -z "$TTNS_DEP_PREFIX"; then
  if test -d /opt/homebrew/opt/curl; then
    CPPFLAGS="-I/opt/homebrew/opt/curl/include $CPPFLAGS"
    LDFLAGS="-L/opt/homebrew/opt/curl/lib $LDFLAGS"
  elif test -d /usr/local/opt/curl; then
    CPPFLAGS="-I/usr/local/opt/curl/include $CPPFLAGS"
    LDFLAGS="-L/usr/local/opt/curl/lib $LDFLAGS"
  fi
fi
AC_CHECK_LIB([curl], [curl_ws_send], [],
		[AC_CHECK_LIB([curl], [curl_easy_init],
		  [AC_MSG_WARN([libcurl found but without WebSocket API — WAN dial-in disabled at runtime])],
		  [AC_MSG_ERROR([**** Could not find libcurl ****])])]
	    )

AC_CHECK_HEADER([winsock2.h], 
		[
		LIBS="$LIBS -lws2_32"
		host_os="windows"
		],
	       )
AM_CONDITIONAL(WINDOWS, test "$host_os" = "windows")

AC_CHECK_PROG([FLTKCONFIG],[fltk-config], [fltk-config])

if test "$FLTKCONFIG" = "fltk-config"; then
    AC_CHECK_LIB([fltk], [main], 
		    [
		    if test "$host_os" = "windows"; then
			# fltk-config --use-images emits /mingw64/lib/*.a paths that
			# MinGW ld cannot resolve on GitHub Actions; -l* import libs work.
			LIBS="$LIBS `fltk-config --ldflags` -lfltk_images -ljpeg -lpng -lz"
		    else
			LIBS="$LIBS `fltk-config --ldflags --use-images`"
		    fi
		    CXXFLAGS="$CXXFLAGS `fltk-config --cxxflags`"
		    ],
		    [AC_MSG_ERROR([**** Could not find libfltk     ****])]
		)
else
  AC_MSG_ERROR([**** Could not find fltk-config     ****])
fi



# Checks for header files.
AC_PATH_X
AC_CHECK_HEADERS([fcntl.h limits.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gethostbyaddr gethostbyname memset pow select socket strchr strdup strpbrk strrchr strstr])

AM_CONDITIONAL([DARWIN], [test "x$(uname -s)" = xDarwin])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
