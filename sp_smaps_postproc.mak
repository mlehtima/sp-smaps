.PHONY: build clean

# tools

PREPROCESS = ./smaps_to_csv.py
ANALYZE    = ./smaps_analyze.py

# sources
CAPS = $(wildcard *.cap)

# targets
HTML = $(patsubst %.cap,%.results.html,$(CAPS))


.PRECIOUS: %.csv

%.results.csv    : %.cap ; $(PREPROCESS) < $< > $@
%.obfuscated.csv : %.cap ; $(PREPROCESS) < $< > $@ flag
%.html           : %.csv ; $(ANALYZE)    $<

build :: $(HTML) $(TXTS)

clean ::
	$(RM) *~ *.results.* *.obfuscated.*
	$(RM) -r *.results
