# NPEd Handbuch

Willkommen bei **NPEd**! NPEd ist ein moderner, auf C++23 und Qt6 basierender Texteditor, der sich als leichtgewichtige, tastaturzentrierte IDE versteht.

## 1. Übersicht

NPEd kombiniert die klassische, effiziente Bedienung bekannter Editoren mit modernen Features einer IDE. Hier sind die wichtigsten Merkmale im Überblick:

### Kernfunktionen
*   **Multifile-Verwaltung:** Arbeite in Tabs, die dir den Dateistatus farblich signalisieren (z. B. rot für modifizierte Dateien).
*   **Kontext-Management:** Jeder Tab verwaltet Cursor-Position und Historie – so navigierst du blitzschnell vor und zurück. Eine Datei kann dabei mehrere Kontexte besitzen.
*   **Standard-Editing:** Umfassende Funktionen wie Undo/Redo, Suchen & Ersetzen (auch mit regulären Ausdrücken) sowie zeilen- und spaltenweises Auswählen (Pick/Put).
*   **LSP-Integration:** Über das *Language Server Protocol* erhältst du IDE-Features wie Code-Navigation (*Go to Definition*), Autovervollständigung und Refactoring.
*   **KI-Agent:** Ein tief integrierter Agent unterstützt lokale (Ollama) sowie Cloud-Modelle (Gemini, Anthropic, OpenAI). Er beherrscht *Tool-Calling* und lässt sich via MCP-Server (Model Context Protocol) nahtlos erweitern.
*   **Git-Integration:** Native Unterstützung für Git-Status, Diffs und Historie direkt im Editorfenster.

### Technische Basis
*   **Modernes C++23 & CMake:** Leistungsstarkes Fundament für hohe Performance.
*   **Qt6:** Robuste grafische Benutzeoberfläche mit flexiblem Styling (via `style.qss`).
*   **Daten-Handling:** Intensive Nutzung von `nlohmann::json` für eine effiziente Kommunikation mit dem LSP und die Verwaltung deiner Einstellungen.

---

## 2. Bedienkonzept: Effizienz pur

NPEd ist für Entwickler konzipiert, die ihre Hände bevorzugt auf der Tastatur lassen. Das Bedienkonzept orientiert sich an klassischen Editoren wie *WordStar* oder *Turbo Pascal*, ergänzt um moderne Funktionen.

### Das Befehlssystem (Command Entry)
Funktionen, die Parameter benötigen (wie das Öffnen einer Datei oder Suchen/Ersetzen), rufst du so auf:
1. Drücke **`<Escape>`**, um am unteren Bildschirmrand ein Eingabefeld zu öffnen.
2. Gib dein Argument ein (z. B. einen Dateinamen oder Suchbegriff).
3. Bestätige mit dem Shortcut der gewünschten Funktion, anstatt die Eingabetaste zu drücken. (Beispiel: Suche mit **`<F7>`** starten).

### Cursor-Steuerung (WordStar/Turbo-Pascal-Stil)
Der "Cursor-Block" ist dein Zuhause. Hier nutzt du <kbd>Ctrl</kbd> als Modifikator und <kbd>Ctrl+Q</kbd> als Präfix für komplexere Sprünge:

*   **Grundbewegung:** 
    *   <kbd>Ctrl+S</kbd>: links | <kbd>Ctrl+D</kbd>: rechts | <kbd>Ctrl+E</kbd>: hoch | <kbd>Ctrl+X</kbd>: runter
*   **Wort-/Seitenweise:**
    *   <kbd>Ctrl+A</kbd>: Wort links | <kbd>Ctrl+F</kbd>: Wort rechts | <kbd>Ctrl+R</kbd>: Seite hoch | <kbd>Ctrl+C</kbd>: Seite runter
*   **Spezialsprünge (<kbd>Ctrl+Q</kbd> als Präfix):**
    *   `Ctrl+Q, Ctrl+S`: Zeilenanfang | `Ctrl+Q, Ctrl+D`: Zeilenende
    *   `Ctrl+Q, Ctrl+E`: Seitenanfang | `Ctrl+Q, Ctrl+X`: Seitenende
    *   `Ctrl+Q, Ctrl+R`: Textanfang | `Ctrl+Q, Ctrl+C`: Textende

