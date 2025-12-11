# ------------------------------------------------------------
#  COMPILER + FLAGS
# ------------------------------------------------------------
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread -DSFML_STATIC
INCLUDES := -Iengine -Iserver -Iclient_cli -Iclient_gui -Ishared
LDFLAGS  := -pthread

# ------------------------------------------------------------
#  STATIC SFML LINK FLAGS  (correct order is critical)
# ------------------------------------------------------------
SFML_STATIC_LIBS := \
    -lsfml-graphics-s \
    -lsfml-window-s \
    -lsfml-system-s \
    -lsfml-audio-s \
    -lsfml-network-s \
    \
    -lfreetype \
    -lharfbuzz \
    -ljpeg \
    -lopenal \
    -lvorbisenc \
    -lvorbisfile \
    -lvorbis \
    -logg \
    -lFLAC \
    \
    -lX11 \
    -lXrandr \
    -lXcursor \
    -lXi \
    -lXext \
    -lXinerama \
    -lXrender \
    -lXfixes \
    \
    -lGL \
    -lGLU \
    -ludev \
    -pthread


# ------------------------------------------------------------
#  Build directories
# ------------------------------------------------------------
BIN_DIR := bin
OBJ_DIR := obj

.PHONY: all clean prep

all: prep bombarena_server bombarena_client_cli bombarena_client_gui

prep:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/engine
	@mkdir -p $(OBJ_DIR)/server
	@mkdir -p $(OBJ_DIR)/client_cli
	@mkdir -p $(OBJ_DIR)/client_gui


# ------------------------------------------------------------
#  Engine
# ------------------------------------------------------------
ENGINE_SRCS := engine/engine.cpp
ENGINE_OBJS := $(ENGINE_SRCS:%.cpp=$(OBJ_DIR)/%.o)


# ------------------------------------------------------------
#  Game Server
# ------------------------------------------------------------
SERVER_SRCS := \
	server/game_server.cpp \
	server/main.cpp

SERVER_OBJS := $(SERVER_SRCS:%.cpp=$(OBJ_DIR)/%.o)

bombarena_server: $(ENGINE_OBJS) $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)/game_server \
		$(ENGINE_OBJS) $(SERVER_OBJS) $(LDFLAGS)


# ------------------------------------------------------------
#  CLI Client
# ------------------------------------------------------------
CLI_SRCS := client_cli/game_client_cli.cpp
CLI_OBJS := $(CLI_SRCS:%.cpp=$(OBJ_DIR)/%.o)

bombarena_client_cli: $(ENGINE_OBJS) $(CLI_OBJS)
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)/game_client_cli \
		$(ENGINE_OBJS) $(CLI_OBJS) $(LDFLAGS)


# ------------------------------------------------------------
#  GUI Client (STATIC SFML)
# ------------------------------------------------------------
GUI_SRCS := client_gui/game_client_sfml.cpp
GUI_OBJS := $(GUI_SRCS:%.cpp=$(OBJ_DIR)/%.o)

bombarena_client_gui: $(ENGINE_OBJS) $(GUI_OBJS)
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)/game_client_gui \
		$(ENGINE_OBJS) $(GUI_OBJS) \
        \
	-Wl,-Bstatic \
		-lsfml-graphics-s \
		-lsfml-window-s \
		-lsfml-system-s \
		-lsfml-audio-s \
		-lsfml-network-s \
		-lfreetype \
		-ljpeg \
	\
	-Wl,-Bdynamic \
		-lpng \
		-lz \
		-lbrotlidec \
		-lbrotlicommon \
		\
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




# ------------------------------------------------------------
#  Generic object compilation
# ------------------------------------------------------------
$(OBJ_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@


# ------------------------------------------------------------
#  Clean
# ------------------------------------------------------------
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
