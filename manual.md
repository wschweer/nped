# NPEd Manual

## Übersicht

NPed (Program Editor) ist ein moderner C++23 und Qt6-basierter Texteditor bzw. eine leichtgewichtige IDE.

Hier ist eine Übersicht der zentralen Funktionen und Architekturmerkmale des Projekts:

#### Kernfunktionen des Editors
  * Multifile-Verwaltung: Öffnen und Bearbeiten mehrerer Dateien in Tabs (TabBar). Tabs visualisieren den Dateistatus farblich (z. B. rot für modifiziert, blau für schreibgeschützt).
  * Kontext-Management: Jeder Tab und jede geöffnete Datei wird in einem Kontext verwaltet, der u.a. die Cursor-Position und den Verlauf (History) speichert. Dies ermöglicht einfaches Navigieren (Vor/Zurück) im Code.
  * Standard-Editing: Undo/Redo-Stack (undo.cpp), Suche & Ersetzen mit Regulären Ausdrücken (search.cpp), Code Folding, sowie zeilen- und spaltenweises Auswählen (Pick/Put).
  * Syntax-Highlighting & Formatting: Automatisiertes Formatieren und Pretty-Printing des Codes (pretty.cpp).

#### Language Server Protocol (LSP) Integration

Über den eingebauten LSclient kann der Editor mit Sprachservern kommunizieren (z.B. clangd für C/C++). Daraus resultieren fortgeschrittene IDE-Funktionen:

  * Code Navigation: Go to Definition, Go to Type Definition, Go to Implementation.
  * Autovervollständigung: Request Completions via LSP mit einem grafischen Popup (completion.cpp).
  * Hover-Informationen: Anzeigen von Typen- oder Dokumentationshinweisen beim Verweilen mit dem Cursor.
  * Refactoring: Kontextweites Umbenennen von Symbolen (Rename).

#### Integrierter KI-Agent (AgentUI)

Das wohl herausstechendste Merkmal ist die enge Integration eines KI-Agenten, der aktiv in die Entwicklung eingreifen kann:

  * Modell-Unterstützung: Kompatibel mit lokalen Modellen (via Ollama) sowie Cloud-Modellen (Gemini, Anthropic).
  * Werkzeuge (Tool-Calling): Der Agent hat direkten Lese- und Schreibzugriff auf das Projekt. Er kann Dateien lesen, ändern und erstellen (handleReadFile, handleModifyFile), Verzeichnisse auflisten, oder das Projekt nach Texten und Symbolen durchsuchen (handleSearchProject, handleFindSymbol).
  * Projekt-Builds: Der Agent kann Build-Kommandos im Projekt ausführen (runBuildCommand), um z. B. CMake oder Make anzustoßen.
  * Web-Recherche: Der Agent kann gezielt Web-Dokumentationen abrufen (fetchWebDocumentation).

### Git-Integration
Der Editor hat native Git-Unterstützung integriert (basierend auf libgit2 in git.cpp):

  * Abrufen von Git-Status, Diffs und Logs.
  * Grafisches Git-Panel und Git-History-Ansicht im Editorfenster.
  * Der KI-Agent selbst kann automatisiert Commits erstellen (createGitCommit), sobald er Aufgaben erledigt hat.

### Architektur und Technik
  * C++23 & CMake: Modernes C++ gepaart mit einem CMake-Buildsystem.
  * Qt6: Verwendet für die grafische Benutzeroberfläche (Widgets, Splitter, Menüs). Styling erfolgt auch über Qt Stylesheets (style.qss).
  * Bibliotheken: Nutzt nlohmann::json stark für das Parsen und Erzeugen von LSP-Nachrichten, LLM-Prompts/Tool-Calls und das Speichern von Settings/Sitzungen.

Zusammenfassend ist nped also ein intelligenter Code-Editor, der LSP für präzise statische Code-Analyse mit LLM-basierten KI-Agenten kombiniert, um Entwicklungsaufgaben zu automatisieren.
NPed ist ein Source-Code Editor mit speziellen Features für C und C++.
Wer bevorzugt auf der Kommandozeile arbeitet, für den ist Ped so eine Art Mini-IDE.

### Bedien-Konzept

Das Bedienkonzept von **nped** orientiert sich stark an der Effizienz für Vielschreiber
und Entwickler, die bevorzugt ohne Mauseinsatz programmieren. Es ist als eine
Art "Mini-IDE für die Kommandozeile, aber mit GUI" konzipiert.

Hier sind die wesentlichen Aspekte des Bedienkonzepts:

