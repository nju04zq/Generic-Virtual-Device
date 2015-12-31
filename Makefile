####################################
###Makefile generated from script###
####################################
CC=gcc

build_dir = build

install_dir = install

gvd_LDSO = -lncurses

.PHONY: all
all: $(build_dir)/gvd

ifneq ($(MAKECMDGOALS), clean)
-include $(build_dir)/./gvd_cli_cfg_sys.d
-include $(build_dir)/./gvd_cli_parser.d
-include $(build_dir)/./gvd_cli_tree.d
-include $(build_dir)/./gvd_cli_tty.d
-include $(build_dir)/./gvd_common.d
-include $(build_dir)/./gvd_line_buffer.d
-include $(build_dir)/./gvd_main.d
-include $(build_dir)/./gvd_tty.d
-include $(build_dir)/./gvd_util.d
endif

INCLUDE_DIR = -I.

MK_CFLAGS =  -g -g -Wall -Werror 

$(build_dir)/gvd: $(build_dir)/./gvd_cli_cfg_sys.o \
                  $(build_dir)/./gvd_cli_parser.o \
                  $(build_dir)/./gvd_cli_tree.o \
                  $(build_dir)/./gvd_cli_tty.o \
                  $(build_dir)/./gvd_common.o \
                  $(build_dir)/./gvd_line_buffer.o \
                  $(build_dir)/./gvd_main.o \
                  $(build_dir)/./gvd_tty.o \
                  $(build_dir)/./gvd_util.o
	$(CC) -o $@ $^ $(gvd_LDSO)
	@echo -e "\nGenerated $@\n"

$(build_dir)/./%.o: ./%.c
	$(CC) -o $@ $(MK_CFLAGS) $(INCLUDE_DIR) -c $(filter %c,$^)
	@echo

$(build_dir)/./%.po: ./%.c
	$(CC) -fPIC -o $@ $(MK_CFLAGS) $(INCLUDE_DIR) -c $(filter %c,$^)
	@echo

$(build_dir)/./%.d: ./%.c $(build_dir)/./.probe
	@set -e; rm -f $@; \
	echo "Making $@"; \
	$(CC) -MM $(INCLUDE_DIR) $(filter %.c,$^) > $@.$$$$; \
	sed 's#\($*\)\.o[ :]*#$(dir $@)\1.o $(dir $@)\1.po $@ : #g'<$@.$$$$>$@;\
	rm -f $@.$$$$

$(build_dir)/./.probe:
	@mkdir -p $@

.PHONY: install
install:
	-mkdir -p $(install_dir); \
	cp $(build_dir)/gvd $(install_dir)

.PHONY: clean
clean:
	-rm -rf $(build_dir)
