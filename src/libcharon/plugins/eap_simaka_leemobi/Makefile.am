AM_CPPFLAGS = \
	-I$(top_srcdir)/src/libstrongswan \
	-I$(top_srcdir)/src/libcharon \
	-I$(top_srcdir)/src/libsimaka \
	-DIPSEC_CONFDIR=\"${sysconfdir}\"

AM_CFLAGS = \
	$(PLUGIN_CFLAGS) \
	$(json_CFLAGS)

if MONOLITHIC
noinst_LTLIBRARIES = libstrongswan-eap-simaka-leemobi.la
libstrongswan_eap_simaka_leemobi_la_LIBADD = $(json_LIBS)
else
plugin_LTLIBRARIES = libstrongswan-eap-simaka-leemobi.la
libstrongswan_eap_simaka_leemobi_la_LIBADD = $(top_builddir)/src/libsimaka/libsimaka.la $(json_LIBS)
endif

libstrongswan_eap_simaka_leemobi_la_SOURCES = \
	eap_simaka_leemobi_plugin.h eap_simaka_leemobi_plugin.c \
	eap_simaka_leemobi_card.h eap_simaka_leemobi_card.c

libstrongswan_eap_simaka_leemobi_la_LDFLAGS = -module -avoid-version
