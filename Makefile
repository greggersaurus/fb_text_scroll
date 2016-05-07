CC = g++

INCLUDES =

LDFLAGS =

CFLAGS = -g -c -Wall $(INCLUDES) -std=gnu++0x

SOURCES = main.cpp

OBJECTS = $(SOURCES:.cpp=.o)
EXECUTABLE = fb_text_scroll

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC)  $(OBJECTS) -o $@ $(LDFLAGS)

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(OBJECTS) $(EXECUTABLE)
