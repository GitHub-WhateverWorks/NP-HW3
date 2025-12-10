# Top-level Makefile
# Builds server-side binaries into ./bin
# (Developer & player GUIs are built in their own subdir Makefiles)

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -pthread
INCLUDES := -Ishared -Iserver

# Where to put final binaries
BINDIR   := bin

# -------------------------------------------------------------------
# Source lists
# -------------------------------------------------------------------

# Developer server sources
DEV_SERVER_SRCS := \
    server/developer_server/main.cpp \
    server/developer_server/developer_server.cpp \
    server/developer_server/base64.cpp \
    server/developer_server/handlers/handle_login.cpp \
    server/developer_server/handlers/handle_register.cpp \
    server/developer_server/handlers/handle_list_my_games.cpp \
    server/developer_server/handlers/handle_upload_game.cpp \
    server/developer_server/handlers/handle_update_game.cpp \
    server/developer_server/handlers/handle_remove_game.cpp \
    server/database/db.cpp

DEV_SERVER_OBJS := $(DEV_SERVER_SRCS:.cpp=.o)

# Lobby server sources
LOBBY_SERVER_SRCS := \
    server/lobby_server/main.cpp \
    server/lobby_server/lobby_server.cpp \
    server/lobby_server/handlers/handle_player_register.cpp \
    server/lobby_server/handlers/handle_player_login.cpp \
    server/lobby_server/handlers/handle_list_games.cpp \
    server/lobby_server/handlers/handle_download_game.cpp \
    server/lobby_server/handlers/handle_create_room.cpp \
    server/lobby_server/handlers/handle_join_room.cpp \
    server/lobby_server/handlers/handle_start_game.cpp \
    server/lobby_server/handlers/handle_submit_review.cpp \
    server/lobby_server/handlers/handle_get_reviews.cpp \
    server/developer_server/base64.cpp \
    server/database/db.cpp


LOBBY_SERVER_OBJS := $(LOBBY_SERVER_SRCS:.cpp=.o)

# -------------------------------------------------------------------
# Phony targets
# -------------------------------------------------------------------

.PHONY: all servers clients dev_client player_client clean distclean

all: servers

servers: $(BINDIR)/dev_server $(BINDIR)/lobby_server

# Convenience targets to build clients via their own Makefiles
clients: dev_client player_client

dev_client:
	$(MAKE) -C developer_client

player_client:
	$(MAKE) -C player_client

# -------------------------------------------------------------------
# Link server binaries
# -------------------------------------------------------------------

$(BINDIR):
	mkdir -p $(BINDIR)

$(BINDIR)/dev_server: $(DEV_SERVER_OBJS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(DEV_SERVER_OBJS) $(INCLUDES)

$(BINDIR)/lobby_server: $(LOBBY_SERVER_OBJS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(LOBBY_SERVER_OBJS) $(INCLUDES)

# -------------------------------------------------------------------
# Generic compile rule
# -------------------------------------------------------------------

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# -------------------------------------------------------------------
# Cleaning
# -------------------------------------------------------------------

clean:
	rm -f $(DEV_SERVER_OBJS) $(LOBBY_SERVER_OBJS)

distclean: clean
	rm -f $(BINDIR)/dev_server $(BINDIR)/lobby_server
