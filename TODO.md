
## TODO

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
