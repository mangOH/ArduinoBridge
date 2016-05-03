TARGETS := ar7 wp7 ar86 wp85 localhost

export MANGOH_ROOT ?= $(PWD)/../..

.PHONY: all $(TARGETS)
all: $(TARGETS)

$(TARGETS):
	export TARGET=$@ ; \
	mkapp -v -t $@ \
		  -i ../DataRouter \
		  arduinoBridge.adef

clean:
	rm -rf _build_* *.ar7 *.wp7 *.ar86 *.wp85 *.wp85.update *.localhost
