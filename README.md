# Installation auf Systemen, auf denen Kompilierung nicht möglich ist über eine statische Binary
```bash
ssh <pool_login>@login.informatik.uni-freiburg.de
mkdir -p ~/.local/bin
wget wget https://github.com/matthejue/RETI-Interpreter/releases/latest/download/reti_interpreter -P ~/.local/bin
chmod 755 ~/.local/bin/reti_interpreter
echo 'export PATH="~/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc # um es direkt anzuwenden ohne sich neu einloggen zu müssen
vim test.reti
./reti_interpreter -d test.reti
```

# Installation auf Systemen, auf denen Kompilierung möglich ist durch eben Kompilierung
```bash
git clone -b main https://github.com/matthejue/RETI-Interpreter.git --depth 1
cd RETI-Interpreter
make
sudo make install-linux # nur unter Linux
```

# Verwendung
Der RETI-Interpreter ist dazu in der Lage, die RETI-Befehle eines in einer `.reti`-Datei angebenen RETI-Programms zu interpretieren. D.h. er kann das RETI-Programm **ausführen**, indem er die RETI-Befehle aus einer Datei `prgrm.reti` herausliest und in den simulierten SRAM schreibt und mithilfe eines autogenerierten EPROM-Startprogramms, dass zu Beginn ausgeführt wird an den Start dieses Programmes springt. Zum Ausführen eines Programmes muss der RETI-Interpreter mit dem Pfad zum RETI-Programm als Argument aufgerufen werden, z.B.:

