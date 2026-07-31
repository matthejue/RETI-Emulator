 [![asciicast](https://asciinema.org/a/693086.svg)](https://asciinema.org/a/693086)

# Was ist der RETI-Emulator?
Eigentlich ist der RETI-Emulator ein **RETI-Emulator**, **RETI-Assembler**, **RETI-Debugger** und **Visualizer** für die Speicherinhalte der Peripheriegeräte der RETI während der Ausführung

- **Interpreter:** Ein Interpreter führt Anweisungen einer Programmiersprache direkt aus, ohne sie vorher in Maschinencode zu übersetzen. Die Ausführung erfolgt zeilenweise oder schrittweise.
  - *Beispiel:* Python-Interpreter.
- **Assembler:** Ein Assembler übersetzt Code in Assemblersprache in ausführbaren Maschinencode, der von einer dafür spezfischen CPU verstanden wird.
  - *Beispiel:* Übersetzung von MOV AX, BX in Maschinencode für eine spezifische Architektur.
- **Emulator:** Ein Emulator ahmt die Funktionalität eines Systems (z.B. Hardware oder Software) nach, sodass Programme für das originale System unverändert darauf laufen können.
  - *Beispiele:* QEMU, der die Funktionalität verschiedener CPU-Architekturen nachahmt oder SNES-Emulatoren, welche alte Konsolenspiele auf einem PC ausführen.
- **Debugger:** Ein Debugger ist ein Tool, das die Ausführung eines Programms erlaubt, mit dem Ziel Fehler (Bugs) zu finden, zu analysieren und zu beheben. Es bietet Funktionen wie Breakpoints, Schritt-für-Schritt-Ausführung und Inspektion von Variablen, Registerwerten und Speicherbereichen. Das Programm kann in verschiedenen Formen spezifiziert sein, einschließlich Quellcode, Bytecode oder ausführbarem Maschinencode.
  - *Beispiel:* GDB (GNU Debugger) für C/C++-Programme.
- **Visualizer:** Ein Visualizer stellt Daten, Abläufe oder Systeme visuell dar, um deren Struktur, Verhalten oder Ergebnisse leichter verständlich zu machen.
  - *Beispiel:* Ein Graph-Visualizer, der Knoten und Verbindungen eines Netzwerks grafisch darstellt.
- **Simulator:** Ein Simulator modelliert ein System oder dessen Verhalten auf höherer Abstraktionsebene, um Analysen, Tests oder Training durchzuführen, ohne jede Funktionalität notwendigerweise exakt nachzubilden.
  - *Beispiel:* Flugsimulator für Pilotentraining.

Der RETI-Emulators hat einmal das Ziel, dass darauf eines Tages ein minimales Betriebssystem läuft, dass in PicoC geschrieben ist. Ein weiteres Ziel des RETI-Emulators ist es im Übungsbetrieb die Studenten beim Schreiben von RETI Programmen zu unterstützen, daher zeigt der RETI-Emulator auch Fehlermeldungen an und hat einen stärkeren Fokus auf Bugtesting und Features, welche die Verwendung für Studenten angenehmer gestalten.

# Übersicht
## Kommandozeilenoptionen
- `-r ram_size`: Setzt die Anzahl adressierbarer 32-Bit-Wörter im SRAM (Stadardwert: `2^16=65536`)
- `-d`: Zeigt das Ncurses Debug TUI an
- `-K`: Hält die Debug-TUI nach dem abschließenden `JUMP 0` geöffnet, bis sie mit `q` beendet wird
- `-f file_dir`: Gibt an, wo die Datei `sram.bin` erzeugt werden soll
- `-e eprom_prgrm_path`: Parst und lädt Eprom-Startprogramm aus Datei, die über Dateipfad gefunden werden kann
- `-i isrs_prgrm_path`: Parst und lädt Interrupt-Service Routinen aus Datei, die über Dateipfad gefunden werden kann
- `-S sections_path`: Verwendet die angegebene `.sections`-Datei anstelle von `<program>.sections`
- `-D debuginfo_path`: Verwendet die angegebene `.debuginfo`-Datei anstelle von `<program>.debuginfo`
- `-w max_waiting_instrs`: Setzt raximale Wartezeit der UART für das Senden und Empfangen von Daten (Anzahle Befehle)
- `-t`: Aktiviert Testmode für Systemtests
- `-m`: Liest Eingaben aus Kommentar `# input: ...` raus
- `-c`: Zeigt Quellkommentare im Debugmode an
- `-v`: Zeigt zusäztliche Informationen an (Welche Kommandozeilenoptionen aktiviert sind)
- `-b`: Aktiviert die Darstellung von Dezimalzahlen in Binärdarstellung
- `-E`: Aktiviere Erweiterte Funktionalitäten (Hilfslinien um unnötige Leerzeichen sichtbar zu machen)
- `-a`, `--assemble`: Assembliert die `.reti`-Datei in eine gleichnamige `.bin`-Datei und schreibt zuerst `codesegment_start`, `datasegment_start`, `heap_start` und `stack_start` aus der gleichnamigen `.sections`-Datei, danach die Maschinenwörter binär kodiert wie in `sram.bin`
- `-u`: Wertet Werte im Datensegment in Zweierkomplementdarstellung oder Betrag-Vorzeichendarstellug aus
- `-I timer_interrupt_interval`: Das Zeitinterval (Anzahl ausgeführte Befehle) zwischen Timer Interrupts; `0` deaktiviert den Timer Interrupt
- `-O`: Startet mit einer synthetischen aktiven ISR für Betriebssysteme, deren erster Prozess per `RTI` gestartet wird
- `-h`: Zeigt Verwendungshinweise an

<!-- - `-p page_size`: Setzt Seitengröße (Standardwert: `2^12=4096`) -->
<!-- - `-r radius`: Setzt Radius an Speicherzellen, die in der Legacy Debug TUI um einen observierten Addresspointer herum angezeigt werden sollen -->
<!-- - `-l`: Zeigt das Legacy Debug Interface anstelle -->

## TUI Aktionen
- `n` ext
- `c` ontinue bis zu Breakpoint `INT 3`
- `E` nter again
  - Unterbricht eine mit `c` gestartete kontinuierliche Ausführung an der aktuellen Programmadresse und kehrt zum schrittweisen Debuggen zurück
- `r` estart
- `s` tep into isr
- `f` inalize isr
- `a` ssign (Adresse oder Register)
  - Decision Menu und jederzeit mit `q` abbrechbar
- `q` uit
- `o` ther actions
  - Wechselt in der Infobox zyklisch zur nächsten Hilfeseite
- `t` rigger isr
  - Löst die aktuell für die Custom-Aktion ausgewählte ISR aus
- `e` xchange isr
  - Wechselt zyklisch zwischen allen per `-i` geladenen ISR-Nummern, die durch `t` ausgelöst werden können
- `V` iew terminal
  - Wechselt aus der Debug-TUI in die UART-Terminalansicht im selben Terminal; `Escape` kehrt zur Debug-TUI zurück

# Installation und Updates
## Installation auf Linux Systemen, auf denen Kompilierung nicht möglich ist über eine statische Binary
```bash
git clone -b main https://github.com/matthejue/RETI-Emulator.git ~/RETI-Emulator --depth 1
cd ~/RETI-Emulator
make install-linux-local
```

## Installation auf Linux Systemen, auf denen Kompilierung möglich ist durch eben Kompilierung
```bash
git clone -b main https://github.com/matthejue/RETI-Emulator.git ~/RETI-Emulator --depth 1
cd ~/RETI-Emulator
make install-linux-global
```

## Deinstallation auf Linux Systemen, wenn vorher lokal installiert wurde
```bash
cd ~/RETI-Emulator
make uninstall-linux-local
```

## Deinstallation auf Linux Systemen, wenn vorher global installiert wurde
```bash
cd ~/RETI-Emulator
make uninstall-linux-global
```

## Updaten auf Linux Systemen, wenn vorher lokal installiert wurde
```bash
cd ~/RETI-Emulator
make update-linux-local
```

## Updaten auf Linux Systemen, wenn vorher global installiert wurde
```bash
cd ~/RETI-Emulator
make update-linux-global
```

> Bitte lokale und globale Installationen nicht mischen, beim Wechsel zur jeweils anderen Installationsart vorher eine Deinstallation für die zuvor verwendete Installationsart durchführen.


# Verwendung
Der RETI-Emulator ist dazu in der Lage, die RETI-Befehle eines in einer `.reti`-Datei angebenen RETI-Programms zu interpretieren. D.h. er kann das RETI-Programm **ausführen**, indem er die RETI-Befehle aus einer Datei `prgrm.reti` herausliest und in den simulierten SRAM schreibt und mithilfe eines autogenerierten EPROM-Startprogramms, dass zu Beginn ausgeführt wird an den Start dieses Programmes springt. Zum Ausführen eines Programmes muss der RETI-Emulator mit dem Pfad zum RETI-Programm als Argument aufgerufen werden, z.B.:

```bash
$ reti_emulator ./prgrm.reti
```

Das RETI-Programm `prgrm.reti`, das im Folgenden als Beispiel verwendet wird, sieht dabei wie folgt aus:
```
# input: 16909060 3
INT 3 # just a breakpoint, use c in debug mode -d to directly jump here
INT 2
MOVE ACC IN1
INT 2
MULT ACC IN1
INT 3
INT 0
JUMP 0
```

> Nicht vergessen `JUMP 0` ans Ende des Programmes zu setzen, sonst wird einfach weiter ausgeführt was danach im SRAM steht bzw. als was für Instructions der Speicherinhalt auf den der `PC` in dem Moment zeigt interpretiert wird.

RETI-Emulator speichert alle Memory-Inhalte des SRAM in einer Datei `sram.bin` ab. Die aus der Datei `prgrm.reti` geparsten Assembly-Befehle werden realitätsgetreu als 32-Bit (4 Byte)  Maschinenbefehle in einer Datei `sram.bin` abgespeichert, weil dies am speichereffizientesten ist und die RETI möglichst realistisch simuliert werden soll.

> Die Datei `sram.bin` ist zwar in der Ausgabe von `ls -lh ./sram.bin` 256KB groß (mit dem default Wert von `-r 65536`, also $2^{16}$), aber in Wirklichkeit verbraucht die Datei bei einem kleinen RETI-Programm nur wenige KibiBytes, weil Sparse Files verwendet werden. Das sieht man z.B. mit `du -h ./sram.bin`.

> *Tipp:* Mittels `-f /tmp` (files) lässt sich das `/tmp` Verzeichnis unter Linux für die Speicherung von `sram.bin` nutzen. Das Verzeichnis `\tmp` ist häufig als **tmpfs**-Partition, welche im Arbeitsspeicher gemounted ist umgesetzt. Dadurch existiert der Inhalt des Verzeichnis nach dem Herunterfahren nicht mehr und das Verzeichnis, indem der RETI-Emulator ausgeführt wird, wird nicht mit unnützen Dateien vollgemüllt.

## Direkte Speicherwerte in `.reti`-Dateien
Neben RETI-Instruktionen können in einer `.reti`-Datei auch direkte Speicherwerte stehen. Solche Werte werden nicht assembliert, sondern unverändert als 32-Bit-Wort in die nächste SRAM-Zelle geschrieben.

Unterstützt werden dezimale Zahlen und einzelne ASCII-Zeichen in einfachen Anführungszeichen:

```reti
LOADI ACC 1
42
-1
'e'
'!'
JUMP 0
```

In diesem Beispiel werden `42`, `-1`, der ASCII-Wert von `e` (`101`) und der ASCII-Wert von `!` (`33`) direkt in aufeinanderfolgende Speicherzellen geschrieben. Das ist besonders nützlich für Datensegmente, Strings oder vom Compiler erzeugte Speicherinhalte, die nicht als RETI-Instruktionen interpretiert werden sollen.

## Abschnittsdateien für Compiler-Ausgaben
Wenn zu einer Datei `program.reti` eine Datei `program.sections` existiert, liest der Emulator diese JSON-Datei ein und verwendet sie, um die `.reti`-Datei in Interrupt-Service-Routinen, Codesegment und Datensegment aufzuteilen. Mit `-S sections_path` kann stattdessen eine Abschnittsdatei mit anderem Pfad angegeben werden.

Beispiel für `program.sections`:

```json
{
  "codesegment_start": 40,
  "datasegment_start": 180
}
```

Die Adressen sind nullbasiert und beziehen sich auf die geparsten Speicherwörter bzw. Instruktionen der `.reti`-Datei:

- `0` bis `39`: Interrupt-Vektor-Tabelle und Interrupt-Service-Routinen
- `40` bis `179`: Codesegment
- `180` bis Dateiende: Datensegment

Das Codesegment wird wie normale RETI-Instruktionen angezeigt. Das Datensegment wird als rohe Speicherwerte angezeigt, also werden die Inhalte dort nicht in RETI-Instruktionen zurückübersetzt. Direkte Zahlen und ASCII-Zeichen wie `'e'` sind dafür gedacht, in diesem Bereich Daten abzulegen.

Wenn eine gleichnamige `program.sections`-Datei zusätzlich `stack_start` enthält, beeinflusst dieser Wert auch das automatisch erzeugte EPROM-Startprogramm. `stack_start: -1` behält das bisherige Verhalten bei und setzt den Stackpointer an das Ende des SRAM. Jeder andere `stack_start`-Wert setzt den Stackpointer auf `stack_start` plus SRAM-Adresskonstante.

Wenn nur ein EPROM-Startprogramm mittels `-e eprom.reti` geladen wird, liest der Debugger optional `eprom.sections` aus demselben Verzeichnis. Diese Datei beschreibt dann das SRAM-Layout, das das EPROM-Startprogramm lädt. Ohne diese Datei zeigt der Debugger SRAM-Inhalte nur als Dezimalwerte bzw. mit `-b` als Binärwerte an.

### Interrupt Service Routinen spezifizieren
Mithilfe der Kommandozeilenoption `-i` (isr code) ist der RETI-Emulator in der Lage die RETI-Befehle für **Interrupt-Service-Routinen** aus einer Datei `interrupt_service_routines.reti` herauszulesen und an den Anfang des simulierten SRAM, vor das geladene Programm aus `program.reti` zu schreiben. Mithilfe von `INT i` kann wie in der Vorlesung erklärt an den Anfang jeder dieser Interrupt-Service-Routinen `i` gesprungen werden. Mittels `RTI` kann am Ende einer Interrupt-Service-Routine wieder an die nächste Stelle im ursprünglichen Programm zurückgesprungen werden, an der dieses mittels `INT i` unterbrochen wurde. 

Falls eine `.sections`-Datei existiert, gibt es zwei Fälle:

- Mit `-i`: Die Interrupt-Service-Routinen werden aus der über `-i` angegebenen Datei geladen. Der ISR-Abschnitt am Anfang der `.reti`-Datei wird übersprungen.
- Ohne `-i`: Der ISR-Abschnitt am Anfang der `.reti`-Datei wird als Interrupt-Vektor-Tabelle und Interrupt-Service-Routinen geladen.

Damit kann eine Compiler-Ausgabe entweder vollständig eigenständig sein oder mit einer explizit angegebenen ISR-Datei kombiniert werden.

Die Interrupt-Vektor-Tabelle besteht aus rohen Zahlenwerten am Anfang des ISR-Bereichs. Jeder Eintrag enthält die Startadresse der zugehörigen ISR relativ zum Anfang des SRAM-Bereichs; beim Sprung in die ISR ergänzt der Emulator automatisch die SRAM-Konstante. `INT 0` verwendet also den ersten Zahlenwert, `INT 1` den zweiten usw. Gültige ISR-Nummern sind `0` bis `254`; der Wert `255` ist intern für "keine ISR zugewiesen" reserviert.

Die Zuordnung von Hardware-Interrupt-Signalleitungen zu ISRs liegt nicht in der Vektortabelle, sondern im speicherabgebildeten Interrupt-Controller im bisherigen UART-Speicherbereich. Nach 3 UART-Zellen folgen 3 Zellen für `signal line -> isr` und danach 3 Zellen für `signal line -> priority`. In den ISR-Zellen bedeutet `255`, dass der Signalleitung keine ISR zugeordnet ist. Die Priorität ist ein 8-Bit-Wert, größere Werte haben höhere Priorität. Es gibt Signal-Line `0` (`INTTIMER`), Signal-Line `1` (`CUSTOM`) und Signal-Line `2` (`UART`).

Die Peripherie-Zelle `9` enthält das Timer-Interrupt-Intervall. Der Standardwert ist `0`, wodurch der Timer Interrupt deaktiviert ist. `-I <wert>` schreibt diesen Wert beim Start in die Zelle; jeder Wert größer `0` aktiviert den Timer Interrupt mit diesem Intervall. Ein Betriebssystem kann die Zelle ebenfalls beschreiben, um den Timer Interrupt zur Laufzeit zu aktivieren, zu deaktivieren oder das Intervall zu ändern.

Optional kann der Interrupt-Controller beim Start mit `-C config_file` vorbelegt werden. Die Datei enthält eine Zeile pro ISR-Index im Format `<priority> <device>`, zum Beispiel `2 INTTIMER`, `1 CUSTOM` oder `3 UART`; `-` bedeutet keine Zuordnung.

Eine Datei mit Interrupt Service Routinen, zum Beispiel `isrs.reti`, kann so aufgebaut sein:

```reti
# Interrupt-Vektor-Tabelle
5
19
33
47
51

# == INT 0 ==
# Interrupt Service Routine für Software-Interrupt 0
...
RTI

# == INT 1 ==
# Interrupt Service Routine für Software-Interrupt 1
...
RTI

# == INT 2 ==
# Interrupt Service Routine für Software-Interrupt 2
...
RTI

# == CUSTOM ==
# Interrupt Service Routine für den durch 't' im TUI ausgelösten Interrupt
...
RTI

# == INTTIMER ==
# Interrupt Service Routine für den automatischen Timer-Interrupt
...
RTI
```

Die Datei `isrs.reti` beginnt also mit den rohen Adress-Einträgen der Interrupt-Vektor-Tabelle. Danach folgen die eigentlichen Interrupt Service Routinen im selben File. Jede ISR sollte üblicherweise mit `RTI` enden.

## Debugging
Mittels der Kommandozeilenoption `-d` (debug mode) ist der RETI-Emulator in der Lage das Programm zu **debuggen**, d.h. er zeigt die Speicher- und Registerinhalte nach Ausführung eines jeden Befehls an. Zwischen diesen kann der Benutzer sich mittels `n` (`n`ext) und dann `Enter` forwärts bewegen. Wird `INT 3` in das RETI-Programm geschrieben stellt dies einen Breakpoint dar, wobei zum jeweils nächsten mittels `c` (`c`ontinue) und dann `Enter` gesprungen werden kann. Während dieser kontinuierlichen Ausführung ersetzt die Infobox `(c)ontinue` durch `(E)nter again, (V)iew terminal`. `E` unterbricht die Ausführung an der aktuellen Programmadresse und ermöglicht wieder alle schrittweisen Debug-Aktionen. In der untersten Zeile des Text-User-Interfaces (TUI) stehen die Aktionen, die Sie in diesem Debug-Modus ausführen können.

Die Infobox besitzt mehrere Hilfeseiten, zwischen denen mit `o` gewechselt werden kann. Auf der zweiten Hilfeseite zeigt der Eintrag `(T)rigger isr <num>` immer die ISR-Nummer an, die aktuell durch Drücken von `T` ausgelöst wird. Mit `e` (`e`xchange isr) kann zwischen allen über `-i` geladenen ISR-Nummern zyklisch gewechselt werden. Mit `t` (`t`ranscode) wechselt die Anzeige sichtbarer SRAM-Werte zwischen Zahl, RETI-Instruktion und ASCII-Zeichen, sofern der jeweilige Wert so dekodiert werden kann.

Auf der dritten Hilfeseite wechselt `V` (`V`iew terminal) aus der Ncurses-TUI in eine UART-Terminalansicht im selben aufrufenden Terminal. Die Ansicht zeigt zuerst die gesamte seit dem Emulatorstart aufgezeichnete UART-Terminalausgabe und danach neue Ausgaben direkt an. `Escape` kehrt zur Debug-TUI zurück und wird nicht als UART-Eingabe übertragen. Wird `V` im angehaltenen Debugger geöffnet, bleibt die Programmausführung angehalten; die Ansicht zeigt nur die bisherige Ausgabe, bis `Escape` zum schrittweisen Debuggen zurückkehrt. `V` kann außerdem während einer mit `c` gestarteten Ausführung gedrückt werden. In diesem Fall läuft das Programm auch in der Terminalansicht weiter, Tastendrücke werden als UART-Eingaben verarbeitet und lösen UART-Hardwareinterrupts aus. Nach `Escape` wird die Debug-TUI vollständig neu gezeichnet, die Ausführung läuft weiter und die Infobox bietet `(E)nter again` sowie `(V)iew terminal` an.

Mit `-c` werden zusätzlich Quellkommentare im Debugmode angezeigt. Dazu werden Kommentare beim Parsen in einer internen Struktur mit Ziel-Speicherbereich, referenziertem Instruktionsindex und Anzeigeposition gespeichert. Beim Anzeigen einer Instruktion wird geprüft, welche gespeicherten Kommentare diesem Instruktionsindex im aktuellen Speicherbereich zugeordnet sind, und diese werden dann vor oder nach der Instruktion ausgegeben.

Mithilfe sogenannter **Watchbojects** wird ein mittels `-r i` (radius) bestimmter Radius von `i` (default ist 2) sichtbaren Instructions bzw. Speicherinhalten über und unter einer von diesem Watchobject betrachteten Speicheradresse angezeigt. Die Speicheradresse ist hierbei entweder über den Inhalt eines vorher dem Watchobject zugewiesenen Registers bestimmt oder einfach direkt durch eine vorher an das Watchobject zugewiesene Speicheradresse. Die verschiedenen verfügbaren Wachpointer in dem TUI für das Debuggen sind `ew` (EPROM Watchobject), `swc` (SRAM Watchobject für das Codesegment), `swd` (SRAM Watchobject für das Datensegment) und `sws` (SRAM Watchobject für den Stack). Die Zuweisung einer Speicheradresse oder eines Registers erfolgt dabei über das Kommando `a` (`a`ssign), z.B. in Form von `a<enter>ew<enter>10`, `a<enter>sws<enter>BAF` usw.

<!-- Damit die Studenten sich immer darauf verlassen können, dass die Kernfunktionalitäten des RETI-Intepreters mit jedem Release während des Semesters gleich bleiben, müssen **neue Features**, welche diese zuerst etablierten Kernfunktionalitäten brechen erst mit `-E` (extended features) **aktiviert** werden. -->

<!-- Momentan wird mit `-E` nur aktiviert, dass eine beliebige Interrupt Service Routine `INT i`, wenn der `PC` auf diese zeigt bei `n` direkt komplett ausgeführt wird. Wenn allerdings der `PC` auf `INT i` zeigt und das Kommando `s` (step into) ausgeführt wird, dann wird in die Interrupt Service Routine gesprungen und diese Schritt für Schritt ausgeführt. `s` funktioniert also so, wie man es bei üblichen Debuggern von Funktionsaufrufen kennt, nur hier für Interrupt Service Routinen. -->

> *Tipp:* Mit der Kommandozeilenoption `-b` (binary) werden alle Registerinhalte, Speicherinhalte und Immediates im Binärsystem angezeigt, damit lässt sich beim debuggen z.B. leichter Shiften nachvollziehen.

> *Tipp:* Um beim Debugging direkt zur Startadresse Ihres in den SRAM geladenen Programmes zu springen setzen man am besten einen Breakpoint `INT 3` an den Anfang des Programmes und führen dann direkt am Anfang nach Ausführen von `reti_emulator -d prgrm.reti` das Kommando `c` aus.

## UART
*Die Kommunikation mit der UART ist wie folgt umgesetzt:*
- Die ersten 3 Zellen im Peripherie-Speicherbereich sind die UART-Register: R0 ist das Senderegister, R1 ist das Empfangsregister und R2 ist das Statusregister. In R2 bedeutet Bit `b0`, dass die UART wieder ein Sendebyte annehmen kann, und Bit `b1`, dass ein neues Empfangsbyte in R1 bereitliegt.
- Die UART überträgt nur einzelne 8-Bit-Bytes. Es gibt kein Datentyp-Byte und keine Sonderbehandlung für Zahlen oder Strings. ASCII-Zeichen werden als ihr 8-Bit-ASCII-Code übertragen.
- Zum Senden schreibt das RETI-Programm ein Byte nach R0 und setzt danach `b0` in R2 auf `0`. Nach der simulierten UART-Wartezeit setzt der Emulator `b0` wieder auf `1`; dann wurde dieses Byte vom simulierten Ausgabegerät übernommen und als ASCII-Zeichen ausgegeben.
- Zum Empfangen setzt das RETI-Programm `b1` in R2 auf `0`, sobald es bereit für das nächste Byte ist. Nach der simulierten UART-Wartezeit schreibt der Emulator das nächste ASCII-Byte nach R1 und setzt `b1` wieder auf `1`; das Programm sollte R1 lesen, bevor es das nächste Byte anfordert.
- Ohne `-d` ist das aufrufende Terminal immer als UART-Terminal aktiv. Jeder Tastendruck wird als einzelnes 8-Bit-Byte nach R1 geschrieben, setzt `b1` und löst die Hardware-Signalleitung `UART` aus. Solange der vorherige UART-Interrupt noch aussteht, werden weitere Tasten gepuffert. Normale UART-Ausgaben erscheinen direkt auf `stdout`.
- Mit `-d` bietet `(V)iew terminal` dieselbe UART-Ein- und -Ausgabe im aufrufenden Terminal. `Escape` kehrt hier zur Debug-TUI zurück und wird nicht nach R1 übertragen.
- Wenn kein gepufferter Input mehr vorhanden ist, fragt der Emulator in einer Input-Box nach UART-Eingabe. Mehrere eingegebene Zeichen werden als Buffer gespeichert und danach byteweise verbraucht. Eine leere Eingabe entspricht `\n`; alternativ können `\n` und `\t` als Escape-Sequenzen eingegeben werden.
- Mit `-m` können Eingaben aus dem Kommentar `# input: ...` gelesen werden. Ein einzelnes Trennleerzeichen oder Tab nach `input:` wird übersprungen; der restliche Text wird als Zeichenbuffer übernommen, inklusive weiterer Leerzeichen, und danach Zeichen für Zeichen über die UART ausgeliefert.
- Spezielle UART-Ausgabeaktionen werden mit dem ASCII-Escape-Byte `27` eingerahmt: `<esc>Aktion<esc>/`. Die gesamte Einrahmung einschließlich der Aktion wird nicht ins Terminal geschrieben. Ein nicht eingerahmtes `load <path>` ist normale Ausgabe; nur `<esc>load <path><esc>/` lädt die Datei und hängt zuerst ihre 32-Bit-Wortanzahl (Big Endian), danach ihren Inhalt an den UART-Eingabepuffer an.
- `<esc>read <path><esc>/` hängt zuerst die 32-Bit-Byteanzahl (Big Endian), danach die unveränderten Bytes einer regulären Datei an den UART-Eingabepuffer an. Bei einer fehlenden oder nicht lesbaren Datei wird stattdessen `UINT32_MAX` zurückgegeben.
- `<esc>read-range <offset> <count> <path><esc>/` hängt zuerst die tatsächlich zurückgegebene 32-Bit-Byteanzahl (Big Endian) und danach höchstens `count` Dateibytes ab `offset` an. Reicht der Bereich über das Dateiende hinaus, wird eine entsprechend kleinere Byteanzahl zurückgegeben. Bei einer fehlenden oder nicht lesbaren Datei wird `UINT32_MAX` statt Dateidaten zurückgegeben.
- `<esc>file-size <path><esc>/` hängt die 32-Bit-Größe einer regulären Datei (Big Endian) an. Bei einer fehlenden oder nicht lesbaren Datei wird `UINT32_MAX` zurückgegeben. PicoOS verwendet diesen Befehl für `file_exists()` und `SEEK_END`, sodass normale Bereichslesevorgänge die vollständige Dateigröße nicht wiederholt übertragen.
- `<esc>!<terminal_command><esc>/` führt den Befehl in dem Arbeitsverzeichnis aus, in dem der Emulator gestartet wurde.
- `<esc>write <path><esc>/` leitet alle folgenden UART-Ausgabebytes in die neu angelegte beziehungsweise geleerte Datei um. `<esc>write stdout<esc>/` schaltet zurück zur Terminalausgabe, `<esc>write stderr<esc>/` leitet auf die Standardfehlerausgabe um.
- `<esc>append <path><esc>/` leitet die folgenden UART-Ausgabebytes an das Ende der Datei um und legt sie bei Bedarf an, ohne vorhandene Daten zu löschen.

Für die UART zeigt das TUI fürs Debuggen neben offensichtlich den Registern R0, R1 und R2 (Senderegister, Empfangsregister und Statusregister) in Form der ersten 3 Adressen noch Informationen an, wie 
- `Waiting time sending: ...`, was die Wartezeit ist, die es braucht ein 8-Bit Packet über die UART an das vom RETI-Emulator simulierte Anzeigegerät zu versenden. Die Wartezeit wird zufällig generiert und ihr Maximalwert wird über `-w i` festgelegt (default für `i` ist 10). Die Wartezeit fängt an sobald durch setzen des Bit b0 im Statusregister auf 0 signalisiert wurde, dass das Byte im Senderegister R0 final feststeht und versandt werden kann. Sobald die Wartezeit abgelaufen ist, wird b0 wieder auf 1 gesetzt. Ohne `-d` wird das Byte über `stdout` ausgegeben. Mit `-d` wird es unabhängig davon, ob die Terminalansicht gerade geöffnet ist, fortlaufend in `/tmp/reti_emulator/terminal_output.bin` aufgezeichnet. `(V)iew terminal` löscht die sichtbare Terminalfläche, gibt die vollständige Aufzeichnung wieder und zeigt neue Bytes danach direkt an. Das Byte wird außerdem unter `Current send data: ...` sowie `All send data: ...` angezeigt.
- `Current send data: ...`, was das zuletzt gesendete ASCII-Zeichen zeigt.
- `All send data: ...`, was die bisher gesendeten ASCII-Zeichen zeigt.
- `Waiting time receiving: ...`, was die Wartezeit ist, die es braucht ein 8-Bit Packet über die UART von dem vom RETI-Emulator simulierte Eingaberät zu empfangen. Für das Setzen der Wartezeit, die für das Empfangen notwendig ist gilt das selbe wie für die Wartezeit, die für das Senden notwendig ist. Die Wartezeit fängt an sobald durch setzen des Bit b1 im Statusregister auf 0 signalisiert wurde, dass man für den Empfang eines weiteren 8-Bit Packets vom Eingabegerät bereit ist, also u.a. das zuletzt empfangene 8-Bit Packet aus dem Empfangsregister R1 gesichert hat und das Empfangsregister R1 somit mit neuen Daten überschreiben kann. Sobald die Wartezeit abgelaufen ist, wird b1 wieder auf 1 gesetzt und unter `Current input: ...` verschwindet das gerade empfangende 8-Bit Packet und wird dafür ins Empfangsregister R1 geladen und dort angezeigt.
- `Current input: ...`, was das aktuell in Übertragung befindliche ASCII-Zeichen zeigt.
- `Remaining input: ...`, was die noch gepufferten ASCII-Zeichen zeigt.

> *Tipp:* Sie können dieser Wartezeit mittels der Kommandozeilenoption `-w 0` (waiting time) auf 0 setzen, um beim Debuggen nicht unnötig warten zu müssen. Allgemein steht `i` in `-w i` für die Anzahl Befehle, die maximal gewartet werden muss. Man sollten hierbei allerdings nicht vergessen, dass ein geschriebenes Programm mit beliebig langen Wartezeit umgehen können sollte.

> *Tipp:* Um beim Debuggen nicht immer selbst einen Input eingeben zu müssen können sie mittels der Kommandozeilenoption `-m` (metadata) leerzeichenseparierte Inputs aus dem Kommentar `# input: 16909060 a hallo` am Anfang des Programms `prgrm.reti` rauslesen.

Eine kurze Kontext-Zusammenfassung steht in [doc/uart_protocol.md](doc/uart_protocol.md).

# TSL Extension
```
TSL DS ACC 0: ACC=M[DS + 0], then M[DS + 0]=1 (DS contains address of Lock variable)
```

Die `TSL`-Instruction hat die Syntax `TSL S D i`. Dabei wird `S` als Register interpretiert, das eine Adresse enthält, `i` ist der Offset auf diese Adresse und `D` ist das Zielregister. `M[]` bezeichnet dabei einen Speicherzugriff. Im obigen Beispiel wird also zuerst der Wert an der Speicheradresse `DS + 0` nach `ACC` geladen und danach dieselbe Speicherzelle auf `1` gesetzt.
