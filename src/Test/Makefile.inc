
CC       = clang
# Compiling Flags:
CFLAGS   = -c -g --std=c99 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L  -I./include -I./../Proxy/include -I./../Proxy/Parsers/include -I./../Utils/include -I./../Utils -I./../Proxy

LINKER 	 = clang
# Linking Flags:
LFLAGS 	 = -g --std=c99 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L  -lpthread -pthread


