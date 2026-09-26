#!/bin/bash
# Lance les tests automatiques du projet sans rendu (chapitres 5, 7, 17, 23 du dossier).
cd "$(dirname "$0")/.."
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PWD/EchosTriforce.uproject" -ExecCmds="Automation RunTests EchosTriforce; Quit" -nullrhi -unattended -nosplash -nosound -stdout 2>&1 | grep -E "Test Completed|Expected|Combat terminé|LogEchos: Error"
