#!/bin/bash

set -e
set -x

conda_env="AppDir/usr"

mkdir -p ${conda_env}

cp -a ../.pixi/envs/default/* ${conda_env}

echo -e "\nDelete unnecessary stuff"
rm -rf ${conda_env}/include
find ${conda_env} -name \*.a -delete

mv ${conda_env}/bin ${conda_env}/bin_tmp
mkdir ${conda_env}/bin
cp ${conda_env}/bin_tmp/freecad ${conda_env}/bin/
cp ${conda_env}/bin_tmp/freecadcmd ${conda_env}/bin
cp ${conda_env}/bin_tmp/ccx ${conda_env}/bin/
cp ${conda_env}/bin_tmp/python ${conda_env}/bin/
cp ${conda_env}/bin_tmp/pip ${conda_env}/bin/
cp ${conda_env}/bin_tmp/pyside6-rcc ${conda_env}/bin/
cp ${conda_env}/bin_tmp/gmsh ${conda_env}/bin/
cp ${conda_env}/bin_tmp/dot ${conda_env}/bin/
cp ${conda_env}/bin_tmp/unflatten ${conda_env}/bin/
rm -rf ${conda_env}/bin_tmp

sed -i '1s|.*|#!/usr/bin/env python|' ${conda_env}/bin/pip

echo -e "\nCopying Icon and Desktop file"
cp ${conda_env}/share/applications/org.freecad.FreeCAD.desktop AppDir/
sed -i 's/Exec=FreeCAD/Exec=AppRun/g' AppDir/org.freecad.FreeCAD.desktop
cp ${conda_env}/share/icons/hicolor/scalable/apps/org.freecad.FreeCAD.svg AppDir/

# Remove __pycache__ folders and .pyc files
find . -path "*/__pycache__/*" -delete
find . -name "*.pyc" -type f -delete

# reduce size

# Remove parts of packages we do not want and we know aren't getting loaded under normal circumstances.
# Do not remove libraries that executables depend from, nor remove conda-meta; the former is handled
# and the latter needed by force_remove_conda_packages.
rm -rf \
    ${conda_env}/doc/global/ \
    ${conda_env}/lib/cmake/ \
    ${conda_env}/lib/graphviz/libgvplugin_gd.* \
    ${conda_env}/lib/graphviz/libgvplugin_gdk.* \
    ${conda_env}/lib/graphviz/libgvplugin_pango.* \
    ${conda_env}/lib/graphviz/libgvplugin_rsvg.* \
    ${conda_env}/lib/graphviz/libgvplugin_webp.* \
    ${conda_env}/lib/python3.11/site-packages/pandas/tests/ \
    ${conda_env}/lib/qt6/mkspecs/ \
    ${conda_env}/lib/qt6/bin/lupdate \
    ${conda_env}/lib/qt6/bin/qdoc \
    ${conda_env}/lib/qt6/plugins/egldeviceintegrations/ \
    ${conda_env}/lib/qt6/plugins/generic/ \
    ${conda_env}/lib/qt6/plugins/platforms/libqeglfs.so \
    ${conda_env}/lib/qt6/plugins/platforms/libqlinuxfb.so \
    ${conda_env}/lib/qt6/plugins/platforms/libqminimal.so \
    ${conda_env}/lib/qt6/plugins/platforms/libqminimalegl.so \
    ${conda_env}/lib/qt6/plugins/platforms/libqoffscreen.so \
    ${conda_env}/lib/qt6/plugins/platforms/libqvkkhrdisplay.so \
    ${conda_env}/lib/qt6/plugins/platforms/libqvnc.so \
    ${conda_env}/lib/qt6/plugins/qmllint/ \
    ${conda_env}/lib/qt6/plugins/qmlls/ \
    ${conda_env}/lib/qt6/plugins/qmltooling/ \
    ${conda_env}/lib/qt6/plugins/sqldrivers/ \
    ${conda_env}/lib/qt6/plugins/wayland-graphics-integration-server/ \
    ${conda_env}/lib/qt6/sbom/ \
    ${conda_env}/lib/libharfbuzz-cairo.so* \
    ${conda_env}/lib/libpangocairo* \
    ${conda_env}/lib/libQt6EglFs* \
    ${conda_env}/share/aclocal/ \
    ${conda_env}/share/gtk-doc/ \
    ${conda_env}/share/wayland/ \
    ${conda_env}/share/wayland-protocols/ \
    ${conda_env}/man/

./force_remove_conda_packages.py \
    --print-rdeps-before-removal \
    -e "${conda_env}" \
    -s symbol_subst.yaml \
    --keep-needed-libs 'libdrm.+' \
    '*openvino*' \
    '*gtk3*' \
    'libllvm*' \
    'libclang*' \
    libva \
    'mysql-*' \
    cairo \
    libdrm \
    pang \
    epoxy \
    sdl3 \
    librsvg \
    sdl2 \
    scipy \
    libraw \
    tk \
    gdk-pixbuf \
    libtheora \
    libpq \
    libsndfile \
    libcurl

rm -rf ${conda_env}/conda-meta/

find ${conda_env} \( \
    -name "*.h" -o \
    -name "*.prl" -o \
    -name "*.cmake" \
    \) -type f -delete

version_name="FreeCAD_${BUILD_TAG}-Linux-$(uname -m)"

echo -e "\################"
echo -e "version_name:  ${version_name}"
echo -e "################"

pixi list -e default > AppDir/packages.txt
sed -i "1s/.*/\nLIST OF PACKAGES:/" AppDir/packages.txt

