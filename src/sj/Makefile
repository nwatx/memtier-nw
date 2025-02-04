CC = g++
CFLAGS = -g -Wall -O3 -fPIC
LDFLAGS = -shared
INCLUDES = -I../linux/usr/include
LIBS = -lm -lpthread

BINDIR = ./bin

all: ${BINDIR}/pebs_total_reads ${BINDIR}/pebs_total_writes ${BINDIR}/pebs_periodic_reads ${BINDIR}/pebs_periodic_writes

${BINDIR}/pebs_total_reads: pebs.cpp pebs.h
	@mkdir -p ${BINDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -o ${BINDIR}/pebs_total_reads.x pebs.cpp $(LIBS)

${BINDIR}/pebs_total_writes: pebs.cpp pebs.h
	@mkdir -p ${BINDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -DENABLE_WRITE -o ${BINDIR}/pebs_total_writes.x pebs.cpp $(LIBS)

${BINDIR}/pebs_periodic_reads: pebs.cpp pebs.h
	@mkdir -p ${BINDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -DPEBS_PERIODIC -o ${BINDIR}/pebs_periodic_reads.x pebs.cpp $(LIBS)

${BINDIR}/pebs_periodic_writes: pebs.cpp pebs.h
	@mkdir -p ${BINDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -DPEBS_PERIODIC -DENABLE_WRITE -o ${BINDIR}/pebs_periodic_writes.x pebs.cpp $(LIBS)

clean:
	rm -f ${BINDIR}/*.o ${BINDIR}/*.x