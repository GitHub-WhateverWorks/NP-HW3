#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"

export LD_LIBRARY_PATH="$HERE/lib:$LD_LIBRARY_PATH"

exec "$HERE/game_store_gui" "$@"
EOF

chmod +x run_game_store_gui.sh
