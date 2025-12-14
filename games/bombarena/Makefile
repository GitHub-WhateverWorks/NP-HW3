#sudo apt install libsfml-dev
#sudo apt install unzip
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread
INCLUDES := -Iengine -Iserver -Iclient_cli -Iclient_gui -I../.. -I../../shared
LDFLAGS  := -pthread

# ---- SFML (GUI client optional) ----
SFML_FLAGS := -lsfml-graphics -lsfml-window -lsfml-system

# Output dirs
BIN_DIR := bin
OBJ_DIR := obj

.PHONY: all clean prep

# Default: build everything
all: prep bombarena_server bombarena_client_cli bombarena_client_gui

prep:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/engine
	@mkdir -p $(OBJ_DIR)/server
	@mkdir -p $(OBJ_DIR)/client_cli
	@mkdir -p $(OBJ_DIR)/client_gui

# ---------------------------
#  Engine
# ---------------------------
ENGINE_SRCS := engine/engine.cpp
ENGINE_OBJS := $(ENGINE_SRCS:%.cpp=$(OBJ_DIR)/%.o)

# ---------------------------
#  Game Server
# ---------------------------
SERVER_SRCS := \
	server/game_server.cpp \
	server/main.cpp        # <--- ADDED

SERVER_OBJS := $(SERVER_SRCS:%.cpp=$(OBJ_DIR)/%.o)

bombarena_server: $(ENGINE_OBJS) $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)/game_server \
		$(ENGINE_OBJS) $(SERVER_OBJS) $(LDFLAGS)

# ---------------------------
#  CLI Client
# ---------------------------
CLI_SRCS := client_cli/game_client_cli.cpp
CLI_OBJS := $(CLI_SRCS:%.cpp=$(OBJ_DIR)/%.o)

bombarena_client_cli: $(ENGINE_OBJS) $(CLI_OBJS)
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)/game_client_cli \
		$(ENGINE_OBJS) $(CLI_OBJS) $(LDFLAGS)

# ---------------------------
#  GUI Client
# ---------------------------
GUI_SRCS := client_gui/game_client_sfml.cpp
GUI_OBJS := $(GUI_SRCS:%.cpp=$(OBJ_DIR)/%.o)

bombarena_client_gui: $(ENGINE_OBJS) $(GUI_OBJS)
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)/game_client_gui \
		$(ENGINE_OBJS) $(GUI_OBJS) $(LDFLAGS) $(SFML_FLAGS)

# ---------------------------
#  Generic Object Build
# ---------------------------
$(OBJ_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# ---------------------------
#  Clean
# ---------------------------
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
