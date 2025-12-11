#!/usr/bin/env bash
set -e

APP_NAME="DeveloperClient"
BIN_SRC="../developer_client/developer_client_gui"
OUT_DIR=".."
APPDIR="${APP_NAME}.AppDir"

# Clean old AppDir
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/lib"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"

ICON_SRC="../developer_client/assets/icon.png"

# Copy binary
cp "${BIN_SRC}" "${APPDIR}/usr/bin/${APP_NAME}"
chmod +x "${APPDIR}/usr/bin/${APP_NAME}"

# Copy icon
cp "${ICON_SRC}" "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${APP_NAME}.png"
cp "${ICON_SRC}" "${APPDIR}/${APP_NAME}.png"
cp "${ICON_SRC}" "${APPDIR}/.DirIcon"

# Create AppRun
cat > "${APPDIR}/AppRun" << EOF
#!/usr/bin/env bash
HERE="\$(dirname "\$(readlink -f "\$0")")"
export LD_LIBRARY_PATH="\$HERE/usr/lib:\$LD_LIBRARY_PATH"
exec "\$HERE/usr/bin/${APP_NAME}" "\$@"
EOF
chmod +x "${APPDIR}/AppRun"

# Create desktop file
cat > "${APPDIR}/usr/share/applications/${APP_NAME}.desktop" << EOF
[Desktop Entry]
Type=Application
Name=Developer Client
Exec=${APP_NAME}
Icon=${APP_NAME}
Categories=Development;
Terminal=false
EOF

# Copy desktop file to root (required)
cp "${APPDIR}/usr/share/applications/${APP_NAME}.desktop" \
   "${APPDIR}/${APP_NAME}.desktop"

# Build AppImage
"$HOME/appimagetool" "${APPDIR}" "${OUT_DIR}/${APP_NAME}-x86_64.AppImage"

echo "Built ${OUT_DIR}/${APP_NAME}-x86_64.AppImage"
