## Current Status

NPed als Mini-IDE bedient in der vorliegenden Version nur meine
persönlichen Vorlieben und meinen Workflow und ist nicht einfach auf
andere Umgebungen anpassbar. Dazu müssten die Konfigurationsmöglichkeiten
erheblich erweitert werden.

Aber hey, dies ist eine C++ IDE, du bist also Programmierer und kannst den
Code direkt anpassen :-)

Die Details:

- Editor

  NPed wird mit NPed entwickelt, der Editor-Part ist fast vollständig
  und (für mich) "Feature-Complete".

- Language-Server

  - Die Implementation ist nicht ganz vollständig,
  - enthält wahrscheinlich noch Fehler
  - die Integration in den Workflow ist teilweise experimentell

- Code Formatierung

Die Code Formatierung nutzt clang-format.
Mein bevorzugter Code-Stil ist xxx (name vergessen).
Unglücklicherweise unterstütz clang-format genau den nicht. Der aktuelle
Hack nutzt eine spezielle clang-format Konfigurationsdatei mit einem
kleinen Postprozessor im Editor. Das ist der Grund, warum ein einfaches
austauschen der clang Konfiguration nicht das gewünschte Ergebnis bringen
wird. Dazu muss auch der Postprozessor Hack aus dem Editor entfernt
werden.

- Ai Integration

Ich nutzt Claude (Anthropic), Gemini (Google) und lokale LLM's über Ollama für
die Entwicklung dieses Editors sowie für andere Projekte. OpenAi ist ungetested.
Techniken zur Verkürzung der Chat Historie (dem Kontext) sind existentiell um den
Token-Verbrauch (und damit die Kosten) zu reduzieren und müssen noch weiter verfeinert
werden.

Manchmal verrennt sich die AI in einen Loop der z.Z. nur durch Beenden des Programms abgebrochen
werden kann. Hier ist noch weiterer Code gefragt, um solche Situationen zu erkennen und zu
behandeln.