### Formatierungsregeln (Auto-Clean)
NPEd sorgt automatisch für sauberen Code:
*   Überflüssige Leerzeichen am Zeilenende (*Trailing Spaces*) werden entfernt.
*   Leerzeilen am Dateiende werden gelöscht.
*   Die letzte Zeile endet **immer** mit einem Zeilenumbruch (`\n`).
*   Leere Dateien werden beim Speichern ignoriert.

### Wichtige IDE-Shortcuts
*   <kbd>Ctrl+O, Ctrl+F</kbd>: Datei formatieren (LSP-unterstützt).
*   <kbd>Ctrl+V</kbd>: Ansicht wechseln (z. B. von Text auf gerenderte Darstellung bei Markdown/HTML oder Funktionsliste bei C++).
*   <kbd>F10</kbd> / <kbd>F11</kbd> / <kbd>F12</kbd>: Go to Type Definition / Implementation / Definition.

## 3. KI-Agent

NPEd integriert KI-Modelle direkt in deinen Entwicklungs-Workflow.

### 3.1 KI-Modelle konfigurieren
Du kannst Modelle von Google (Gemini), Anthropic (Claude) und lokale Modelle via Ollama nutzen. Läuft ein Ollama-Server auf deinem Rechner, werden die verfügbaren Modelle automatisch erkannt und im AI-Panel unter "Models" gelistet. Für Cloud-Modelle ist in der Regel ein API-Key erforderlich.

### 3.2 MCP-Server (Model Context Protocol)
MCP ermöglicht es dem KI-Agenten, auf externe Werkzeuge und Daten zuzugreifen. Hier ist eine Übersicht empfohlener MCP-Server:

| Server | Zweck | Installation / Befehl |
| :--- | :--- | :--- |
| **Filesystem** | Lese-/Schreibzugriff auf Projektdateien | `rust-mcp-filesystem` |
| **GitHub** | GitHub-Integration | `npx -y @modelcontextprotocol/server-github` |
| **Git** | Lokale Git-Operationen | `pip install mcp-server-git` |
| **Websearch** | Websuche-Erweiterung | `npx websearch-mcp` |
| **Chrome DevTools** | Frontend-Debugging/Steuerung | `npx -y chrome-devtools-mcp@latest` |

*Hinweis: MCP-Server werden in der Konfigurationsdatei unter `mcpServers` registriert.*

### 3.3 Interne Tools & Sicherheit
Zusätzlich zu den MCP-Servern bietet NPEd eigene interne Tools an. Die Sicherheit steht dabei im Vordergrund:
*   **Dateizugriff:** KI-Modelle können nur auf Dateien innerhalb des aktuellen Projektverzeichnisses zugreifen. Schreibrechte sind optional pro Rolle konfigurierbar.
*   **Bash-Sandbox:** Das `bash`-Tool erlaubt Shell-Kommandos, läuft jedoch in einer geschützten Sandbox, um dein System abzusichern.

### 3.4 Sessions & Rollen
*   **Sessions:** Werden automatisch gespeichert und sind an ein spezifisches Modell gebunden. Du kannst bequem im AI-Panel zwischen verschiedenen Sessions wechseln.
*   **Rollen:** Hier definierst du System-Prompts für dein Modell und konfigurierst Berechtigungen (z.B. ob das Modell Dateien verändern darf).

## 4. Installation

### Schriftart
NPEd erfordert eine nichtproportionale Schriftart (*Monospace*). In den Einstellungen werden dir automatisch nur geeignete Fonts angeboten.

### Language Server
Für C/C++ ist `clangd` der Standard. Installiere ihn einfach über deinen Paketmanager:
`sudo apt install clangd`

### Tipp: Core Dumps für Debugging
Falls du unter Kubuntu/Ubuntu mit Core-Dumps arbeitest, musst du diese explizit aktivieren:
1. Deaktiviere Apport: `/etc/default/apport` auf `enabled=0` setzen.
2. Stoppe den Dienst: `sudo systemctl stop apport.service`
3. Deaktiviere den Dienst: `sudo systemctl disable apport.service`

## 5. Beispiele
Weitere Anwendungsbeispiele findest du unter [Examples](manual/examples.md).

## 6. Kommando-Referenz

Hier findest du eine kompakte Übersicht aller verfügbaren Kommandos und der zugehörigen Tastaturkürzel.

### Globale Kommandos
| Kommando | Beschreibung | Shortcut(s) |
| :--- | :--- | :--- |
| `CMD_SAVE` | Datei speichern | <kbd>Ctrl+K, Ctrl+S</kbd> |
| `CMD_SAVE_QUIT` | Speichern & Beenden | <kbd>F1</kbd> |
| `CMD_QUIT` | Beenden ohne Speichern | <kbd>Ctrl+K, Ctrl+Q</kbd>, <kbd>Shift+F1</kbd> |

