#
# Makefile - build eeshow
#
# Written 2016 by Werner Almesberger
# Copyright 2016 by Werner Almesberger
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#

SHELL = /bin/bash

NAME = eeshow
OBJS = main.o version.o \
       main/common.o \
       kicad/sch-parse.o kicad/sch-render.o kicad/lib-parse.o \
       kicad/lib-render.o kicad/dwg.o kicad/delta.o kicad/sexpr.o \
       kicad/pl-parse.o kicad/pl-render.o kicad/ext.o kicad/pro.o \
       gui/gui.o gui/over.o gui/style.o gui/aoi.o gui/fmt-pango.o gui/input.o \
       gui/progress.o gui/glabel.o gui/sheet.o gui/history.o gui/render.o \
       gui/help.o gui/icons.o gui/index.o gui/timer.o \
       file/file.o file/git-util.o file/git-file.o file/git-hist.o \
       gfx/style.o gfx/fig.o gfx/record.o gfx/cro.o gfx/diff.o gfx/gfx.o \
       gfx/text.o gfx/misc.o gfx/pdftoc.o \
       misc/diag.o misc/util.o
EETEST_OBJS = main/eetest.o main/common.o version.o \
       kicad/sexpr.o \
       gui/fmt-pango.o \
       file/file.o file/git-util.o file/git-file.o file/git-hist.o \
       misc/diag.o misc/util.o

ICONS = delta diff

CFLAGS = -g  -Wall -Wextra -Wno-unused-parameter -Wshadow \
	 -Wmissing-prototypes -Wmissing-declarations \
	 -I. \
	 `pkg-config --cflags cairo` \
	 `pkg-config --cflags libgit2` \
	 `pkg-config --cflags gtk+-3.0`
LDLIBS = -lm \
	 `pkg-config --libs cairo` \
	 `pkg-config --libs libgit2` \
	 `pkg-config --libs gtk+-3.0`

GIT_VERSION = $(shell git log -1 --format='%h' -s .)
GIT_STATUS = $(shell [ -z "`git status -s -uno`" ] || echo +)
BUILD_DATE = $(shell date +'%Y%m%d-%H:%M')

CFLAGS += -DVERSION='"$(GIT_VERSION)$(GIT_STATUS)"' \
	  -DBUILD_DATE='"$(BUILD_DATE)"'

ifneq ($(USE_WEBKIT),)
	CFLAGS += -DUSE_WEBKIT `pkg-config --cflags webkit2gtk-4.0`
	LDLIBS += `pkg-config --libs webkit2gtk-4.0`
	HELP_TEXT = help.html
else
	HELP_TEXT = help.txt
endif

include Makefile.c-common

.PHONY:		test neo900 sch test testref png pngref pdf diff view newref
.PHONY:		leak

all::		$(NAME) eetest

$(NAME):	$(OBJS)
		$(MAKE) -B version.o
		$(CC) -o $(NAME) $(OBJS) $(LDLIBS)

eetest:		$(EETEST_OBJS)
		$(MAKE) -B version.o
		$(CC) -o eetest $(EETEST_OBJS) $(LDLIBS)

#----- Help texts -------------------------------------------------------------

help.inc:	$(HELP_TEXT) Makefile
		$(BUILD) sed 's/"/\\"/g;s/.*/"&\\n"/' $< >$@ || \
		    { rm -f $@; exit 1; }

gui/help.c:	help.inc

clean::
		rm -f help.inc

#----- Icons ------------------------------------------------------------------

icons/%.hex:	icons/%.fig Makefile
		$(BUILD) fig2dev -L png -S 4 -Z 0.60 $< | \
		    convert - -transparent white - | \
		    hexdump -v -e '/1 "0x%x, "' >$@; \
		    [ "$${PIPESTATUS[*]}" = "0 0 0" ] || { rm -f $@; exit 1; }

gui/icons.c:	$(ICONS:%=icons/%.hex)

clean::
		rm -f $(ICONS:%=icons/%.hex)

#----- Test sheet -------------------------------------------------------------

sch:
		eeschema test.sch

test:		$(NAME)
		./$(NAME) test.lib test.sch -- fig >out.fig
		fig2dev -L png -m 2 out.fig _out.png
		[ ! -r ref.png ] || \
		    compare -metric AE ref.png _out.png _diff.png || \
		    qiv -t -R -D _diff.png ref.png _out.png

testref:	$(NAME)
		./$(NAME) test.lib test.sch -- fig | \
		    fig2dev -L png -m 2 >ref.png

png:		$(NAME)
		./$(NAME) test.lib test.sch -- png -o _out.png -s 2
		[ ! -r pngref.png ] || \
		    compare -metric AE pngref.png _out.png _diff.png || \
		    qiv -t -R -D _diff.png pngref.png _out.png

pngref:		$(NAME)
		./$(NAME) test.lib test.sch -- png -o pngref.png -s 2

clean::
		rm -f out.fig _out.png _diff.png

#----- Render Neo900 schematics -----------------------------------------------

NEO900_HW = ../../n9/ee/hw
KICAD_LIBS = ../../qi/kicad-libs/components

SHEET ?= 12

neo900:		$(NAME)
		./$(NAME) $(NEO900_HW)/neo900.lib \
		    $(KICAD_LIBS)/powered.lib \
		    $(NEO900_HW)/neo900_SS_$(SHEET).sch \
		    >out.fig

neo900.pdf:	$(NAME) sch2pdf neo900-template.fig
		./sch2pdf -o $@ -t neo900-template.fig \
		    $(NEO900_HW)/neo900.lib $(KICAD_LIBS)/powered.lib \
		    $(NEO900_HW)/neo900.sch

pdf:		$(NAME)
		./eeshow $(NEO900_HW)/neo900.pro -- pdf -o neo900.pdf

#----- Regression test based on Neo900 schematics -----------------------------

diff:		$(NAME)
		test/genpng test out
		test/comp test || $(MAKE) view

view:
		qiv -t -R -D `echo test/_diff*.png | \
		    sed 's/\([^ ]*\)_diff\([^ ]*\)/\1_diff\2 \1ref\2 \1out\2/g'`

newref:
		test/genpng test ref

#----- Memory leak checking ---------------------------------------------------

SUPP = dl-init cairo-font

leak:		$(NAME)
		valgrind --leak-check=full --show-leak-kinds=all \
		    --num-callers=50 \
		    $(SUPP:%=--suppressions=%.supp) \
		    eeshow $(NEO900_HW)/neo900.pro -- \
		      diff $(NEO900_HW)/neo900.pro >/dev/null
#		    eeshow -N 1 $(NEO900_HW)/neo900.pro -- pdf >/dev/null
#		    eeshow -N 1 $(NEO900_HW)/neo900.pro -- png >/dev/null

#----- Cleanup ----------------------------------------------------------------

spotless::
		rm -f eetest