### 1. Tastaturzentriert ("Blindes Editieren")
* **Kein Mauszwang:** Nahezu alle Funktionen werden über Tastaturkürzel aufgerufen. Du sollst deine Hände nicht von der Tastatur nehmen müssen.
* **Zehnfingersystem empfohlen:** Das Konzept ist nicht optimal für das \"Zwei-Finger-Suchsystem\", da die Befehle blind und schnell ausgeführt werden sollen.
* **Keine überladenen Menüs:** Die verfügbaren Befehle sind im Programm nicht prominent als Schaltflächen sichtbar. Das Erlernen der Shortcuts über eine Referenzkarte oder das Manual ist notwendig.

### 2. Das Befehlssystem (Command Entry)
Funktionen, die Parameter benötigen (wie das Öffnen einer Datei oder Suchen/Ersetzen), folgen einem speziellen Workflow:

1. **`<Escape>`** drücken: Dadurch öffnet sich am unteren Bildschirmrand (in der Statuszeile) ein Eingabefeld.
2. **Text eingeben:** Du tippst das Argument ein (z. B. den Suchbegriff oder den Dateinamen).
3. **Kommando-Taste drücken:** Anstatt Enter zu drücken, beendest du die Eingabe mit dem Shortcut der gewünschten Funktion. Wenn du z. B. nach dem eingegebenen Text suchen willst, drückst du `<F7>`.

### 3. Cursor-Steuerung (WordStar/Turbo-Pascal-Stil)
Die Cursor-Bewegungen orientieren sich an einem klassischen Block-Konzept um die Tasten `S, E, D, X`
(der "Cursor-Block"), wobei `Ctrl` als Modifikator und `Ctrl+Q` als Verstärker (Prefix) genutzt wird.
Dieses Layout ist stark an Editoren wie *WordStar* oder dem alten *Turbo Pascal / Borland* Editor angelehnt.

* **Einzelne Schritte:**
  * `Ctrl+S`: Zeichen links
  * `Ctrl+D`: Zeichen rechts
  * `Ctrl+E`: Zeile hoch
  * `Ctrl+X`: Zeile tief
* **Wort-/Seitenweise:**
  * `Ctrl+A`: Wort links
  * `Ctrl+F`: Wort rechts
  * `Ctrl+R`: Seite hoch
  * `Ctrl+C`: Seite tief

* **Sprünge (mit `Ctrl+Q` als Prefix):**
  * `Ctrl+Q, Ctrl+S`: Zeilenanfang
  * `Ctrl+Q, Ctrl+D`: Zeilenende
  * `Ctrl+Q, Ctrl+E`: Seitenanfang
  * `Ctrl+Q, Ctrl+X`: Seitenende
  * `Ctrl+Q, Ctrl+R`: Textanfang
  * `Ctrl+Q, Ctrl+C`: Textende

### 4. Code-spezifische Formatierungsregeln (Auto-Clean)
Der Editor erzwingt bestimmte Formatierungen automatisch, um sauberen Code zu gewährleisten:

* Leerzeichen (Trailing Spaces) am Ende einer Zeile werden automatisch entfernt.
* Leerzeilen am Ende einer Datei werden gelöscht.
* Die letzte Zeile wird **immer** mit einem Zeilenumbruch (`\\n`) abgeschlossen.
* Es können keine komplett leeren Dateien gespeichert werden; sie werden beim Sichern ignoriert.

### 5. IDE-Features über Shortcuts
Da nped als Mini-IDE fungiert, sind LSP-Funktionen ebenfalls auf Tasten gelegt (oft im Zusammenspiel mit
dem Command Entry).
Zum Beispiel das Formatieren der aktuellen Datei geschieht über die Kombination `Ctrl+O, Ctrl+F`.

Zusammenfassend richtet sich nped an Entwickler, die eine puristische, hochgradig tastaturgesteuerte
Umgebung suchen, die klassischen Editor-Flow mit modernen C++ Language Server-Funktionen verbindet.

## IDE - Features

Ein "Projekt" ist ein Verzeichnis, welches eine CMakeLists.txt Datei enthält. Der Editor
kann auch in einem Projekt Unterverzeichnis gestartet werden.

Für Informationen über das gesamte Projekt kontaktiert Ped einen Languageserver. Für c++ wird dazu der clangd
Languageserver gestartet.
Der LS parst den aktuellen Quellcode und benötigt dazu die gleiche Umgebung wie beim späteren Compilerlauf damit
er z.B. alle Headerdateien findet. Dazu muss man CMake anweisen, eine Datei ``compile_commands.json`` zu erzeugen die
genau diese Informationen enthält.

CMake wird dazu wie folgt aufgerufen:

    cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -G Ninja

