# based on source: https://stackoverflow.com/a/30602701

SRC_DIR      := source
OBJ_DIR      := object
BIN_DIR      := binary
TEST_DIR     := unit_test
OBJ_TEST_DIR := object_test
LIB_DIR      := library
INCLUDE_DIR  := include
VENDOR_DIR   := vendor

BIN_SRC  := $(BIN_DIR)/$(basename $(notdir $(wildcard $(SRC_DIR)/*_main.c)))
BIN_TEST := $(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/%,$(wildcard $(TEST_DIR)/*_test.c))
SRC      := $(filter-out %_main.c %_test.c, $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/*/*.c))
VENDOR_SRC := $(wildcard $(VENDOR_DIR)/cJSON/*.c)
OBJ_SRC  := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o) $(VENDOR_SRC:%.c=$(OBJ_DIR)/%.o)

CC       := gcc
CPPFLAGS := -I$(INCLUDE_DIR) -I$(VENDOR_DIR)/cJSON -MMD -MP
CFLAGS   := -Wall
LDFLAGS  :=
LDLIBS   := -lm

ifeq ($(LINUX_STATIC), 1)
	CPPFLAGS += -I$(INCLUDE_DIR)/ncursesw
	LDFLAGS  += -static -L$(LIB_DIR)
	LDLIBS   += -lncursesw
else
	ifeq ($(LINUX), 1)
		LDLIBS += -lncurses
	else
		ifeq ($(WINDOWS), 1)
			CPPFLAGS += -I$(INCLUDE_DIR)/ncurses
			CFLAGS   += -DNCURSES_STATIC
			LDFLAGS  += -L$(LIB_DIR)
			LDLIBS   += -lncurses
		else
			ifeq ($(MACOS), 1)
				LDLIBS += -lncurses
			else
				CFLAGS += -g
				LDLIBS += -lncurses
			endif
		endif
	endif
endif

.PRECIOUS: $(OBJ_DIR)/%.o $(OBJ_TEST_DIR)/%.o

.PHONY: \
	all \
	sys-test \
	test_no_passed \
	unit-test \
	run \
	clean \
	clean-directories \
	clean-files \
	debug \
	run_send_keypresses \
	debug_send_keypresses \
	patch-pico-os-kernel-stack \
	install-linux-local \
	pull-latest-version \
	update-linux-local \
	uninstall-linux-local \
	install-linux-global \
	update-linux-global \
	uninstall-linux-global

all: $(BIN_SRC)

SHELL := /bin/bash -x

unit-test: $(BIN_TEST)
	@bash -c 'for T in $(BIN_TEST); do \
		echo "Running $$T"; \
		./$$T || { \
			test_status=$$?; \
			echo "$$T failed with exit code $$test_status"; \
			exit $$test_status; \
		}; \
	done'

$(BIN_DIR)/%_main: $(OBJ_DIR)/%_main.o $(OBJ_SRC) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BIN_DIR)/%_test: $(OBJ_TEST_DIR)/%_test.o $(OBJ_SRC) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_TEST_DIR)/%.o: $(TEST_DIR)/%.c | $(OBJ_TEST_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR) $(OBJ_TEST_DIR):
	mkdir -p $@

sys-test: $(BIN_SRC)
	./export_environment_vars_for_makefile.sh;\
	./run_sys_tests.sh "$${COLUMNS}" "$(shell cat ./config/test_pattern.txt)" "$(EXTRA_ARGS)"

test_no_passed: $(BIN_SRC)
	./export_environment_vars_for_makefile.sh;\
	./run_sys_tests.sh --not-passed "$${COLUMNS}" "" "$(EXTRA_ARGS)"

patch-pico-os-kernel-stack:
	sed -i -E 's/"stack_start": *-?[0-9]+/"stack_start": 5000/' /home/areo/Documents/Studium/Pico-OS/kernel.sections

run: $(BIN_SRC)
	./binary/reti_emulator_main $(shell cat ./config/run_opts.txt) $(EXTRA_ARGS) $(shell cat ./config/run_path.txt)

clean: clean-files clean-directories

clean-directories:
	@$(RM) -rv $(BIN_DIR) $(OBJ_DIR) $(OBJ_TEST_DIR)
	find . -type d -wholename ".cache" -delete

clean-files:
	find . -type f -wholename "./system_test/*.output" -delete
	find . -type f -wholename "./system_test/*.expected_output" -delete
	find . -type f -wholename "./system_test/*.error" -delete
	find . -type f -name "sram.bin" -delete
	find . -type f -name "test_results" -delete

DEB_BIN := reti_emulator_main

run_send_keypresses:
	./send_keypresses.py --input ./config/input.txt ./binary/$(DEB_BIN) $(shell cat ./config/run_opts.txt) $(EXTRA_ARGS) $(shell cat ./config/run_path.txt)

debug: $(BIN_SRC) $(BIN_TEST)
	gdb --tui -n -x ./.gdbinit --args ./binary/$(DEB_BIN) $(shell cat ./config/deb_opts.txt) $(EXTRA_ARGS) $(shell cat ./config/debug_path.txt)

debug_send_keypresses:
	./send_keypresses.py --input ./config/debug_input.txt make debug DEB_BIN=$(DEB_BIN) EXTRA_ARGS=$(EXTRA_ARGS)

install-linux-local:
	if [ -f ~/.local/bin/reti_emulator ]; then rm ~/.local/bin/reti_emulator; fi
	mkdir -p ~/.local/bin
	wget https://github.com/matthejue/RETI-Emulator/releases/latest/download/reti_emulator-linux -O ~/.local/bin/reti_emulator
	chmod 700 ~/.local/bin/reti_emulator
	grep -qxF 'export PATH="~/.local/bin:$$PATH"' ~/.bashrc || echo 'export PATH="~/.local/bin:$$PATH"' >> ~/.bashrc

pull-latest-version:
	git pull

update-linux-local: pull-latest-version install-linux-local

uninstall-linux-local:
	rm -f ~/.local/bin/reti_emulator
	sed -i '/export PATH="~\/.local\/bin:$$PATH"/d' ~/.bashrc

install-linux-global: $(BIN_SRC)
	@sudo bash -c "if [ -L /usr/local/bin/reti_emulator ]; then rm -f /usr/local/bin/reti_emulator; fi && chmod 700 ./binary/reti_emulator_main && ln -s $(realpath .)/binary/reti_emulator_main /usr/local/bin/reti_emulator"

update-linux-global: pull-latest-version install-linux-global

uninstall-linux-global:
	@sudo rm -f /usr/local/bin/reti_emulator

-include $(OBJ_SRC:.o=.d)
-include $(patsubst $(BIN_DIR)/%,$(OBJ_DIR)/%.d,$(BIN_SRC))
-include $(patsubst $(BIN_DIR)/%,$(OBJ_TEST_DIR)/%.d,$(BIN_TEST))
