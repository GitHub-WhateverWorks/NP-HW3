#!/usr/bin/env bash
DIR="$(cd "$(dirname "$0")" && pwd)"

USER_SFML="$HOME/local/sfml/usr/lib/x86_64-linux-gnu"

if [ -d "$USER_SFML" ]; then
    export LD_LIBRARY_PATH="$USER_SFML:$LD_LIBRARY_PATH"
fi

exec "$DIR/game_client_gui"
