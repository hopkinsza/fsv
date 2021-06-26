# Makefile

PROG = fsv
SRCS = fsv.c proc.c util.c

CFLAGS += -Wall


.PHONY: clean

$(PROG): $(SRCS)

$(SRCS): extern.h
	@echo "extern.h updated: touch \`$@'"
	@touch $@

clean:
	@rm $(PROG)
	@echo cleaned

# a.out: fsv.c
# 	cc fsv.c `pkg-config --cflags --libs libbsd-overlay`
