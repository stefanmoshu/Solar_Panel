CC = gcc
CFLAGS = -g -Wall -Wextra -O0
LD = gcc
LDFLAGS = -O0
TARGET = myapp

SRCDIR = src/
OBJDIR = obj/
INCDIR = -lpigpio -lpthread -lrt -ldl -I ./inc

SRCFILES = $(wildcard ${SRCDIR}*.c)
OBJFILES = $(patsubst ${SRCDIR}%.c, ${OBJDIR}%.o, ${SRCFILES})

CFLAGS += ${INCDIR}

all: clean ${OBJDIR} ${TARGET}

${OBJDIR}%.o: src/%.c
	@echo "[Compiling]" $@
	${CC} ${CFLAGS} -c $< -o $@

$(TARGET): $(OBJFILES)
	@echo "[Linking]" $@
	${LD} ${LDFLAGS} -o $@ $^ ${INCDIR}

run:
#	gpio mode 1 in
#	gpio mode 16 in
#	gpio mode 1 up
#	gpio mode 16 up
	./$(TARGET)

clean:
	rm -f ${OBJDIR}*.o
	rm -f $(TARGET)
