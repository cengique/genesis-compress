# TODO: could be made into a .in file for autoconf

# direct testing not possible since the compression only has 16-bits resolution
# must use matlab to load the files and ensure correctness:


# Cengiz Gunay 2007-07-17

SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .bin .genflac

TARGETS = genesis-single-chan.test.genflac genesis-single-chan.test.bin 

SRCDIR = ../src
GEN2FLAC = $(SRCDIR)/gen2flac
FLAC2GEN = $(SRCDIR)/flac2gen

all: $(TARGETS)
	matlab -nodesktop -r 'genflactest'

# Generic rules
#%.genflac: %.bin
#	$(GEN2FLAC) $< $@

%.bin: %.genflac
	$(FLAC2GEN) $< $@

# test rule
genesis-single-chan.test.genflac: genesis-single-chan.bin $(GEN2FLAC)
	$(GEN2FLAC) $< $@

clean:
	rm -f $(TARGETS)
