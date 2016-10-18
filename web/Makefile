UPLOAD = wpwrak@neo900.org:pub/eeshow/
SCP = rsync -e "neo900 --ssh" -v

FIGS = wt-start wt-plain wt-pin-type wt-glabel wt-index wt-history wt-diff \
       wt-diff-old wt-diff-new wt-diff-index wt-doc

.PHONY:		upload

index.html:	$(FIGS:%=%.png)

%.png:		%.fig
		fig2dev -L png -S 4 $< >$@ || { rm -f $@; exit 1; }

upload:		$(FIGS:%=%.png)
		$(SCP) index.html $(FIGS:%=%.png) $(UPLOAD)
