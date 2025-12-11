#!/usr/bin/env bash
set -e

APP_NAME="BombArenaClient"

# Correct path to BombArena GUI binary
BIN_SRC="../developer_client/games/bombarena/bin/game_client_gui"

OUT_DIR=".."
APPDIR="${APP_NAME}.AppDir"

ICON_SRC="../developer_client/assets/icon.png"

# Clean
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/lib"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"

# Copy binary
cp "${BIN_SRC}" "${APPDIR}/usr/bin/${APP_NAME}"
chmod +x "${APPDIR}/usr/bin/${APP_NAME}"

# Icon
cp "${ICON_SRC}" "${APPDIR}/${APP_NAME}.png"
cp "${ICON_SRC}" "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${APP_NAME}.png"
cp "${ICON_SRC}" "${APPDIR}/.DirIcon"

# AppRun
cat > "${APPDIR}/AppRun" << EOF
#!/usr/bin/env bash
HERE="\$(dirname "\$(readlink -f "\$0")")"
export LD_LIBRARY_PATH="\$HERE/usr/lib:\$LD_LIBRARY_PATH"
exec "\$HERE/usr/bin/${APP_NAME}" "\$@"
EOF

chmod +x "${APPDIR}/AppRun"

# Desktop file
cat > "${APPDIR}/usr/share/applications/${APP_NAME}.desktop" << EOF
[Desktop Entry]
Type=Application
Name=BombArena Client
Exec=${APP_NAME}
Icon=${APP_NAME}
Categories=Game;
Terminal=false
EOF

cp "${APPDIR}/usr/share/applications/${APP_NAME}.desktop" \
   "${APPDIR}/${APP_NAME}.desktop"

# Build AppImage
"$HOME/appimagetool" "${APPDIR}" "${OUT_DIR}/${APP_NAME}-x86_64.AppImage"

echo "Built ${OUT_DIR}/${APP_NAME}-x86_64.AppImage"
