## TODO

### SelectionMode CharSelect
- [x] implement the mising SelectionMode "CharSelect". Its the most common selection
   mode and has a start line/column and an end line/column. All characters in between are
   selected.

### Clean Git
- [x] Die Editor() Klasse enthält den String "currentBranchName" wenn in initProjekt() festgestellt
      wurde, das wir ein Projekt gefunden haben welches von GIT verwaltet wird. Versuche in dem
      Fall festzustellen, ob der aktuelle Git Branch "clean" ist, dh. keine veränd Dateien
      enthält. Wenn der Branch clean ist, dann zeige das current branch label in der Statuszeile unter
      dem Edit-Window in hellgrün, ansonsten in einem hellen gelb.

### Feature: Ai Dashboard
- [x] Extend the Toolbar at the bottom of the AI Panel into a Dashboard so that
      it is possible to add more functionality. Move the current content of the
      toolbar into the new dashboard and organize them in two rows.

### Feature: Language Server Konfiguration
- [x] Ändere den Panel-Basierten Ansatz zur Konfiguration der Language Server
      wie folgt:
      - die Language Server werden auf der linken Seite als Liste dargestellt.
      - Auf der rechten Seite wird ein Language Server im Detail dargestellt.
      - Wird links ein Server ausgewählt, dann werden Details rechts
        aktualisiert

### Feature: Dateityp Konfiguration
- [x] Ändere den Panel-Basierten Ansatz zur Konfiguration der File Types
      wie folgt:
      - die Dateitypen werden auf der linken Seite als Liste dargestellt.
      - Auf der rechten Seite wird ein Dateityp im Detail dargestellt.
      - Wird links ein Dateityp ausgewählt, dann werden Details rechts
        aktualisiert

### Feature: move filter
- [x] the flags "filterToolMessages" and "filterThoughts" are not
      properties of Model() anymore but only properties of Agent(),
      So this flags do not depend on Model anymore.
- [x] remove the flags from Model
- [x] add the flags to Model
- [x] adapt configuration for this flags
- [x] adapt buttons in toolbar below the chat log

### Feature: Model Konfiguration
- [x] Ändere den Panel-Basierten Ansatz zur Konfiguration der Ai-Modell
      wie folgt:
      - die Modellnamen werden auf der linken Seite als Liste dargestellt.
      - Auf der rechten Seite wird ein Modell im Detail dargestellt.
      - Wird links ein Modell ausgewählt, dann werden Details rechts
        aktualisiert

### Feature: Navigation Buttons for WebView
- [x] Implement four buttons on top of the WebView widget in the main view.
      Back, Forward, Reload, Home
- [x] Implement the necessary functions and connect them to the buttons
- [x] Createor or Search and Download svg icon images for the buttons and
      style them for dark and light theme

### Bug: Links in gerenderten Markdown Seiten
- [x] Wenn sich eine Seite im View Mode Render-View befindet und ich auf einen
      Link clicke, dann wird die neue Seite im Text Mode dargestellt statt im
      WebView als html angezeigt zu werden

### Bug Kontext Umschalten
- [x] Wenn sich eine Seite im View Mode Render-View befindet, d.h. z.B. ein
      Markdown Text gerendert in einem WebView dargestellt wird, dann kann
      der Kontext im Tab-Bar umgeschaltet werden, aber der dargestellte Inhalt
      verändert sich nicht.

### Check Prompt Verkürzung

- [x] Checke, ob beim Zusammenbau des Prompts bei einer verkürzten Historie diese immer auch
      die erste Benutzeranfrage enthält (Wobei wir hoffen, das die nicht nur ein einfaches "Hallo"
      enthält sondern die Aufgaben/Problembeschreibung). Checke dies für alle Modell API's.

### System Prompt

- [x] Erzeuge separate System Prompts (Manifest) für Build/Plan Mode.
- [x] Versuche im Plan Modus den System Prompt aud der Datei agents-plan.md und
      im "Build" Modus aus der Datei agents-build.md zu lesen. Sind die Dateien nicht
      vorhanden, dann nimm die eingebauten Prompts.
- [x] Merke dir, wenn du den System Prompt aus einer Datei gelesen hast, so dass
      nur einmal gelesen wird.

### Tool-Sets

- [x] Erzeuge separate Tool-Sets für Build/Plan Mode. Übergib im Prompt nur die Tools
      des jeweiligen Modes.

### Image drop
- [x] intercept the image drop event of the prompt input widget, if necessary create an
  derived class for this.
- [x] Handle the image drop like the screenshot is handled: save data into _pendingScreenshotBase64
  and notify the user with a screenshotIconLabel in the dataPanel

### Screen Shot

- [x] Erstelle eine neue Editor Funktion "CMD_SCREENSHOT" mit dem default Key Binding "Ctrl+O, Ctrl+O, Ctrl+P"
- [x] Erstelle eine Editor() Methode screenshot(), die ein Image des aktuellen Editor-Bildschirm Inhalts
  erstellt und als Datei "screenshot-nn.jpg" im jpeg format im Projekt Rootverzeichnis ablegt. "nn" ist
  dabei eine fortlaufende Nummer die hochgezählt wird um keine vorhandenen shots zu überschreiben.
  Benutze die Qt Klasse QScreen

