
## TODO

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
