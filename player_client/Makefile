# ================================
# Player Game Store GUI Makefile
# ================================

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -pthread

SRC_DIR  := src
TARGET   := game_store_gui

INCLUDES := -I./shared -I./src

SOURCES  := \
    $(SRC_DIR)/game_store_gui.cpp \
    $(SRC_DIR)/base64.cpp

OBJECTS  := $(SOURCES:.cpp=.o)

# ------------------------------------------------------------
# SFML detection (user-space preferred, fallback to system)
# ------------------------------------------------------------

USER_SFML_PREFIX := $(HOME)/local/sfml/usr

ifeq ($(shell test -d $(USER_SFML_PREFIX)/include/SFML && echo yes),yes)
    SFML_INC := $(USER_SFML_PREFIX)/include
    SFML_LIB := $(USER_SFML_PREFIX)/lib/x86_64-linux-gnu
else
    SFML_INC := /usr/include
    SFML_LIB := /usr/lib/x86_64-linux-gnu
endif

INCLUDES += -I$(SFML_INC)

LDFLAGS := -L$(SFML_LIB) \
           -lsfml-graphics \
           -lsfml-window \
           -lsfml-system \
           -lsfml-network \
           -lsfml-audio \
           -pthread

# ------------------------------------------------------------
# Build rules
# ------------------------------------------------------------

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
