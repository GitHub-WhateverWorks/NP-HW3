# ================================
# Player Game Store GUI Makefile
# ================================

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2

SFML_LIBS := -lsfml-graphics -lsfml-window -lsfml-system

# ✅ shared + local src only
INCLUDES := -I./shared -I./src
LDFLAGS  := $(SFML_LIBS)

SRC_DIR  := src

# ✅ base64.cpp now local
SOURCES  := \
    $(SRC_DIR)/game_store_gui.cpp \
    $(SRC_DIR)/base64.cpp

OBJECTS  := $(SOURCES:.cpp=.o)

TARGET   := game_store_gui

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
