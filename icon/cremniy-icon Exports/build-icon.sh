mkdir AppIcon.iconset
SRC=cremniy-icon-iOS-ClearDark-1024@1x.png

sips -z 16 16   "$SRC" --out AppIcon.iconset/icon_16x16.png
sips -z 32 32   "$SRC" --out AppIcon.iconset/icon_16x16@2x.png
sips -z 32 32   "$SRC" --out AppIcon.iconset/icon_32x32.png
sips -z 64 64   "$SRC" --out AppIcon.iconset/icon_32x32@2x.png
sips -z 128 128 "$SRC" --out AppIcon.iconset/icon_128x128.png
sips -z 256 256 "$SRC" --out AppIcon.iconset/icon_128x128@2x.png
sips -z 256 256 "$SRC" --out AppIcon.iconset/icon_256x256.png
sips -z 512 512 "$SRC" --out AppIcon.iconset/icon_256x256@2x.png
sips -z 512 512 "$SRC" --out AppIcon.iconset/icon_512x512.png
cp "$SRC" AppIcon.iconset/icon_512x512@2x.png

iconutil -c icns AppIcon.iconset -o cremniy.icns