```bash
$ reti_interpreter ./prgrm.reti
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

RETI-Interpreter speichert alle Memory-Inhalte des SRAM in einer Datei `sram.bin` ab. Die aus der Datei `prgrm.reti` geparsten Assembly-Befehle werden realitätsgetreu als 32-Bit (4 Byte)  Maschinenbefehle in einer Datei `sram.bin` abgespeichert, weil dies am speichereffizientesten ist und die RETI möglichst realistisch simuliert werden soll.

> Die Datei `sram.bin` ist zwar in der Ausgabe von `ls -alh /tmp/sram.bin` 8GiB groß, aber in Wirklichkeit verbraucht die Datei bei einem kleinen RETI-Programm nur wenige KibiBytes, weil Sparse Files verwendet werden. Das sieht man z.B. mit `du -h /tmp/sram.bin`.

> *Tipp:* Mittels `-f /tmp` (files) lässt sich das `/tmp` Verzeichnis unter Linux für die Speicherung von `sram.bin` nutzen. Das Verzeichnis `\tmp` ist häufig als **tmpfs**-Partition, welche im Arbeitsspeicher gemounted ist umgesetzt. Dadurch existiert der Inhalt des Verzeichnis nach dem Herunterfahren nicht mehr und das Verzeichnis, indem der RETI-Interpreter ausgeführt wird, wird nicht mit unnützen Dateien vollgemüllt.

Mithilfe der Kommandozeilenoption `-i` (isr code) ist der RETI-Interpreter in der Lage die RETI-Befehle für **Interrupt-Service-Routinen** aus einer Datei `interrupt_service_routines.reti` herauszulesen und an den Anfang des simulierten SRAM, vor das geladene Programm aus `program.reti` zu schreiben. Mithilfe von `INT i` kann wie in der Vorlesung erklärt an den Anfang jeder dieser Interrupt-Service-Routinen `i` gesprungen werden. Mittels `RTI` kann am Ende einer Interrupt-Service-Routine wieder an die nächste Stelle im ursprünglichen Programm zurückgesprungen werden, an der dieses mittels `INT i` unterbrochen wurde. 

Mittels der Direktive `IVTE i` (**I**nterrupt **V**ector **T**able **E**ntry) können **Einträge einer Interrupt Vector Tabelle** mit der korrekten Startadresse einer Interrupt-Sevice-Routine erstellt werden. Die SRAM Konstante wird automatisch auf `i` draufaddiert.

## Debugging
Mittels der Kommandozeilenoption `-d` (debug mode) ist der RETI-Interpreter in der Lage das Programm zu **debuggen**, d.h. er zeigt die Speicher- und Registerinhalte nach Ausführung eines jeden Befehls an. Zwischen diesen kann der Benutzer sich mittels `n` (`n`ext) und dann `Enter` forwärts bewegen. Wird `INT 3` in das RETI-Programm geschrieben stellt dies einen Breakpoint dar, wobei zum jeweils näcsten mittels `c` (`c`ontinue) und dann `Enter` gesprungen werden kann. In der untersten Zeile des Text-User-Interfaces (TUI) stehen alle Aktionen, die Sie in diesem Debug-Modus ausführen können.

Mithilfe sogenannter **Watchpointer** wird ein mittels `-r i` (radius) bestimmter Radius von `i` (default ist 2) sichtbaren Instructions bzw. Speicherinhalten über und unter einer von diesem Watchpointer betrachteten Speicheradresse angezeigt. Die Speicheradresse ist hierbei entweder über den Inhalt eines vorher dem Watchpointer zugewiesenen Registers bestimmt oder einfach direkt durch eine vorher an den Watchpointer zugewiesene Speicheradresse. Die verschiedenen verfügbaren Wachpointer in dem TUI für das Debuggen sind `ew` (EPROM Watchpointer), `swc` (SRAM Watchpointer für das Codesegment), `swd` (SRAM Watchpointer für das Datensegment) und `sws` (SRAM Watchpointer für den Stack). Die Zuweisung der Speicheradresse oder eines Registers erfolgt dabei über das Kommando `a <watchpointer-shorthand> <register_or_address>` (`a`ssign), z.B. in Form von `a ew 10`, `a swd 2147483848`, `a sws BAF` usw.

Damit die Studenten sich immer darauf verlassen können, dass die Kernfunktionalitäten des RETI-Intepreters mit jedem Release während des Semesters gleich bleiben, müssen **neue Features**, welche diese zuerst etablierten Kernfunktionalitäten brechen erst mit `-E` (extended features) **aktiviert** werden.

Momentan wird mit `-E` nur aktiviert, dass eine beliebige Interrupt Service Routine `INT i`, wenn der `PC` auf diese zeigt bei `n` direkt komplett ausgeführt wird. Wenn allerdings der `PC` auf `INT i` zeigt und das Kommando `s` (step into) ausgeführt wird, dann wird in die Interrupt Service Routine gesprungen und diese Schritt für Schritt ausgeführt. `s` funktioniert also so, wie man es bei üblichen Debuggern von Funktionsaufrufen kennt, nur hier für Interrupt Service Routinen.

> *Tipp:* Mit der Kommandozeilenoption `-b` (binary) werden alle Registerinhalte, Speicherinhalte und Immediates im Binärsystem angezeigt, damit lässt sich beim debuggen z.B. leichter Shiften nachvollziehen.

> *Tipp:* Um beim Debugging direkt zur Startadresse Ihres in den SRAM geladenen Programmes zu springen setzen man am besten einen Breakpoint `INT 3` an den Anfang des Programmes und führen dann direkt am Anfang nach Ausführen von `reti_interpreter -d prgrm.reti` das Kommando `c` aus.

## UART
*Die Kommunikation mit der UART ist wie folgt umgesetzt:*
- Die 4 8-Bit-Packete zum Empfang einer 32-Bit-Ganzzahl oder eines 32-Bit ASCII-Zeichens (wobei 3 Bytes verschwendet werden, da diese immer 0 sind) werden im vom RETI-Interpreter simulierten Eingabegerät im **Big-Endian Format** (höchstwertige Bytes / 8-Bit Packete zuerst) übertragen. Nachdem jeweils durch setzen des Bit b1 im **Statusregister** auf 0 dem simulierten Eingabegerät mitgeteilt wurde, dass man für den Empfang einer (weiteren) 8-Bit bereit ist, wird der User nach einer Eingabe gefragt (falls nicht die Kommandozeilenoption `-m` aktiviert wurde, es keinen `# input: ...` Kommentar am Anfang des RETI-Programms gibt oder alle Eingaben aus dem Input-Kommentar verbraucht wurde). Nach einer Wartezeit kommt jeweils die 8-Bit Eingabe im **Empfangsregister** R1 an und b1 im Statusregister wird vom simulierten Eingabegerät wieder auf 1 gesetzt. Diese Prozedur muss 4 mal in dieser Form ausgeführt werden, um eine vollständige 32-Bit Ganzzahl oder ein 32-Bit ASCII-Zeichen zu empfangen.
- Zuerst muss im Protokoll die Zahl 4 bzw. 0 versandt werden, um dem im RETI-Interpreter simulierten Anzeigegerät auf der anderen Seite der UART-Kommunikation mitzuteilen, dass als nächstes eine Ganzzahl bzw. ein nullterminierter String übertragen wird. Dann wird jeweils in das **Senderegister** R0 ein 8-Bit Packet geschrieben welches ein Teil der 32-Bit Ganzzahl sein wird, die sich aus 4 solchen 8-Bit Packeten zusammensetzt bzw. ein Teil des nullterminierten Strings sein wird, der sich aus maximal 256 solchen 8-Bit Packeten zusammensetzt, wobei das letzte das null-terminator Zeichen sein wird. Zudem wird jeweils darauffolgend im **Statusregister** R2 das Bit b0 auf 0 gesetzt, um zu signalieren, dass das zu versendende 8-Bit Packet im Senderegister R0 feststeht und so versandt werden kann. Danach wird vom RETI-Interpreter jeweils eine Wartezeit für die Übertragung eines 8-Bit Packets simuliert und erst nach dieser Wartezeit wird b0 wieder auf 1 gesetzt und ein 8-Bit Packet, welches zu dem Zeitpunkt als die Wartezeit startete im Senderegister R0 stand wurde versandt. Das Anzeigegerät wird vom RETI-Interpreter simuliert, indem dieses nach Erhalt des 4ten der 4 8-Bit Packete im **Big-Endian Format** unter Einhaltung der gerade beschriebenen Prozedur die aus diesen 4 8-Bit Packeten zusammengesetzte 32-Bit-Zahl im Terminal über `stdout` ausgibt bzw. nach Erhalt von maximal 256 ASCII-Zeichen als 8-Bit Packete, wobei das letzte Zeichen das null-terminator Zeichen `\0` ist unter Einhaltung der gerade beschriebenen Prozedur, den aus diesen ASCII-Zeichen zusammengesetzten String im Terminal über `stdout` ausgibt.

