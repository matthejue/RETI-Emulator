# Installation on systems where compilation is not possible via static binary
```bash
ssh <pool_login>@login.informatik.uni-freiburg.de
mkdir -p ~/.local/bin
wget https://github.com/matthejue/RETI-Interpreter/releases/download/v1.2.0/reti_interpreter -P ~/.local/bin
chmod 755 ~/.local/bin/reti_interpreter
echo 'export PATH="~/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc # to apply it directly without re-login
vim test.reti
./reti_interpreter -d test.reti
```

# Installation on systems where compilation is possible via compilation
```bash
git clone -b main https://github.com/matthejue/RETI-Interpreter.git --depth 1
cd RETI-Interpreter
make
sudo make install-linux # only under Linux
```
