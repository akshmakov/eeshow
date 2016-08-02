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
OBJS = main.o sch-parse.o sch-render.o lib-parse.o lib-render.o \
       file.o git-file.o \
       style.o fig.o record.o cro.o diff.o gfx.o dwg.o text.o misc.o

CFLAGS = -g  -Wall -Wextra -Wno-unused-parameter -Wshadow \
	 -Wmissing-prototypes -Wmissing-declarations \
	 `pkg-config --cflags cairo` \
	 `pkg-config --cflags libgit2`
LDLIBS = -lm \
       `pkg-config --libs cairo` \
       `pkg-config --libs libgit2`

include ../common/Makefile.c-common

.PHONY:		test neo900 sch test testref png pngref diff view newref

all::		$(NAME)

$(NAME):	$(OBJS)
		$(CC) -o $(NAME) $(OBJS) $(LDLIBS)

#----- Test sheet -------------------------------------------------------------

sch:
		eeschema test.sch

test:		$(NAME)
		./$(NAME) test.lib test.sch >out.fig
		fig2dev -L png -m 2 out.fig _out.png
		[ ! -r ref.png ] || \
		    compare -metric AE ref.png _out.png _diff.png || \
		    qiv -t -R -D _diff.png ref.png _out.png

testref:	$(NAME)
		./$(NAME) test.lib test.sch | fig2dev -L png -m 2 >ref.png

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

#----- Regression test based on Neo900 schematics -----------------------------

diff:		$(NAME)
		test/genpng test out
		test/comp test || $(MAKE) view

view:
		qiv -t -R -D `echo test/_diff*.png | \
		    sed 's/\([^ ]*\)_diff\([^ ]*\)/\1_diff\2 \1ref\2 \1out\2/g'`

newref:
		test/genpng test ref