Für die UART zeigt das TUI fürs Debuggen neben offensichtlich den Registern R0, R1 und R2 (Senderegister, Empfangsregister und Statusregister) in Form der ersten 3 Adressen noch Informationen an, wie 
- `Waiting time sending: ...`, was die Wartezeit ist, die es braucht ein 8-Bit Packet über die UART an das vom RETI-Interpreter simulierte Anzeigegerät zu versenden. Die Wartezeit wird zufällig generiert und ihr Maximalwert wird über `-w i` festgelegt (default für `i` ist 10). Die Wartezeit fängt an sobald durch setzen des Bit b0 im Statusregister auf 0 signalisiert wurde, dass die Daten im Senderegister R0 final feststehen und versandt werden können. Sobald die Wartezeit abgelaufen ist, wird b0 wieder auf 1 gesetzt und unter `Current send data: ...` wird das gerade versandte 8-Bit Packet hinzugefügt, was in dem moment im Senderegister R0 steht. Falls alle 8-Bit Teilpackete empfangen wurden, wird nach ablaufen der Wartezeit für das letzte 8-Bit Packet die zusammengesetzte 32-Bit Ganzzahl oder der zusammengesetzte String über `stdout` ausgegeben und unter `All send data: ...` angezeigt.
- `Current send data: ...`, was die für die zu versendende 32-Bit Ganzzahl oder den aktuell zu versendenden String die bisher gesendeten 8-Bit Packete sind. Bei einer 32-Bit Ganzzahl ist es zuerst das Initialisierungspacket mit der Zahl 4 und danach die leerzeichenseparierten 4 8-Bit Packete in Dezimaldarstellung welche zusammengesetzt die 32-Bit Ganzzahl ergeben. Bei einem String ist es zuerst das Initialisierungspacket mit der Zahl 0 und nach einem Leerzeichen die 8-Bit Packete des Strings als ASCII-Zeichen ohne leerzeichenseparierung bis vor das null-terminator Zeichen `\0`. Sobald die Wartezeit für das Senden über die UART für ein aktuelles 8-Bit Packet abgelaufen ist, wird hier das aktuelle 8-Bit Packet hinzugefügt, dass in dem Moment im Senderegister R0 steht.
- `All send data: ...`, was die bisher versandten 32-Bit Ganzzahlen bzw. vollständigen Strings sind, die dann final, sobald alle 8-Bit Teilpackete empfangen wurden auch wirklich in dieser Form vom RETI-Interpreter über `stdout` ausgegeben wurden, um das Ausgabegerät zu simulieren. Sobald die Wartezeit für das Senden über die UART für das 4te 8-Bit Packet einer 32-Bit Ganzzahl oder das 8-Bit Packet für das null-terminator Zeichen eines ASCII-Strings abgelaufen ist, erscheint hier die zusammengestezte 32-Bit Ganzzahl bzw. der zusammengesetzte String, welche ebenfalls über `stdout` ausgegeben werden.
- `Waiting time receiving: ...`, was die Wartezeit ist, die es braucht ein 8-Bit Packet über die UART von dem vom RETI-Interpreter simulierte Eingaberät zu empfangen. Für das Setzen der Wartezeit, die für das Empfangen notwendig ist gilt das selbe wie für die Wartezeit, die für das Senden notwendig ist. Die Wartezeit fängt an sobald durch setzen des Bit b1 im Statusregister auf 0 signalisiert wurde, dass man für den Empfang eines weiteren 8-Bit Packets vom Eingabegerät bereit ist, also u.a. das zuletzt empfangene 8-Bit Packet aus dem Empfangsregister R1 gesichert hat und das Empfangsregister R1 somit mit neuen Daten überschreiben kann. Sobald die Wartezeit abgelaufen ist, wird b1 wieder auf 1 gesetzt und unter `Current input: ...` verschwindet das gerade empfangende 8-Bit Packet und wird dafür ins Empfangsregister R1 geladen und dort angezeigt.
- `Current input: ...`, was die aktuell zu empfangende 8-Bit Eingabe ist, die hierher aus der 8-Bit Aufteilung in `Remaining input: 16909060(2 3 4)` wandert, sobald im Statusregister das Empfangsbit b1 auf 0 gesetzt wurde. Für die 32-Bit Ganzzahl Eingabe `16909060` welches sich aus den 4 8-Bit Packeten `1 2 3 4` zusammensetzt, die hier in Klammern leerzeichensepariert visualisiert werden, ist dies die 8-Bit Eingabe `1`. Wie bereits im Punkt zu `Waiting time receiving: ...` erklärt verschwindet das zu empfangende 8-Bit Packet sobald die Wartezeit abgelaufen ist.
- `Remaining input: ...`, was, leerzeichensepariert alle verbleibenden, noch nicht verbrauchten Eingaben sind. Das können entweder einzelne 32-Bit Zahlen sein oder ein String aus ASCII-Zeichen. Diese Eingaben wurden entweder aus dem Kommentar `# input: ...` am Anfang des Programmes durch Setzen der Kommandozeilenoption `-m` rausgelesen oder das ist eine einzelne Eingabe, die vom Benutzer des RETI-Interpreters eingegeben wird, da entweder die Kommandozeilenoption `-m` nicht aktiviert wurde, es keinen Kommentar `# input: ...` am Anfang des RETI-Programms gibt oder bereits alle Eingeban verbraucht wurden. Die Eingabe des Benutzers ist entweder 32-Bit Zahl oder einzelnes ASCII-Zeichen. Sobald im Statusregister initial das Empfangsbit b1 auf 0 gesetzt, um das erste 8-Bit Packet einer 32-Bit Ganzzahl oder ASCII String Eingabe zu empfangen, wird bei der aktuell zu empfangenden Eingabe die Aufteilung in die 8-Bit Packete in Klammern `16909060(2 3 4)` daneben angezeigt, wobei das aktuel zu empfangende 8-Bit Packet unter `Current input: ...` angezeigt wird.

> *Tipp:* Sie können dieser Wartezeit mittels der Kommandozeilenoption `-w 0` (waiting time) auf 0 setzen, um beim Debuggen nicht unnötig warten zu müssen. Allgemein steht `i` in `-w i` für die Anzahl Befehle, die maximal gewartet werden muss. Man sollten hierbei allerdings nicht vergessen, dass ein geschriebenes Programm mit beliebig langen Wartezeit umgehen können sollte.

> *Tipp:* Um beim Debuggen nicht immer selbst einen Input eingeben zu müssen können sie mittels der Kommandozeilenoption `-m` (metadata) leerzeichenseparierte Inputs aus dem Kommentar `# input: 16909060 3` am Anfang des Programms `prgrm.reti` rauslesen.
