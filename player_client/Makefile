# ================================
# Player Game Store GUI Makefile
# ================================

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -DSFML_STATIC

SRC_DIR  := src
INCLUDES := -I./shared -I./src

TARGET   := game_store_gui

SOURCES  := \
    $(SRC_DIR)/game_store_gui.cpp \
    $(SRC_DIR)/base64.cpp

OBJECTS  := $(SOURCES:.cpp=.o)

# ================================
# Static SFML (2.5.1) + Dynamic SysLibs
# ================================

SFML_STATIC_LIBS := \
        -Wl,-Bstatic \
            -lsfml-graphics-s \
            -lsfml-window-s \
            -lsfml-system-s \
            -lsfml-audio-s \
            -lsfml-network-s \
            -lfreetype \
            -ljpeg \
        -Wl,-Bdynamic \
            -lpng \
            -lz \
            -lbrotlidec \
            -lbrotlicommon \
            -lharfbuzz \
            -lopenal \
            -lvorbisenc \
            -lvorbisfile \
            -lvorbis \
            -logg \
            -lFLAC \
            -lX11 \
            -lXrandr \
            -lXcursor \
            -lXi \
            -lXext \
            -lXinerama \
            -lXrender \
            -lXfixes \
            -lGL \
            -lGLU \
            -ludev \
            -pthread

LDFLAGS := $(SFML_STATIC_LIBS)

# ================================
# Build Rules
# ================================

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
