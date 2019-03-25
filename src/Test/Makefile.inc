
CC       = clang
# Compiling Flags:
CFLAGS   = -c -g --std=c99 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200809L -I./include -I./../pop3filter/include -I./../pop3filter/Parsers/include -I./../Utils/include -I./../Utils -I./../pop3filter

LINKER 	 = clang
# Linking Flags:
LFLAGS 	 = -g --std=c99 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200809L  -lpthread -pthread


