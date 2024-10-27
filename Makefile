# based on source: https://stackoverflow.com/a/30602701

SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin
TEST_DIR := unit_test
OBJ_TEST_DIR := obj_test
INCLUDE_DIR := include
LIB_DIR := lib

BIN_SRC := $(BIN_DIR)/$(basename $(notdir $(wildcard $(SRC_DIR)/*_main.c)))
BIN_TEST := $(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/%,$(wildcard $(TEST_DIR)/*_test.c))
SRC := $(filter-out %_main.c %_test.c, $(wildcard $(SRC_DIR)/*.c))
OBJ_SRC := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

CPPFLAGS := -I$(INCLUDE_DIR) -MMD -MP
CFLAGS   := -Wall -g # -O2
LDFLAGS  := -L$(LIB_DIR) #-static
LDLIBS   := -lm

.PRECIOUS: $(OBJ_DIR)/%.o $(OBJ_TEST_DIR)/%.o
.PHONY: all sys-test unit-test run clean clean-directories clean-files debug install-linux

all: $(BIN_SRC)

SHELL := /bin/bash -x
unit-test: $(BIN_TEST)
		@bash -c 'for T in $(BIN_TEST); do \
			echo "Running $$T"; \
			./$$T || echo "$$T failed with exit code $$?"; \
		done'

$(BIN_DIR)/%_main: $(OBJ_DIR)/%_main.o $(OBJ_SRC) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BIN_DIR)/%_test: $(OBJ_TEST_DIR)/%_test.o $(OBJ_SRC) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_TEST_DIR)/%.o: $(TEST_DIR)/%.c | $(OBJ_TEST_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR) $(OBJ_TEST_DIR):
	mkdir -p $@

TEST_CLASS := all
sys-test: $(BIN_SRC)
	./export_environment_vars_for_makefile.sh;\
	./run_sys_tests.sh $${COLUMNS} $(TEST_CLASS) $(EXTRA_ARGS);

RUN_PRGRM := ./run/prgrm.reti
run: $(BIN_SRC)
	./bin/reti_interpreter_main $(shell cat ./run/run_opts.txt) $(EXTRA_ARGS) $(RUN_PRGRM)

clean: clean-files clean-directories
clean-directories:
	@$(RM) -rv $(BIN_DIR) $(OBJ_DIR) $(OBJ_TEST_DIR)
	find . -type d -wholename ".cache" -delete

clean-files:
	find . -type f -wholename "./sys_test/*.output" -delete
	find . -type f -wholename "./sys_test/*.expected_output" -delete
	find . -type f -wholename "./sys_test/*.error" -delete
	find . -type f -name "sram.bin" -delete
	find . -type f -name "hdd.bin" -delete
	find . -type f -name "test_results" -delete

DEB_BIN := reti_interpreter_main
debug: $(BIN_SRC) $(BIN_TEST)
	gdb --tui -n -x ./.gdbinit --args ./bin/$(DEB_BIN) $(shell cat ./run/deb_opts.txt) $(EXTRA_ARGS) $(RUN_PRGRM)

install-linux: $(BIN_SRC)
	@sudo bash -c "([[ ! -f /usr/local/bin/reti_interpreter ]] || rm /usr/local/bin/reti_interpreter) && chmod 755 ./bin/reti_interpreter_main && sudo ln -s $(realpath .)/bin/reti_interpreter_main /usr/local/bin/reti_interpreter"

-include $(OBJ:.o=.d)
