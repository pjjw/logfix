CC = gcc
CFLAGS = -O2 -I/usr/src/opensolaris/usr/src/uts/common/fs/zfs
LDFLAGS = -lnvpair -lzpool

OBJS = logfix.o 

logfix: ${OBJS}
	      ${CC} -o logfix ${LDFLAGS} ${OBJS}

logfix.o: logfix.c
	      ${CC} ${CFLAGS} -c logfix.c

clean:
	      rm -f logfix ${OBJS} core
	            @echo "all cleaned up!"
