# Konfiguration
## Step1

Erstelle das Module ```Configuration```, welches es dem Benutzer ermöglicht,
die App zu Konfigurieren.

Realisiere die GUI durch eine Html-Seite, die auf dem Widget ConfigWebView dargestellt
wird.

Erstelle die GUI dynamisch aus einer JSON Datei:

```json
{
"titel": "Configuration",
"sections": [
      {
      "name": "General & GUI"
      },
      {
      "name": "Editor Shortcuts"
      },
      {
      "name": "Light Text Styles"
      },
      {
      "name": "Dark Text Styles"
      },
      {
      "name": "File Types"
      },
      {
      "name": "Language Servers"
      },
      {
      "name": "AI - Models"
      },
      {
      "name": "AI - Roles"
      }
      ]

```

Verwende für die Html-Seite die Bootstrap Bibliothek. Die Webseite soll lokal funktionieren,
darf also keine Web-Links enthalten.

Auf der obersten Ebene besteht die GUI aus "Sektions". Sektions haben Namen, die als
vertikale Liste in einem Panel auf der linken Seite der Html Seite dargestellt werden.
Der Benutzer kann eine Sektion auswählen, die dann als Panel auf der rechten Seite der
Html Seite dargestellt wird.

Erstelle neben einer Kopfzeile, die den text "titel"  darstellt, auch eine Fusszeile,
die drei Buttons enthält: "Reset", "Cancel" und "Save".
Verbinde die Buttons mit den entsprechenden Signals in ConfigWebView.

## Step 2
Implementiere die erste Sektion "General & GUI". Lies dazu die JSON Beschreibung der
Sektion und implementiere den Sektion Generator:

- [type] beschreibt den typ der Sektion.
   - [properties] Die Sektion besteht aus properties, die mit
   ```setProperty(const QString& name, const QVariant& value)``` geschrieben und mit
   ```QVariant getProperty(const QString& name) const``` gelesen werden.
- [sequence] beschreibt die Reihenfolge der darzustellenden Properties
- [xxxx] property aus der Liste [sequence]
   - [name] ist der Name des Properties wie er im getter/setter angegeben wird
   - [type] ist der Typ des Properties:
      - ["bool"] bool Type
      - ["font"] Font Family Name. Das Font Property wird als Combobox aller
       verfügbaren Fonts dargestellt.
   - [label] dies ist der UI Name des Properties.

## Step 3
Implementiere die zweite Sektion "Editor Shortcuts". Lies dazu die JSON Beschreibung der
Sektion und implementiere den Type "list". Dieser Typ konfiguriert eine Liste von
Objekten. Sie besteht auf der linken Seite aus einer vertikalen Liste von Elementnamen
und auf der recten Seite aus den zu konfigurierenden Werten, den List Elementen. Die
Liste besteht aus Werten vom Typ "listType". Der Elementname ist "listItemTitle".
Es werden auf der rechten Seite die List-Werte aus "listSequence" dargestellt. Werte mit
gesetztem "readOnly" sind nicht editierbar. Wenn die Liste selbst "readOnly" gesetzt hat
bedeutet dies, das der Benutzer keine Elemente löschen oder hinzufügen kann.

## Step 4
Implementiere nicht konstante Listen (Sektion "File Types"). Diese Listen bekommen
eine zusätzliche Fusszeile mit den Buttons "Add" und "Remove", um Elemente zu löschen
oder ein Element zuzufügen. Implementiere ausserdem den "type" "int" mit den
editier-Einstränkungen "min" und "max"
