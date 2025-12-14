# ------------------------------------------------------------
#  Compiler
# ------------------------------------------------------------
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread

# ------------------------------------------------------------
#  Project structure
# ------------------------------------------------------------
BIN_DIR := bin
OBJ_DIR := obj

INCLUDES := -Iengine -Iserver -Iclient_cli -Iclient_gui -Ishared

# ------------------------------------------------------------
#  SFML detection (user-space preferred)
# ------------------------------------------------------------

# User-space SFML (no sudo)
USER_SFML_PREFIX := $(HOME)/local/sfml/usr

ifeq ($(shell test -d $(USER_SFML_PREFIX)/include/SFML && echo yes),yes)
    # No user-space SFML → use system
    SFML_INC := /usr/include
    SFML_LIB := /usr/lib/x86_64-linux-gnu
else
    # User-space SFML present
    SFML_INC := $(USER_SFML_PREFIX)/include
    SFML_LIB := $(USER_SFML_PREFIX)/lib/x86_64-linux-gnu
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
#  Targets
# ------------------------------------------------------------
.PHONY: all clean prep

all: prep game_server game_client_cli game_client_gui

prep:
	@mkdir -p $(BIN_DIR)
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
#  Server
# ------------------------------------------------------------
SERVER_SRCS := server/game_server.cpp server/main.cpp
SERVER_OBJS := $(SERVER_SRCS:%.cpp=$(OBJ_DIR)/%.o)

game_server: $(ENGINE_OBJS) $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)/game_server \
		$(ENGINE_OBJS) $(SERVER_OBJS) -pthread

# ------------------------------------------------------------
#  CLI Client
# ------------------------------------------------------------
CLI_SRCS := client_cli/game_client_cli.cpp
CLI_OBJS := $(CLI_SRCS:%.cpp=$(OBJ_DIR)/%.o)

game_client_cli: $(ENGINE_OBJS) $(CLI_OBJS)
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)/game_client_cli \
		$(ENGINE_OBJS) $(CLI_OBJS) -pthread

# ------------------------------------------------------------
#  GUI Client (SFML)
# ------------------------------------------------------------
GUI_SRCS := client_gui/game_client_sfml.cpp
GUI_OBJS := $(GUI_SRCS:%.cpp=$(OBJ_DIR)/%.o)

game_client_gui: $(ENGINE_OBJS) $(GUI_OBJS)
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)/game_client_gui \
		$(ENGINE_OBJS) $(GUI_OBJS) $(LDFLAGS)

# ------------------------------------------------------------
#  Object compilation
# ------------------------------------------------------------
$(OBJ_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# ------------------------------------------------------------
#  Clean
# ------------------------------------------------------------
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
