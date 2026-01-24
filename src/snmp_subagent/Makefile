CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lnetsnmpagent -lnetsnmp -lcrypto -lm

# 获取 Net-SNMP 的编译标志
NETSNMP_CFLAGS = $(shell net-snmp-config --cflags)
NETSNMP_LIBS = $(shell net-snmp-config --libs)

TARGET = custom_subagent
SOURCES = custom_subagent.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(NETSNMP_CFLAGS) $^ -o $@ $(NETSNMP_LIBS) $(LDFLAGS)

clean:
	rm -f $(TARGET)
