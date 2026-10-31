#!/bin/zsh
# Builds dist/ForgeMaster-<version>-macOS.pkg from the universal release build.
# Installs Forge Master.vst3 and Forge Master.component into /Library/Audio/Plug-Ins.
# Usage: tools/make_installer.sh   (run from the project root after building build-universal)
set -euo pipefail

ROOT="${0:A:h:h}"
VERSION=$(sed -n 's/^project(ForgeMaster VERSION \([0-9.]*\)).*/\1/p' "$ROOT/CMakeLists.txt")
ART="$ROOT/build-universal/ForgeMaster_artefacts/Release"
# Stage on the internal disk: external volumes (exFAT etc.) don't keep Unix permissions,
# and pkgbuild would otherwise install the plug-ins readable by root only.
WORK="$(mktemp -d -t forgemaster-pkg)"
trap 'rm -rf "$WORK"' EXIT
DIST="$ROOT/dist"

mkdir -p "$WORK/vst3/VST3" "$WORK/au/Components" "$DIST"
ditto "$ART/VST3/Forge Master.vst3" "$WORK/vst3/VST3/Forge Master.vst3"
ditto "$ART/AU/Forge Master.component" "$WORK/au/Components/Forge Master.component"
chmod -R u=rwX,go=rX "$WORK/vst3" "$WORK/au"

pkgbuild --root "$WORK/vst3" --install-location "/Library/Audio/Plug-Ins" \
         --identifier com.forgeaudio.forgemaster.vst3 --version "$VERSION" "$WORK/vst3.pkg"
pkgbuild --root "$WORK/au" --install-location "/Library/Audio/Plug-Ins" \
         --identifier com.forgeaudio.forgemaster.au --version "$VERSION" "$WORK/au.pkg"

cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Forge Master $VERSION</title>
    <options customize="allow" require-scripts="false" hostArchitectures="x86_64,arm64"/>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>
    <choice id="vst3" title="VST3 plug-in" description="For FL Studio, Ableton Live, Cubase, Reaper and other VST3 hosts.">
        <pkg-ref id="com.forgeaudio.forgemaster.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit (AU)" description="For Logic Pro, GarageBand and other AU hosts.">
        <pkg-ref id="com.forgeaudio.forgemaster.au"/>
    </choice>
    <pkg-ref id="com.forgeaudio.forgemaster.vst3" version="$VERSION">vst3.pkg</pkg-ref>
    <pkg-ref id="com.forgeaudio.forgemaster.au" version="$VERSION">au.pkg</pkg-ref>
</installer-gui-script>
EOF

productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" \
             "$DIST/ForgeMaster-$VERSION-macOS.pkg"
echo "Built $DIST/ForgeMaster-$VERSION-macOS.pkg"
