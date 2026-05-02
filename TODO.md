---
# TODO
---

## Project-Panel
Das Projekt Panel zeigt die Liste der Dateien im aktuellen Verzeichnis.
Das aktuelle Verzeichnis ist das Verzeichnis des aktuellen Files.

Erweitere den QSplitter im Main-Window der App um ein weiteres Panel: dem Project-Panel.

Das Projekt Panel kann durch das neue Editor Kommando CMD_TOGGLE_PROJECT (Ctrl+O, Ctrl+P) ein- und
ausgeschaltet werden.

Erzeuge einen neuen Button links neben dem "AI" Button der das Project Panel ein-/ausschaltet.

Die Breite der App soll sich jeweils um die Breite des Panels bien ein-/ausschalten anpassen so dass das
Editor-Window immer gleich breit bleibt (wie für das AI-Panel).

Speichere den aktuelle Zustand (visible/invisible) sowie die Breite des Panels in nped.json (mache es persistent).

Ein Doppelclick auf ein File in der Liste öffnet einen neuen Kontext() für das File.

Markiere das aktuelle File des Editors im Project-Panel optisch.
