# ExtreamFS Backend - Proyecto 1 MIA
CXX = g++
CXXFLAGS = -std=c++17 -I.
SOURCES = main.cpp Server/Server.cpp Analyzer/Analyzer.cpp \
          DiskManagement/DiskManagement.cpp Sessions/SessionManager.cpp \
          Utilities/Utilities.cpp Structs/Structs.cpp
TARGET = main

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)

rebuild: clean all

.PHONY: all clean rebuild
