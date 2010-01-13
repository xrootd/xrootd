

noinst_LTLIBRARIES = libsslGridSite.la
INCLUDES = ${OPENSSL_INCDIR} -I/usr/kerberos/include -I. ${XML2_INCDIR}

libsslGridSite_la_CFLAGS = -Wall -Werror -Wno-deprecated-declarations -Wno-deprecated-declarations ${CFLAGS}

libsslGridSite_la_SOURCES = \
	grst_err.c  \
	grst_asn1.c \
	grst_x509.c \
	grst_gacl.c \
	grst_xacml.c \
	grst_http.c \
	grst_verifycallback.c \
	grst_verifycallback.h \
	gridsite.h






