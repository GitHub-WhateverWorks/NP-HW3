#!/usr/bin/env bash
set -e

APP_NAME="GameStore"
BIN_SRC="../player_client/game_store_gui"
OUT_DIR=".."
APPDIR="${APP_NAME}.AppDir"

# Clean old AppDir
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/lib"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"
ICON_SRC="../player_client/assets/icon.png"

if [ ! -f "$ICON_SRC" ]; then
    echo "WARNING: icon.png missing — creating placeholder"
    convert -size 256x256 xc:blue "${APPDIR}/usr/share/icons/hicolor/256x256/apps/GameStore.png"
else
    cp "$ICON_SRC" "${APPDIR}/usr/share/icons/hicolor/256x256/apps/GameStore.png"
fi
cp ../player_client/assets/icon.png \
   "${APPDIR}/usr/share/icons/hicolor/256x256/apps/GameStore.png"

# Also copy to AppDir root for AppImage
cp "${APPDIR}/usr/share/icons/hicolor/256x256/apps/GameStore.png" \
   "${APPDIR}/.DirIcon"

# And ensure icon exists for desktop
cp "${APPDIR}/usr/share/icons/hicolor/256x256/apps/GameStore.png" \
   "${APPDIR}/GameStore.png"

# Copy binary (FIXED PATH)
cp "${BIN_SRC}" "${APPDIR}/usr/bin/${APP_NAME}"
chmod +x "${APPDIR}/usr/bin/${APP_NAME}"

# AppRun
cat > "${APPDIR}/AppRun" << 'EOF'
#!/usr/bin/env bash
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:$LD_LIBRARY_PATH"
exec "$HERE/usr/bin/GameStore" "$@"
EOF
chmod +x "${APPDIR}/AppRun"

# Desktop file
cat > "${APPDIR}/usr/share/applications/${APP_NAME}.desktop" << EOF
[Desktop Entry]
Type=Application
Name=Game Store GUI
Exec=${APP_NAME}
Icon=${APP_NAME}
Categories=Game;
Terminal=false
EOF
# Copy .desktop file to AppDir root (required for older appimagetool)
cp "${APPDIR}/usr/share/applications/${APP_NAME}.desktop" \
   "${APPDIR}/${APP_NAME}.desktop"

# Icon (optional)
if [ -f "../player_client/assets/icon.png" ]; then
    cp "../player_client/assets/icon.png" \
        "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${APP_NAME}.png"
fi

# Build AppImage
if [ ! -f "$HOME/appimagetool" ]; then
    echo "ERROR: ~/appimagetool missing"
    exit 1
fi

"$HOME/appimagetool" "${APPDIR}" "${OUT_DIR}/${APP_NAME}-x86_64.AppImage"
echo "Built ${OUT_DIR}/${APP_NAME}-x86_64.AppImage"
