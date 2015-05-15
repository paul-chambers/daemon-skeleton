CC      = gcc
CFLAGS  += -Wall -Wextra
LDFLAGS += -ldl -lpopt
SRC	    = $(wildcard *.c)
OBJ     = $(patsubst %.c, obj/%.o, $(SRC))
BIN     = daemon

debug:   $(BIN)
    CFLAGS += -g -finstrument-functions
    LDFLAGS += -Wl,--export-dynamic

#release: $(BIN)
#    CFLAGS += -Werror
#    LDFLAGS += -Wl,--strip-all

obj/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) -DLOG_SCOPE=$(*F)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

obj/logscopes.inc:
	@echo "recreating" $@
	mkdir -p $(@D)
	@echo "typedef enum {" > $@
	@echo $(foreach scope, $(wildcard *.c), kLog_$(basename $(scope)),) >> $@
	@echo "kMaxLogScope } eLogScope;" >> $@
	@echo "" >> $@
	@echo $(foreach scope, $(wildcard *.c), "extern unsigned int gLogMax_$(basename $(scope));" ) >> $@

obj/logscopedefs.inc:
	@echo "recreating" $@
	mkdir -p $(@D)
	@echo "const char * logScopeNames[] = {" > $@
	@echo $(foreach scope, $(wildcard *.c), \"$(basename $(scope))\",) >> $@
	@echo "NULL };" >> $@
	@echo "" >> $@
	@echo "void logLogInit( void ) {" >> $@
	@echo "unsigned char *ptr = calloc(" >> $@
	@echo $(foreach scope, $(wildcard *.c), "gLogMax_$(basename $(scope)) +" ) >> $@
	@echo "0, sizeof(unsigned char));"  >> $@
	@echo "if (ptr == NULL) { fprintf(stderr, \"### Failed to allocate memory for logging - cannot continue\\n\" ); exit(ENOMEM); }" >> $@
	@echo $(foreach scope, $(wildcard *.c), "gLog[kLog_$(basename $(scope))].site = ptr; ptr += gLogMax_$(basename $(scope)); " ) >> $@
	@echo $(foreach scope, $(wildcard *.c), "gLog[kLog_$(basename $(scope))].max = gLogMax_$(basename $(scope));" ) >> $@
	@echo "}" >> $@


logging.h: obj/logscopes.inc

logging.c: obj/logscopedefs.inc

*.c: logging.h logging-epilogue.h

cleandebug:	clean
cleanrelease:	clean

clean:
	rm -f obj/* $(BIN)

.PHONY: debug release clean
