# what flags you want to pass to the C compiler & linker

AM_CPPFLAGS = -IFLTK

#
# # this lists the binaries to produce, the (non-PHONY, binary) targets in
# # the previous manual Makefile
bin_PROGRAMS = butt 
if DARWIN
butt_CXXFLAGS = -x objective-c++
endif
butt_SOURCES = butt.cpp butt.h cfg.cpp cfg.h icecast.cpp icecast.h lame_encode.cpp \
			   lame_encode.h parseconfig.cpp parseconfig.h port_audio.cpp \
			   port_audio.h ringbuffer.cpp ringbuffer.h shoutcast.cpp shoutcast.h \
			   sockfuncs.cpp sockfuncs.h strfuncs.cpp strfuncs.h timer.cpp timer.h \
			   util.cpp util.h vorbis_encode.cpp vorbis_encode.h vu_meter.cpp vu_meter.h \
			   wav_header.cpp wav_header.h opus_encode.cpp opus_encode.h flac_encode.cpp flac_encode.h \
			   FLTK/Fl_ILM216.cpp FLTK/Fl_ILM216.h FLTK/fl_callbacks.cpp \
			   FLTK/fl_callbacks.h FLTK/fl_funcs.cpp FLTK/fl_funcs.h FLTK/flgui.cpp FLTK/flgui.h \
			   FLTK/FL/Fl_My_Double_Window.H FLTK/FL/Fl_My_Value_Slider.H \
			   FLTK/FL/Fl_Ttns_Cart_Button.H FLTK/Fl_Ttns_Cart_Button.cpp \
			   FLTK/FL/Fl_Ttns_Fader.H FLTK/Fl_Ttns_Fader.cpp \
			   FLTK/FL/Fl_Ttns_Transport_Button.H FLTK/Fl_Ttns_Transport_Button.cpp \
			   FLTK/FL/Fl_Ttns_Border_Button.H FLTK/Fl_Ttns_Border_Button.cpp \
			   FLTK/FL/Fl_Ttns_Check_Button.H FLTK/Fl_Ttns_Check_Button.cpp \
			   FLTK/FL/Fl_Ttns_Mic_Button.H FLTK/Fl_Ttns_Mic_Button.cpp \
			   FLTK/Fl_My_Native_File_Chooser.cxx FLTK/FL/Fl_My_Native_File_Chooser.H \
			   FLTK/fl_timer_funcs.cpp FLTK/fl_timer_funcs.h aac_encode.cpp aac_encode.h \
			   ttns_audio.cpp ttns_audio.h ttns_ui.cpp ttns_ui.h ttns_zones.cpp ttns_zones.h \
			   cart_player.cpp cart_player.h cart_loader.cpp cart_loader.h \
			   ttns_theme.cpp ttns_theme.h \
			   ttns_paths.cpp ttns_paths.h ttns_about.cpp ttns_about.h

ttnsdeckdir = $(datadir)/ttns-deck

install-data-local:
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 755 butt $(DESTDIR)$(bindir)/ttns-deck
	$(INSTALL) -d $(DESTDIR)$(ttnsdeckdir)/data $(DESTDIR)$(ttnsdeckdir)/assets
	$(INSTALL) -m 644 $(top_srcdir)/data/ttns-zones.json $(DESTDIR)$(ttnsdeckdir)/data/
	$(INSTALL) -m 644 $(top_srcdir)/assets/ttns-logo.png $(DESTDIR)$(ttnsdeckdir)/assets/
	$(INSTALL) -m 644 $(top_srcdir)/assets/ttns-deck.desktop $(DESTDIR)$(ttnsdeckdir)/ 2>/dev/null || true

if DARWIN
butt_SOURCES += FLTK/Fl_My_Native_File_Chooser_MAC.mm
endif

if WINDOWS
butt_SOURCES += resource.rc
endif

# used only under MinGW to compile the resource.rc file (manifest and program icon)

.rc.o:
	windres $^ -o $@
%.o : %.rc
	windres $^ -o $@