### Datenanzeige

- [x] Erstelle eine schmales vertikales Fenster zur Aufname von Icons links neben der Prompt Eingabe im Ai-Window.
  Immer wenn ein Image (z.B. ein screenshot) für den nächsten Prompt zur verfügung steht,
  soll hier ein "Picture" Icon erscheinen.

### screen shot

- [x] Ergänze die Tool Leiste unter dem Ai panel um einen button, der einen screen shot auslösen soll und
  der ein kamera icon zeigt.

- [x] implementiere eine screen shot funktionalität für linux und wayland. Benutze dazu d-bus und das
  xdg desktop portal

- [x] Füge das bild dem aktuellen promt an das ai-modell hinzu. Implementiere dazu die umwandlung in ein
  passendes format sowie die erstellung eines prompts für gemini anthropic und ollama


### ask_user tool
- Implementiere für die Interaktion von Ai-Modell und User das tool "ask_user" mit einem Parameter
  "question", um dem
  Modell eine Möglichkeit zu geben, Rückfragen zu stellen.

### Umbau Ai Panel
- [x] Verschiebe die tool bar im Titel nach unten unter die promt Eingabe.
- [x] Implementiere die beiden flags im pulldownmenü "Filter Tool Messages" und "Filter Thoughts" als zwei
  Buttons in der Tool Bar die jeweils nur ein Icon zeigen: ein "Tool" icon sowie ein "Brain" icon.
  Das Pulldown Menu ist dann leer und kann entfernt werden.
- [x] verschiebe den "Run" Button links neben der Prompt Eingabe auch in die tool bar und plaziere ihn ganz
  links

### GIT current branch label
- [x] Erweitere die Editor() Klasse um das Status-Flag "hasGit". Dieses Flag wird in initProjekt
gesetzt um anzuzeigen, das das aktuelle Project von GIT verwaltet wird.
Der Check auf vorhandensein von GIT wird nur durchgeführt, wenn ein Projekt gefunden wurde, d.h. wenn
Editor->projectMode true ist.

- [x] Erweitere die Editor() Klasse um den string "currentBranchName". Wenn in initProjekt() festgestellt wurde,
das wir ein Projekt gefunden haben welches von GIT verwaltet wird, dann initialisiere den string
mit dem aktuelle git branch.

- [x] Füge in die Statuszeile des Editor Hauptfensters links neben dem Pfad der aktuellen Datei ein
Label ein welches "currentBranchName" zeigt.
Das Label wird nur angezeigt, wenn Editor->projectMode true ist.

### Configuration erweitern

Erweitere das Konfigurationspanel in qml/ConfigDialog.qml um alle noch nicht behandelten Felder in Model
configurieren zu können.
Erstelle auch die Verbindung zu Model im c++ Teil des editors.

### Check Anthropic Implementation

Analysiere im Detail die Implementation der Anthropic Model Integration in
der Datei src/anthropic.cpp. Suche Fehler und mache Verbesserungsvorschläge.
Checke auch, ob die Funktionalität der Anthropic API voll abedeckt wird.

### Option Pulldown Menü

Füge rechts neben dem "Build"/"Plan" mode button einen weiteren Button für ein
Option-Menü ein. Der Button soll drei vertikale punkte oder kurze horizontale
Striche zeigen.

### Filter Tool Messages
Füge in das neue Options-Menü einen Toggle Switch ein, der mit einer neuen Variablen
bool Model::filterToolMessages verbunden ist. Die Variable soll persistent sein
und mit Agent::saveStatus() gespeichert und mit Agent::loadStatus() geladen werden.
D.h. sie ist Teil des Chat Log Files.

Wenn filterToolMessages true ist, dann sollen Model Tool Requests sowie die Antwort
des Agenten an das Model nicht mehr im ChatDisplay angezeigt werden.


### Do not truncate chat history:

Currently, the entire history is always transmitted to the AI model.
To prevent the context from becoming too large, the list is implemented as a "sliding window",
where old entries are removed after exceeding a limit.
However, I would like to keep the complete list, save it in the session file,
and only transmit a maximum of `HistoryManager::maxEntries` entries when preparing the AI context.

Procedure:
- Extend `HistoryManager` with an `activeEntries` variable.
- Write the `activeEntries` variable into the session file.
- `HistoryManager::trim()` should no longer delete entries but adjust `activeEntries`.
- Increment `activeEntries` after every `HistoryManager::addRequest(...)` and `History::addResult(...)`.
- Add a method `getActiveEntries()` to `HistoryManager` that returns the list of active entries (activeEntries) as JSON.
- In `LLMClient::prompt()` (in all derived classes), replace the call to `historyManager->data()` with the call to `getActiveEntries()`.


### Optimize session log writing:

Currently, the session log is always completely rewritten. Change the logic so that whenever the session log in `HistoryManager` grows, the new data is only appended to the file.