Cmake schreibt dann die "compile_commands.json" ins build-Verzeichnis "build".
Wird Ped innerhalb des Projektverzeichnisses oder eines Projekt Unterverzeichnis gestartet, findet der clangd
LS alles weitere automatisch.

### **Globale Kommandos**

      [F1]                Sichert geänderte Dateien und Beendet den Editor.
      [Shift+F1]          Sichert
      [Ctrl+K, Ctrl+Q]    Beendet den Editor **ohne** geänderte Dateien zurückzuschreiben.
      [Escape]            "Enter" Kommando: öffnet Eingabefenster auf der Statuszeile zur Eingabe von Parmetern
      [Enter <name> F3]   öffnet Datei `name` in neuem Kontextfenster


### **Cursor Kommandos**

      [Ctrl+D oder Cursor-Rechts]  CMD_CHAR_RIGHT
      [Ctrl+S oder `Cursor-Links]  CMD_CHAR_LEFT
      [Up;    Ctrl+E]              CMD_LINE_UP
      [Down;  Ctrl+X]              CMD_LINE_DOWN
      [Ctrl+Q,  Ctrl+S]            CMD_LINE_START
      [Ctrl+Q,  Ctrl+D]            CMD_LINE_END
      [Ctrl+Q,  Ctrl+E]            CMD_LINE_TOP
      [Ctrl+Q,  Ctrl+X]            CMD_LINE_BOTTOM
      [Ctrl+R;  PageUp]            CMD_PAGE_UP
      [Ctrl+C;  PageDown]          CMD_PAGE_DOWN
      [Ctrl+Q,  Ctrl+R]            CMD_FILE_BEGIN
      [Ctrl+Q,  Ctrl+C]            CMD_FILE_END
      [Ctrl+A]                     CMD_WORD_LEFT
      [Ctrl+F]                     CMD_WORD_RIGHT
      [Enter <nn> Ctrl+G]          gehe nach Zeile nn

### **Text verändern**

      [Ctrl+Z]                     CMD_UNDO
      [Shift+Ctrl+Z]               CMD_REDO
      [Delete; Backspace]          CMD_RUBOUT
      [Ctrl+G]                     CMD_CHAR_DELETE
      [Ctrl+N]                     CMD_INSERT_LINE
      [F8]                         CMD_PICK
      [F9]                         CMD_PUT
      [Ctrl+Y]                     CMD_DELETE_LINE
      [Ctrl+Q,  Ctrl+Y]            CMD_DELETE_LINE_RIGHT
      [Ctrl+T]                     CMD_DELETE_WORD


### **Datei Kommandos**

      [Ctrl+K,  Ctrl+S]            CMD_SAVE
      [Ctrl+K,  Ctrl+K; F4]        CMD_KONTEXT_COPY
      [Ctrl+K,  Ctrl+J; Shift+F3]  CMD_KONTEXT_PREV
      [Ctrl+K,  Ctrl+L; F3]        CMD_KONTEXT_NEXT
      [Ctrl+K,  Ctrl+I]            CMD_SHOW_BRACKETS
      [Ctrl+Up]                    CMD_KONTEXT_UP
      [Ctrl+Down]                  CMD_KONTEXT_DOWN
      [F5]                         CMD_SELECT_ROW
      [F6]                         CMD_SELECT_COL
      [Ctrl+O,  Ctrl+W]            CMD_ENTER_WORD


### **Suchen/Ersetzen**

      [Enter <text> F7]               suche nach `text`
      [Enter <suchen>/<ersetzen> F7]  suche `suchen` und ersetze durch `ersetzen`
      [F7]                            vorwärts weitersuchen
      [SHIFT+F7]                      rückwärts weitersuchen
      [Ctrl+O, Ctrl+R]                projektweites umbenennen über den Language Server


### **IDE Kommandos**

      [Ctrl+K,  Ctrl+M]             CMD_SHOW_LEVEL
#### **``Ctrl+O,  Ctrl+F``** Formatiert eine Datei
Das Format Kommando wird zum Language Server geschickt. Es muss ein LS für den aktuellen Filetyp
konfiguriert und verfügbar sein und er muss das Format Kommando unterstützen. Für C/C++ funktioniert
das mit dem clangd LanguageServer prima.

#### **``Ctrl+V``** Schaltet auf eine andere Ansicht der aktuellen Datei.
Für Markdown- sowie Html-Dateien
wird von der editierbaren Textdarstellung auf eine gerenderte 'nur lesen' Darstellung umgeschaltet.
<p>Für C/C++ Dateien wird
eine Liste von Funktions- bzw. Methodennamen gezeigt. Du kannst den Cursor auf eine Funktion positionieren und wieder in
die Textdarstellung zurückschalten um blitzschnell zu dieser Funktion zu navigieren.

      [Ctrl+B]                      CMD_VIEW_BUGS
      [F10]                         CMD_GOTO_TYPE_DEFINITION
      [F11]                         CMD_GOTO_IMPLEMENTATION
      [F12]                         CMD_GOTO_DEFINITION
      [Ctrl+O, Ctrl+E]              CMD_EXPAND_MACROS
      [Ctrl+Tab; Shift+Tab]         CMD_COMPLETIONS
      [Ctrl+M]                      CMD_FOLD_ALL
      [Ctrl+Shift+M]                CMD_UNFOLD_ALL
      [Ctrl+<]                      CMD_FOLD_TOGGLE
      [Ctrl+O, Ctrl+H]              CMD_FUNCTION_HEADER
      [Ctrl+O, Ctrl+G]              CMD_GIT_TOGGLE
      [Ctrl+F12]                    CMD_GOTO_BACK
      [Ctrl+H]                      CMD_SHOW_INFO
      [Enter <name> Ctrl+F]         Erzeugt leere c++ Funktion mit Namen `name`


## Code Expander

Der Code Expander ist ein experimentelles Feature in NPed, um C++ Code via Python script zu erzeugen.
Die Python Scripte sind als C Kommentare getarnt und werden beim Schreiben der Sourcedatei ausgeführt.
Python Output wird dabei hinter dem Script in die Datei eingefügt.

Dies bedeutet:

* Es ist kein separater Präprozessor notwendig, der Quellcode wird "in place" geändert
* Keine Anpassung für das Build-System
* Leicht zu debuggen, da sowohl die Python Scripte als auch deren Output direkt sichtbar sind
* die expandierten Dateien sind normaler c++ code und das projekt kann auch ohne diese Ped-Features
weiter bearbeitet werden

Nachteile:

* Werden ausserhalb der Ped-Umgebung die generierten Code-Teile verändert, sollten auch die
  Scripte angepasst werden. Ansonsten gehen Änderungen verloren, wenn Code neu vom Script generiert wird.

Normalerweise zeigt Ped den generierten Teil auch an, was für Debug-Zwecke nützlich ist.
Für eine bessere Übersichtlichkeit kann der generierte Text auch einfach beim Lesen der Datei
übersprungen werden. Ein Kommentar in der ersten Zeile der Datei mit dem Text "//##H" schaltet
dieses Verhalten ein.


## Installation
### Font
      Es gibt verschiedene Fonts, die sich speziell zum Programmieren eignen:

- Fira Code
Das Konzept hinter Fira Code ist einfach: Die monospaced Schriftart wurde entwickelt, um häufig verwendete Zeichenfolgen zu einer einzigen zusammenzufassen und so die Zeit zu verkürzen, die du brauchst, um deinen Code zu überfliegen und das Gesuchte zu finden.

So wird z. B. aus dem Gleichheitszeichen (!=) ein Gleichheitszeichen mit einem Schrägstrich, die öffnenden und schließenden Symbole in HTML (</) sind enger beieinander und so weiter. Diese Ligaturen gibt es für viele Programmiersprachen.

Dabei werden die zugrunde liegenden Zeichen selbst nicht verändert, so dass es keine Auswirkungen auf deinen Code hat. Es macht ihn nur einfacher zu lesen! Es gibt auch einige Zeichenvarianten, damit du die Schriftart nach deinen Wünschen anpassen kannst.

Fira Code wird von den meisten Browsern unterstützt und du kannst dir in den Code-Beispielen ansehen, wie er in der Praxis aussieht.

- Proggy-Schriften
- DejaVu Sans Mono
- Source code Pro
- Dina
- Terminus
- Input
- Hack
- Cascadia Code
- JetBrains Mono
- Anonymous Pro
- Ubuntu Mono
- Consolas

Nped kann nur Schriften verwenden, die nichtproportional (Monospace) sind.

### Language Server

Für die "C" Sprachfamilie: c, c++

      sudo apt install clangd

### LLM
#### Ollama

Ollama wird über die Url http://localhost:11434 angesprochen. Wenn der Ollama Server beim Start von NPed
erreichbar ist werden die verfügbaren LLM's automatisch ermittelt und können
im Pulldownmenü angezeigt werden.


### Core Dumps

Ein normales Kubuntu Linux erzeugt keine Core-Dumps. Core Dumps sind für das Testen und Debuggen
von Programmen essentiell.

Um auf Ubuntu/Kubuntu Core-Dumps zu ermöglichen sind musst du:

- Apport abschalten

      sudo nano /etc/default/apport

ändere "enabled=1" nach "enabled=0"

dann:

      sudo systemctl stop apport.service
      sudo systemctl disable apport.service
