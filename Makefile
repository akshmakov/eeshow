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

NAME = eeshow
OBJS = main.o \
       kicad/sch-parse.o kicad/sch-render.o kicad/lib-parse.o \
       kicad/lib-render.o kicad/dwg.o kicad/delta.o \
       gui/gui.o gui/over.o gui/style.o gui/aoi.o gui/fmt-pango.o \
       file/file.o file/git-util.o file/git-file.o file/git-hist.o \
       gfx/style.o gfx/fig.o gfx/record.o gfx/cro.o gfx/diff.o gfx/gfx.o \
       gfx/text.o gfx/misc.o \
       diag.o

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

include ../common/Makefile.c-common

.PHONY:		test neo900 sch test testref png pngref pdf diff view newref

all::		$(NAME)

$(NAME):	$(OBJS)
		$(CC) -o $(NAME) $(OBJS) $(LDLIBS)

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

NEO900_HW = ../../../n9/ee/hw
KICAD_LIBS = ../../kicad-libs/components

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
		./eeshow -r neo900.lib kicad-libs/components/powered.lib \
		    $(NEO900_HW)/neo900.sch -- pdf -o neo900.pdf

#----- Regression test based on Neo900 schematics -----------------------------

diff:		$(NAME)
		test/genpng test out
		test/comp test || $(MAKE) view

view:
		qiv -t -R -D `echo test/_diff*.png | \
		    sed 's/\([^ ]*\)_diff\([^ ]*\)/\1_diff\2 \1ref\2 \1out\2/g'`

newref:
		test/genpng test ref