### Command-Entry Funktionen (Escape-Modus)
*Nach drücken von `<Escape>` den Namen/Parameter eingeben und dann den Shortcut bestätigen.*

| Kommando | Beschreibung | Shortcut(s) |
| :--- | :--- | :--- |
| `CMD_ENTER_ADD_FILE` | Datei öffnen | <kbd>F3</kbd> |
| `CMD_ENTER_SEARCH` | Suche / Ersetzen | <kbd>F7</kbd> |
| `CMD_ENTER_CREATE_FUNCTION` | Neue C++ Funktion | <kbd>Ctrl+F</kbd> |
| `CMD_ENTER_GOTO_LINE` | Zu Zeile springen | <kbd>Ctrl+G</kbd> |

### Cursor-Steuerung
| Kommando | Beschreibung | Shortcut(s) |
| :--- | :--- | :--- |
| `CMD_CHAR_RIGHT` | Zeichen rechts | <kbd>Right</kbd>, <kbd>Ctrl+D</kbd> |
| `CMD_CHAR_LEFT` | Zeichen links | <kbd>Left</kbd>, <kbd>Ctrl+S</kbd> |
| `CMD_LINE_UP` | Zeile hoch | <kbd>Up</kbd>, <kbd>Ctrl+E</kbd> |
| `CMD_LINE_DOWN` | Zeile runter | <kbd>Down</kbd>, <kbd>Ctrl+X</kbd> |
| `CMD_WORD_LEFT` | Wort links | <kbd>Ctrl+A</kbd> |
| `CMD_WORD_RIGHT` | Wort rechts | <kbd>Ctrl+F</kbd> |
| `CMD_PAGE_UP` | Seite hoch | <kbd>PgUp</kbd>, <kbd>Ctrl+R</kbd> |
| `CMD_PAGE_DOWN` | Seite runter | <kbd>PgDown</kbd>, <kbd>Ctrl+C</kbd> |
| `CMD_FILE_BEGIN` | Textanfang | <kbd>Ctrl+Q, Ctrl+R</kbd> |
| `CMD_FILE_END` | Textende | <kbd>Ctrl+Q, Ctrl+C</kbd> |

### Textbearbeitung
| Kommando | Beschreibung | Shortcut(s) |
| :--- | :--- | :--- |
| `CMD_UNDO` | Rückgängig | <kbd>Ctrl+Z</kbd> |
| `CMD_REDO` | Wiederherstellen | <kbd>Shift+Ctrl+Z</kbd> |
| `CMD_PICK` | Kopieren (Pick) | <kbd>F8</kbd> |
| `CMD_PUT` | Einfügen (Put) | <kbd>F9</kbd> |
| `CMD_DELETE_LINE` | Zeile löschen (Cut) | <kbd>Ctrl+Y</kbd> |
| `CMD_INSERT_LINE` | Zeile einfügen | <kbd>Ctrl+N</kbd> |

### IDE & LSP Features
| Kommando | Beschreibung | Shortcut(s) |
| :--- | :--- | :--- |
| `CMD_FORMAT` | Code formatieren | <kbd>Ctrl+O, Ctrl+F</kbd> |
| `CMD_GOTO_DEFINITION` | Zur Definition springen | <kbd>F12</kbd> |
| `CMD_GOTO_IMPLEMENTATION`| Zur Implementierung | <kbd>F11</kbd> |
| `CMD_GOTO_TYPE_DEFINITION`| Zum Typ springen | <kbd>F10</kbd> |
| `CMD_RENAME` | Symbol umbenennen | <kbd>Ctrl+O, Ctrl+R</kbd> |
| `CMD_COMPLETIONS` | Autovervollständigung | <kbd>Ctrl+Tab</kbd>, <kbd>Shift+Tab</kbd> |
| `CMD_TOGGLE_AI` | AI Panel anzeigen | <kbd>Ctrl+I</kbd> |
| `CMD_TOGGLE_GIT` | Git Panel anzeigen | <kbd>Ctrl+O, Ctrl+G</kbd> |

*(Hinweis: Diese Liste enthält die wichtigsten Befehle. Weitere Funktionen können durch Kombinationen oder das Command-Entry-System erreicht werden.)*
