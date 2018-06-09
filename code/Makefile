# Necessary
SHELL		= /bin/sh

# Some variables
CC 			= gcc
CFLAGS		= -g -Wall -Werror -O2 -DENABLE_LOG
LDFLAGS		= -lm
TESTDEFS	= -DTESTING	-DDEBUG		# comment this out to disable debugging code
OBJS		= peer.o bt_parse.o spiffy.o debug.o input_buffer.o chunk.o sha.o \
				linkedlist.o bt_io.o log.o packet.o hash.o transfer.o \
				bt_server.o bt_client.o congestion.o mytime.o \
				parse.o bitrate.o stream.o
MK_CHUNK_OBJS   = make_chunks.o chunk.o sha.o log.o hash.o

BINS            = peer make-chunks
TESTBINS        = test_debug test_input_buffer

# Implicit .o target
.c.o:
	$(CC) $(TESTDEFS) -c $(CFLAGS) $<

# Explit build and testing targets

all: ${BINS} ${TESTBINS}

run: peer_run
	./peer_run

test: peer_test
	./peer_test

peer: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

make-chunks: $(MK_CHUNK_OBJS)
	$(CC) $(CFLAGS) $(MK_CHUNK_OBJS) -o $@ $(LDFLAGS)

clean:
	rm -f *.o $(BINS) $(TESTBINS)

bt_parse.c: bt_parse.h

# The debugging utility code

debug-text.h: debug.h
	./debugparse.pl < debug.h > debug-text.h

test_debug.o: debug.c debug-text.h
	${CC} debug.c ${INCLUDES} ${CFLAGS} -c -D_TEST_DEBUG_ -o $@

test_input_buffer:  test_input_buffer.o input_buffer.o
