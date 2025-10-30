 [![asciicast](https://asciinema.org/a/693086.svg)](https://asciinema.org/a/693086)

# Was ist der RETI-Emulator?
Eigentlich ist der RETI-Emulator ein **RETI-Emulator**, **RETI-Assembler**, **RETI-Debugger** und **Visualizer** für die Peripheriegeräte (UART) der RETI während der Ausführung:

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

Der RETI-Emulators hat einmal das Ziel, dass darauf eines Tages ein minimales Betriebssystem läuft, dass in PicoC geschrieben ist. Im Branch https://github.com/matthejue/RETI-Emulator/tree/statemachine befindet sich die aktuellste Version des RETI-Emulators. Ein weiteres Ziel des RETI-Emulators ist es im Übungsbetrieb die Studenten beim Schreiben von RETI Programmen zu unterstützen, daher zeigt der RETI-Emulator auch Fehlermeldungen an und hat einen stärkeren Fokus auf Bugtesting und Features, welche die Verwendung für Studenten angenehmer gestalten.

# Übersicht
## Kommandozeilenoptionen
- `-r ram_size`: Setzt Größe des SRAM (Stadardwert: `2^16=65536`)
- `-d`: Zeigt zweckmäßige Legacy Debug TUI an
- `-T`: Wenn zusammen mit `-d` aufgerufen zeigt es das viel bessere Ncurses Debug TUI an
- `-D`: Verwendet DS-ADDRESS-Extension anstelle der Addition des DS-Registers *
- `-r radius`: Setzt Radius an Speicherzellen, die in der Legacy Debug TUI um einen observierten Addresspointer herum angezeigt werden sollen
- `-f file_dir`: Gibt an, wo die Datei `sram.bin` erzeugt werden soll
- `-e eprom_prgrm_path`: Parst und lädt Eprom-Startprogramm aus Datei, die über Dateipfad gefunden werden kann
- `-i isrs_prgrm_path`: Parst und lädt Interrupt-Service Routinen aus Datei, die über Dateipfad gefunden werden kann
- `-w max_waiting_instrs`: Setzt raximale Wartezeit der UART für das Senden und Empfangen von Daten (Anzahle Befehle)
- `-t`: Aktiviert Testmode für Systemtests
- `-m`: Liest Eingaben aus Kommentar `# input: ...` raus
- `-v`: Zeigt zusäztliche Informationen an (Welche Kommandozeilenoptionen aktiviert sind)
- `-b`: Aktiviert die Darstellung von Dezimalzahlen in Binärdarstellung
- `-E`: Aktiviere Erweiterte Funktionalitäten (Hilfslinien um unnötige Leerzeichen sichtbar zu machen)
- `-u`: Wertet Werte im Datensegment in Zweierkomplementdarstellung oder Betrag-Vorzeichendarstellug aus *
- `-I timer_interrupt_interval`: Das Zeitinterval (Anzahl ausgeführte Befehle) zwischen Timer Interrupts *
- `-h`: Zeigt Verwendungshinweise an
- `-p page_size`: Setzt Seitengröße (Standardwert: `2^12=4096`) *
- `-a`: Aktiviert die Kommandozeilenoptionen, welche für die meisten Verwendungszwecke nützlich sind *
<!-- - `-l`: Zeigt das Legacy Debug Interface anstelle -->

> \* nur in der neusten Version im Branch https://github.com/matthejue/RETI-Emulator/tree/statemachine

## TUI Aktionen
- `n` ext
- `c` ontinue bis zu Breakpoint `INT 3` 
- `r` estart *
- `s` step into
- `f` inalize  *
- `t` rigger isr *
- `a` ssign (Adresse oder Regisgter)
  - Decision Menu und jederzeit mit `q` abbrechbar *
- `q` uit

> \* nur in der neusten Version im Branch https://github.com/matthejue/RETI-Emulator/tree/statemachine

# Installation und Updates
## Installation auf Linux Systemen
<!-- , auf denen Kompilierung nicht möglich ist über eine statische Binary -->
```bash
git clone -b main https://github.com/matthejue/RETI-Emulator.git ~/RETI-Emulator --depth 1
cd ~/RETI-Emulator
make install-linux-local

# kleiner Testlauf des RETI-Emulators 
# (funktioniert auf Rechnern, die Ncurses installiert haben, 
# z.B. auf Poolrechnern aber nicht den Rechnern des Rechenzentrums):
cd ~; reti_emulator -d -T -m -i ./RETI-Emulator/opts/isrs.reti \
  ./RETI-Emulator/sys_test/special_several_uart_receive_and_print_string_and_int.reti
```

<!-- ## Installation auf Linux Systemen, auf denen Kompilierung möglich ist durch eben Kompilierung -->
<!-- ```bash -->
<!-- git clone -b main https://github.com/matthejue/RETI-Emulator.git ~/RETI-Emulator --depth 1 -->
<!-- cd ~/RETI-Emulator -->
<!-- make install-linux-global -->
<!-- ``` -->

> Bitte für die Übungen die statische Binary verwenden und nicht selbst kompilieren, weil diese das letzte mal als sie in der Betriebssysteme Vorlesung eingesetzt wurde auf jeden Fall funktioniert hat. 

> Die statisch komplilierte Binary funktioniert inklusive des mittels `-d -T` aktivierbaren Ncurses Debug TUI auf den Poolrechnern aber nicht auf den Rechnern des Rechnezentrums. 

> Falls ihr ohne Gewähr die neusten Features des RETI-Emulators ausprobieren wollt, dann ist der aktuelle Stand des RETI-Emulators auf dem Branch: https://github.com/matthejue/RETI-Emulator/tree/statemachine zu finden, wo viele Bugs des main-Branch bereits gefixt wurden.

## Deinstallation auf Linux Systemen
<!-- , wenn vorher lokal installiert wurde -->
```bash
cd ~/RETI-Emulator
make uninstall-linux-local
```

<!-- ## Deinstallation auf Linux Systemen, wenn vorher global installiert wurde -->
<!-- ```bash -->
<!-- cd ~/RETI-Emulator -->
<!-- make uninstall-linux-global -->
<!-- ``` -->

## Updaten auf Linux Systemen
<!-- , wenn vorher lokal installiert wurde -->
```bash
cd ~/RETI-Emulator
make update-linux-local
```

<!-- ## Updaten auf Linux Systemen, wenn vorher global installiert wurde -->
<!-- ```bash -->
<!-- cd ~/RETI-Emulator -->
<!-- make update-linux-global -->
<!-- ``` -->
<!---->
<!-- > Bitte lokale und globale Installationen nicht mischen, beim Wechsel zur jeweils anderen Installationsart vorher eine Deinstallation für die zuvor verwendete Installationsart durchführen. -->


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

Mithilfe der Kommandozeilenoption `-i` (isr code) ist der RETI-Emulator in der Lage die RETI-Befehle für **Interrupt-Service-Routinen** aus einer Datei `interrupt_service_routines.reti` herauszulesen und an den Anfang des simulierten SRAM, vor das geladene Programm aus `program.reti` zu schreiben. Mithilfe von `INT i` kann wie in der Vorlesung erklärt an den Anfang jeder dieser Interrupt-Service-Routinen `i` gesprungen werden. Mittels `RTI` kann am Ende einer Interrupt-Service-Routine wieder an die nächste Stelle im ursprünglichen Programm zurückgesprungen werden, an der dieses mittels `INT i` unterbrochen wurde. 

Mittels der Direktive `IVTE i` (**I**nterrupt **V**ector **T**able **E**ntry) können **Einträge einer Interrupt Vector Tabelle** mit der korrekten Startadresse einer Interrupt-Sevice-Routine erstellt werden. Die SRAM Konstante wird automatisch auf `i` draufaddiert.

## Debugging
Mittels der Kommandozeilenoption `-d` (debug mode) ist der RETI-Emulator in der Lage das Programm zu **debuggen**, d.h. er zeigt die Speicher- und Registerinhalte nach Ausführung eines jeden Befehls an. Zwischen diesen kann der Benutzer sich mittels `n` (`n`ext) und dann `Enter` forwärts bewegen. Wird `INT 3` in das RETI-Programm geschrieben stellt dies einen Breakpoint dar, wobei zum jeweils näcsten mittels `c` (`c`ontinue) und dann `Enter` gesprungen werden kann. In der untersten Zeile des Text-User-Interfaces (TUI) stehen alle Aktionen, die Sie in diesem Debug-Modus ausführen können. Wird zusätzlich die Kommandozeilenoptionen `-T` hinzugefügt (also `-d -T`), wird das übersichtlichere Ncurses Debug TUI angezeigt.

Mithilfe sogenannter **Watchbojects** wird ein mittels `-r i` (radius) bestimmter Radius von `i` (default ist 2) sichtbaren Instructions bzw. Speicherinhalten über und unter einer von diesem Watchobject betrachteten Speicheradresse angezeigt. Die Speicheradresse ist hierbei entweder über den Inhalt eines vorher dem Watchobject zugewiesenen Registers bestimmt oder einfach direkt durch eine vorher an das Watchobject zugewiesene Speicheradresse. Die verschiedenen verfügbaren Wachpointer in dem TUI für das Debuggen sind `ew` (EPROM Watchobject), `swc` (SRAM Watchobject für das Codesegment), `swd` (SRAM Watchobject für das Datensegment) und `sws` (SRAM Watchobject für den Stack). Die Zuweisung einer Speicheradresse oder eines Registers erfolgt dabei über das Kommando `a` (`a`ssign), z.B. in Form von `a<enter>ew<enter>10`, `a<enter>sws<enter>BAF` usw.

<!-- Damit die Studenten sich immer darauf verlassen können, dass die Kernfunktionalitäten des RETI-Intepreters mit jedem Release während des Semesters gleich bleiben, müssen **neue Features**, welche diese zuerst etablierten Kernfunktionalitäten brechen erst mit `-E` (extended features) **aktiviert** werden. -->

<!-- Momentan wird mit `-E` nur aktiviert, dass eine beliebige Interrupt Service Routine `INT i`, wenn der `PC` auf diese zeigt bei `n` direkt komplett ausgeführt wird. Wenn allerdings der `PC` auf `INT i` zeigt und das Kommando `s` (step into) ausgeführt wird, dann wird in die Interrupt Service Routine gesprungen und diese Schritt für Schritt ausgeführt. `s` funktioniert also so, wie man es bei üblichen Debuggern von Funktionsaufrufen kennt, nur hier für Interrupt Service Routinen. -->

> *Tipp:* Mit der Kommandozeilenoption `-b` (binary) werden alle Registerinhalte, Speicherinhalte und Immediates im Binärsystem angezeigt, damit lässt sich beim debuggen z.B. leichter Shiften nachvollziehen.

> *Tipp:* Um beim Debugging direkt zur Startadresse Ihres in den SRAM geladenen Programmes zu springen setzen man am besten einen Breakpoint `INT 3` an den Anfang des Programmes und führen dann direkt am Anfang nach Ausführen von `reti_emulator -d prgrm.reti` das Kommando `c` aus.

## UART
*Die Kommunikation mit der UART ist wie folgt umgesetzt:*
- Die 4 8-Bit-Packete zum Empfang einer 32-Bit-Ganzzahl oder eines 32-Bit ASCII-Zeichens (wobei 3 Bytes verschwendet werden, da diese immer 0 sind) werden im vom RETI-Emulator simulierten Eingabegerät im **Big-Endian Format** (höchstwertige Bytes / 8-Bit Packete zuerst) übertragen. Nachdem jeweils durch setzen des Bit b1 im **Statusregister** auf 0 dem simulierten Eingabegerät mitgeteilt wurde, dass man für den Empfang eines (weiteren) 8-Bit Packets bereit ist, wird der User nach einer Eingabe gefragt (falls nicht die Kommandozeilenoption `-m` aktiviert wurde, es keinen `# input: ...` Kommentar am Anfang des RETI-Programms gibt oder alle Eingaben aus dem Input-Kommentar verbraucht wurde). Nach einer Wartezeit kommt jeweils die 8-Bit Eingabe im **Empfangsregister** R1 an und b1 im Statusregister wird vom simulierten Eingabegerät wieder auf 1 gesetzt. Diese Prozedur muss 4 mal in dieser Form ausgeführt werden, um eine vollständige 32-Bit Ganzzahl oder ein 32-Bit ASCII-Zeichen zu empfangen.
- Zuerst muss im Protokoll die Zahl 4 bzw. 0 versandt werden, um dem im RETI-Emulator simulierten Anzeigegerät auf der anderen Seite der UART-Kommunikation mitzuteilen, dass als nächstes eine Ganzzahl bzw. ein nullterminierter String übertragen wird. Dann wird jeweils in das **Senderegister** R0 ein 8-Bit Packet geschrieben welches ein Teil der 32-Bit Ganzzahl sein wird, die sich aus 4 solchen 8-Bit Packeten zusammensetzt bzw. ein Teil des nullterminierten Strings sein wird, der sich aus maximal 256 solchen 8-Bit Packeten zusammensetzt, wobei das letzte das null-terminator Zeichen sein wird. Zudem wird jeweils darauffolgend im **Statusregister** R2 das Bit b0 auf 0 gesetzt, um zu signalieren, dass das zu versendende 8-Bit Packet im Senderegister R0 feststeht und so versandt werden kann. Danach wird vom RETI-Emulator jeweils eine Wartezeit für die Übertragung eines 8-Bit Packets simuliert und erst nach dieser Wartezeit wird b0 wieder auf 1 gesetzt und ein 8-Bit Packet, welches zu dem Zeitpunkt als die Wartezeit startete im Senderegister R0 stand wurde versandt. Das Anzeigegerät wird vom RETI-Emulator simuliert, indem dieses nach Erhalt des 4ten der 4 8-Bit Packete im **Big-Endian Format** unter Einhaltung der gerade beschriebenen Prozedur die aus diesen 4 8-Bit Packeten zusammengesetzte 32-Bit-Zahl im Terminal über `stdout` ausgibt bzw. nach Erhalt von maximal 256 ASCII-Zeichen als 8-Bit Packete, wobei das letzte Zeichen das null-terminator Zeichen `\0` ist unter Einhaltung der gerade beschriebenen Prozedur, den aus diesen ASCII-Zeichen zusammengesetzten String im Terminal über `stdout` ausgibt.

Für die UART zeigt das TUI fürs Debuggen neben offensichtlich den Registern R0, R1 und R2 (Senderegister, Empfangsregister und Statusregister) in Form der ersten 3 Adressen noch Informationen an, wie 
- `Waiting time sending: ...`, was die Wartezeit ist, die es braucht ein 8-Bit Packet über die UART an das vom RETI-Emulator simulierte Anzeigegerät zu versenden. Die Wartezeit wird zufällig generiert und ihr Maximalwert wird über `-w i` festgelegt (default für `i` ist 10). Die Wartezeit fängt an sobald durch setzen des Bit b0 im Statusregister auf 0 signalisiert wurde, dass die Daten im Senderegister R0 final feststehen und versandt werden können. Sobald die Wartezeit abgelaufen ist, wird b0 wieder auf 1 gesetzt und unter `Current send data: ...` wird das gerade versandte 8-Bit Packet hinzugefügt, was in dem moment im Senderegister R0 steht. Falls alle 8-Bit Teilpackete empfangen wurden, wird nach ablaufen der Wartezeit für das letzte 8-Bit Packet die zusammengesetzte 32-Bit Ganzzahl oder der zusammengesetzte String über `stdout` ausgegeben und unter `All send data: ...` angezeigt.
- `Current send data: ...`, was die für die zu versendende 32-Bit Ganzzahl oder den aktuell zu versendenden String die bisher gesendeten 8-Bit Packete sind. Bei einer 32-Bit Ganzzahl ist es zuerst das Initialisierungspacket mit der Zahl 4 und danach die leerzeichenseparierten 4 8-Bit Packete in Dezimaldarstellung welche zusammengesetzt die 32-Bit Ganzzahl ergeben. Bei einem String ist es zuerst das Initialisierungspacket mit der Zahl 0 und nach einem Leerzeichen die 8-Bit Packete des Strings als ASCII-Zeichen ohne leerzeichenseparierung bis vor das null-terminator Zeichen `\0`. Sobald die Wartezeit für das Senden über die UART für ein aktuelles 8-Bit Packet abgelaufen ist, wird hier das aktuelle 8-Bit Packet hinzugefügt, dass in dem Moment im Senderegister R0 steht.
- `All send data: ...`, was die bisher versandten 32-Bit Ganzzahlen bzw. vollständigen Strings sind, die dann final, sobald alle 8-Bit Teilpackete empfangen wurden auch wirklich in dieser Form vom RETI-Emulator über `stdout` ausgegeben wurden, um das Ausgabegerät zu simulieren. Sobald die Wartezeit für das Senden über die UART für das 4te 8-Bit Packet einer 32-Bit Ganzzahl oder das 8-Bit Packet für das null-terminator Zeichen eines ASCII-Strings abgelaufen ist, erscheint hier die zusammengestezte 32-Bit Ganzzahl bzw. der zusammengesetzte String, welche ebenfalls über `stdout` ausgegeben werden.
- `Waiting time receiving: ...`, was die Wartezeit ist, die es braucht ein 8-Bit Packet über die UART von dem vom RETI-Emulator simulierte Eingaberät zu empfangen. Für das Setzen der Wartezeit, die für das Empfangen notwendig ist gilt das selbe wie für die Wartezeit, die für das Senden notwendig ist. Die Wartezeit fängt an sobald durch setzen des Bit b1 im Statusregister auf 0 signalisiert wurde, dass man für den Empfang eines weiteren 8-Bit Packets vom Eingabegerät bereit ist, also u.a. das zuletzt empfangene 8-Bit Packet aus dem Empfangsregister R1 gesichert hat und das Empfangsregister R1 somit mit neuen Daten überschreiben kann. Sobald die Wartezeit abgelaufen ist, wird b1 wieder auf 1 gesetzt und unter `Current input: ...` verschwindet das gerade empfangende 8-Bit Packet und wird dafür ins Empfangsregister R1 geladen und dort angezeigt.
- `Current input: ...`, was die aktuell zu empfangende 8-Bit Eingabe ist, die hierher aus der 8-Bit Aufteilung in `Remaining input: 16909060(2 3 4)` wandert, sobald im Statusregister das Empfangsbit b1 auf 0 gesetzt wurde. Für die 32-Bit Ganzzahl Eingabe `16909060` welches sich aus den 4 8-Bit Packeten `1 2 3 4` zusammensetzt, die hier in Klammern leerzeichensepariert visualisiert werden, ist dies die 8-Bit Eingabe `1`. Wie bereits im Punkt zu `Waiting time receiving: ...` erklärt verschwindet das zu empfangende 8-Bit Packet sobald die Wartezeit abgelaufen ist.
- `Remaining input: ...`, was, leerzeichensepariert alle verbleibenden, noch nicht verbrauchten Eingaben sind. Das können entweder einzelne 32-Bit Zahlen sein oder ein String aus ASCII-Zeichen. Diese Eingaben wurden entweder aus dem Kommentar `# input: ...` am Anfang des Programmes durch Setzen der Kommandozeilenoption `-m` rausgelesen oder das ist eine einzelne Eingabe, die vom Benutzer des RETI-Emulators eingegeben wird, da entweder die Kommandozeilenoption `-m` nicht aktiviert wurde, es keinen Kommentar `# input: ...` am Anfang des RETI-Programms gibt oder bereits alle Eingeban verbraucht wurden. Die Eingabe des Benutzers ist entweder 32-Bit Zahl oder einzelnes ASCII-Zeichen. Sobald im Statusregister initial das Empfangsbit b1 auf 0 gesetzt, um das erste 8-Bit Packet einer 32-Bit Ganzzahl oder ASCII String Eingabe zu empfangen, wird bei der aktuell zu empfangenden Eingabe die Aufteilung in die 8-Bit Packete in Klammern `16909060(2 3 4)` daneben angezeigt, wobei das aktuel zu empfangende 8-Bit Packet unter `Current input: ...` angezeigt wird.

> *Tipp:* Sie können dieser Wartezeit mittels der Kommandozeilenoption `-w 0` (waiting time) auf 0 setzen, um beim Debuggen nicht unnötig warten zu müssen. Allgemein steht `i` in `-w i` für die Anzahl Befehle, die maximal gewartet werden muss. Man sollten hierbei allerdings nicht vergessen, dass ein geschriebenes Programm mit beliebig langen Wartezeit umgehen können sollte.

> *Tipp:* Um beim Debuggen nicht immer selbst einen Input eingeben zu müssen können sie mittels der Kommandozeilenoption `-m` (metadata) leerzeichenseparierte Inputs aus dem Kommentar `# input: 16909060 3` am Anfang des Programms `prgrm.reti` rauslesen.