echo "Running FreeCAD command-line smoke test..."
if ! "${conda_env}/bin/freecadcmd" --safe-mode --version; then
    echo "FreeCAD command-line smoke test failed; the Linux bundle cannot start."
    exit 1
fi

echo "Running FreeCAD bundled Pivy smoke test..."
if ! "${conda_env}/bin/freecadcmd" --safe-mode --console "import pivy; from pivy import coin; print(pivy.__file__); print(coin.SoDB.getVersion())"; then
    echo "FreeCAD bundled Pivy smoke test failed; the Linux bundle cannot import the bundled Coin/Pivy runtime."
    exit 1
fi

#curl -LO https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-$(uname -m).AppImage
chmod a+x appimagetool-$(uname -m).AppImage

if [ "${UPLOAD_RELEASE}" == "true" ]; then
    case "${BUILD_TAG}" in
        *weekly*)
            GH_UPDATE_TAG="weeklies"
            ;;
        *rc*)
            GH_UPDATE_TAG="${BUILD_TAG}"
            ;;
        *)
            GH_UPDATE_TAG="latest"
            ;;
    esac
fi

echo -e "\nCreate the appimage"
# export GPG_TTY=$(tty)
chmod a+x ./AppDir/AppRun
./appimagetool-$(uname -m).AppImage \
  --comp zstd \
  --mksquashfs-opt -Xcompression-level \
  --mksquashfs-opt 22 \
  -u "gh-releases-zsync|FreeCAD|FreeCAD|${GH_UPDATE_TAG}|FreeCAD*$(uname -m)*.AppImage.zsync" \
  AppDir ${version_name}.AppImage
  # -s --sign-key ${GPG_KEY_ID} \

echo -e "\nCreate hash"
sha256sum ${version_name}.AppImage > ${version_name}.AppImage-SHA256.txt

if [ "${UPLOAD_RELEASE}" == "true" ]; then
    gh release upload --clobber ${BUILD_TAG} "${version_name}.AppImage" "${version_name}.AppImage.zsync" "${version_name}.AppImage-SHA256.txt"
    if [ "${GH_UPDATE_TAG}" == "weeklies" ]; then
        generic_name="FreeCAD_weekly-Linux-$(uname -m)"
        mv "${version_name}.AppImage" "${generic_name}.AppImage"
        mv "${version_name}.AppImage.zsync" "${generic_name}.AppImage.zsync"
        mv "${version_name}.AppImage-SHA256.txt" "${generic_name}.AppImage-SHA256.txt"
        gh release create weeklies --prerelease | true
        gh release upload --clobber weeklies "${generic_name}.AppImage" "${generic_name}.AppImage.zsync" "${generic_name}.AppImage-SHA256.txt"
    fi
fi
