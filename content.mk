# build dir location, do not change the var name
build_dir = build

# install dir location, do not change the var name
install_dir = install

# Do not change the following target var name
gvd_bin = gvd

# Define include dir
# Do not change the include dir name
INCLUDE_DIR = -I .

# Define CFLAGS
# Do not change the include dir name
MK_CFLAGS = -g -Wall -Werror

gvd = gvd_cli_cfg_sys.c \
      gvd_cli_parser.c \
      gvd_cli_tree.c \
      gvd_cli_tty.c \
      gvd_common.c \
      gvd_line_buffer.c \
      gvd_main.c \
      gvd_tty.c \
      gvd_util.c

# define relied so path for bin target
# keep the var name as bin_LDSO
gvd_LDSO = -lncurses
