#!/bin/bash
# Compile la cible éditeur du projet.
cd "$(dirname "$0")/.."
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" EchosTriforceEditor Mac Development -Project="$PWD/EchosTriforce.uproject" -WaitMutex 2>&1 | grep -E "error|warning: |Result:|Total execution" | grep -v "Referenced directory"
