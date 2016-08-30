FIGS = wt-start wt-plain wt-pin-type wt-glabel wt-index wt-history wt-diff \
       wt-diff-old wt-diff-new

.PHONY:		upload

index.html:	$(FIGS:%=%.png)

%.png:		%.fig
		fig2dev -L png -S 4 $< >$@ || { rm -f $@; exit 1; }

upload:		$(FIGS:%=%.png)
		neo900 index.html $(FIGS:%=%.png) pub/werner/tmp/eeshow/
