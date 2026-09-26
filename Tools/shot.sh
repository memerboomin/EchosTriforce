#!/bin/bash
# Lance le jeu, prend des captures automatiques puis quitte.
# Exemple : Tools/shot.sh nereide "8,16" 18 battle [-ZAutoPlay]
cd "$(dirname "$0")/.."
AUTO="$1"; SHOTS="$2"; QUIT="$3"; NAME="$4"; shift 4
rm -f Saved/Screenshots/*/"${NAME}"_*.png 2>/dev/null
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor" "$PWD/EchosTriforce.uproject" -game -windowed -ResX=1920 -ResY=1080 -nosplash -nosound -unattended \
  -ZAuto="$AUTO" -ZShots="$SHOTS" -ZQuitAt="$QUIT" -ZShotName="$NAME" "$@" -log=EchosShot.log > /dev/null 2>&1
find Saved/Screenshots -name "${NAME}_*.png" -newer EchosTriforce.uproject 2>/dev/null | sort
