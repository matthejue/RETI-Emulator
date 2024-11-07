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
Der RETI-Interpreter ist dazu in der Lage, die RETI-Befehle eines in einer `.reti`-Datei angebenen RETI-Programms zu interpretieren. D.h. er kann das RETI-Programm **ausführen**, indem er die RETI-Befehle aus einer Datei `prgrm.reti` herausliest und in den simulierten SRAM schreibt und mithilfe eines autogenerierten EPROM-Startprogramms, dass zu Beginn ausgeführt wird an den Start dieses Programms springt. Zum Ausführen eines Programm muss der RETI-Interpreter mit dem Pfad zum RETI-Programm als Argument aufgerufen werden, z.B.:

```bash
$ reti_interpreter ./prgrm.reti
```

Das RETI-Programm `prgrm.reti`, das im Folgenden als Beispiel verwendet wird, sieht dabei wie folgt aus:
```
# input: 16909060 3
INT 3 # just a breakpoint, use c in debug mode -d to directly jump here
INT 0
MOVE ACC IN1
INT 0
MULT ACC IN1
INT 3
INT 1
JUMP 0
```

> Nicht vergessen `JUMP 0` ans Ende des Programmes zu setzen, sonst wird einfach weiter ausgeführt was danach im SRAM steht bzw. als was für Instructions der Speicherinhalt auf den der `PC` in dem Moment zeigt interpretiert wird.

RETI-Interpreter speichert die Befehle als 32-Bit Maschinenbefehle (4 Byte) in `sram.bin` ab, weil das am speichereffizientesten ist.

Mithilfe der Kommandozeilenoption `-d` ist der RETI-Interpreter in der Lage das Programm zu **debuggen**, d.h. er zeigt die Speicher- und Registerinhalte nach Ausführung eines jeden Befels an, zwischen denen der Benutzer sich mittels `n` (next) und dann `Enter` bewegen kann. Wird `INT 3` in das RETI-Programm geschrieben stellt dies einen Breakpoint dar, zu dem mittels `c` und dann `Enter` gesprungen werden kann. In der untersten Zeile des Text-User-Interfaces (TUI) stehen alle Aktionen, die Sie in diesem Debug-Modus ausführen können.

> *Tipp:* Mittels `-r i` ist der Radius $i$ sichtbarer Instructions bzw. Speicherinhalte über und unter der Speicherzelle auf welchen der vom jeweiligen Watchpointer ausgewählten Adress- bzw. Register-Zeiger zeigt einstellbar

> *Tipp:* Mit der Kommandozeilenoption `-b` werden alle Registerinhalte, Memoryinhalte und Immediates im Binärsystem angezeigt, damit lässt sich beim debuggen leichter das ganze Shiften nachvollziehen.

> *Tipp:* Mittels `-f /tmp` lässt sich das `/tmp` Verzeichnis unter Linux für die Speicherung von `hdd.bin` und `sram.bin` nutzen. Das Verzeichnis `\tmp` ist häufig als **tmpfs**-Partition, welche im Arbeitsspeicher gemounted ist umgesetzt. Dadurch existiert der Inhalt des Verzeichnis nach dem Herunterfahren nicht mehr.
\item die \inlinebox{sram.bin} Datei $\frac{2^{31}}{2^{30}} \cdot 4 = 8GiB$ groß ist, wenn man sich \inlinebox{ls -alh /tmp/sram.bin} ausgibt, aber in Wirklichkeit verbraucht die Datei nur $8KiB$, weil Sparse Files verwendet werden. Das sieht man z.B. mit \inlinebox{du -h /tmp/sram.bin}

\item \alert{Debugging direkt beim Programmstart beginnen:} Breakpoint \inlinebox{INT 3} an den Anfang des Programmes setzen und dann direkt am Anfang nach Ausführen von \inlinebox{reti_interpreter -d prgrm.reti} das Kommando \inlinebox{c} ausführen


Mithilfe der Kommandozeilenoption `-i` ist der RETI-Interpreter in der Lage die RETI-Befehle für **Interrupt-Service-Routinen** aus einer Datei `interrupt_service_routines.reti` herauszulesen und an den Anfang des simulierten SRAM, vor das geladene Programm aus `program.reti` zu schreiben. Mithilfe von `INT i` kann wie in der Vorlesung erklärt an den Anfang jeder dieser Interrupt-Service-Routinen `i` gesprungen werden. Mittels `RTI` kann am Ende einer Interrupt-Service-Routine wieder an die nächste Stelle im ursprünglichen Programm zurückgesprungen werden, an der dieses mittels `INT i` unterbrochen wurde. 

Mittels der Direktive `IVTE i` können **Einträge einer Interrupt Vector Tabelle** mit der korrekten Startadresse einer Interrupt-Sevice-Routine erstellt werden. Die SRAM Konstante wird automatisch auf `i` draufaddiert.

*Die Kommunikation mit der UARt ist wie folgt umgesetzt:*
- Die 4 8-Bit-Packete zu Übertratung einer 32-Bit-Ganzzahl werden im vom RETI-Interpreter simulierten Eingabegerät im **Big-Endian Format** (höchstwertige Bytes / 8-Bit Packete zuerst) übertragen. Nachdem Sie durch setzen des Bit b1 im **Statusregister** auf 0 dem simulierten Eingabegerät mitgeteilt haben für den Empfang von Eingaben bereit zu sein, wird der User nach einer Eingabe gefragt (falls nicht die Kommandozeilenoption `-m`} aktiviert wurde oder der gesamte im Input aus dem Input-Kommentar verbraucht wurde). Nach einer Wartezeit kommen die Daten im **Empfangsregister** R1 an und b1 im Statusregister wird vom simulierten Eingabegerät wieder auf 1 gesetzt. 
- Zur Einhaltung eines Protokolls muss zuerst die Zahl 4 bzw. 0 versandt werden, um dem im RETI-Interpreter simulierten Anzeigegerät auf der anderen Seite der UART-Kommunikation mitzuteilen, dass als nächstes eine Ganzzahl bzw. ein nullterminierter String übertragen wird. Nachdem Sie in das **Senderegister** R0 die gewünschten Daten geschrieben haben und im **Statusregister** R2 das Bit b0 auf 0 gesetzt haben, wird vom RETI-Interpreter eine Wartezeit für die Übertragung der Daten simuliert und erst nach dieser Wartezeit wird b0 wieder auf 1 gesetzt. Das Anzeigegerät wird vom RETI-Interpreter simuliert, indem dieses nach Erhalt der von 4 8-Bit-Zahlen im **Big-Endian Format** die zusammengesetzte 32-Bit-Zahl im Terminal über `stdout` ausgibt bzw. nach Erhalt von maximal 256 Ascii-Zeichen, wobei das letzte Zeichen das null-terminator Zeichen `\0` ist den zusammengesetzten String im Terminal über `stdout` ausgibt.

TODO: Bedeutung der TUI

> *Tipp:* Sie können dieser Wartezeit mittels der Kommandozeilenoption `-w 0` auf 0 setzen, um beim Debuggen nicht unnötig warten zu müssen. Man sollten hierbei allerdings nicht vergessen, dass ein geschriebenes Programm mit beliebig langen Wartezeit umgehen können sollte.

> *Tipp:* Um beim Debuggen nicht immer selbst einen Input eingeben zu müssen können sie mittels der Kommandozeilenoption `-m` Leerzeichenseparierte Inputs aus dem Kommentar `# input: -42 3` am Anfang des Programms `prgrm.reti` rauslesen.
