TARGETS := wp85

export MANGOH_ROOT=$(LEGATO_ROOT)/../mangOH

export MANGOH_ROOT ?= $(PWD)/../..

.PHONY: all $(TARGETS)
all: wp85

$(TARGETS):
	export TARGET=$@ ; \
	mkapp -v -t $@ \
          arduinoBridge.adef

clean:
	rm -rf _build_* *.wp85 *.wp85.update
