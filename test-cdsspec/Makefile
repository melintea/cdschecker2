#DIRS := barrier mcs-lock mpmc-queue spsc-queue spsc-bugfix linuxrwlocks \
	dekker-fences chase-lev-deque ms-queue chase-lev-deque-bugfix \
	concurrent-hashmap seqlock spsc-example spsc-queue-scfence \
	treiber-stack register

DIRS := register ms-queue linuxrwlocks mcs-lock chase-lev-deque-bugfix \
	treiber-stack ticket-lock seqlock-versioned

.PHONY: $(DIRS)

all: $(DIRS)

clean: $(DIRS:%=clean-%)

$(DIRS):
	$(MAKE) -C $@

clean-%:
	-$(MAKE) -C $* clean
