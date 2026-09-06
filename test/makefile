# Compiler and tools 
CC      = gcc
CFLAGS  = -Wall -g -Iobj -Isrc
PARSER  = tpc_parser
LEXER   = tpc_lexer

#  Directories 
SRC_DIR      = src
OBJ_DIR      = obj
BIN_DIR      = bin
TEST_DIR     = test
GOOD_DIR     = $(TEST_DIR)/good
WARN_DIR     = $(TEST_DIR)/warn
SYN_ERR_DIR  = $(TEST_DIR)/syn-err
SEM_ERR_DIR  = $(TEST_DIR)/sem-err
REP_DIR      = rep
RESULT_FILE  = $(REP_DIR)/result.txt

# Auxiliary files 
ASM = _anonymous.asm
OBJ = obj/out.o
EXE = test_exec

#  Executable name 
EXECUTABLE = $(BIN_DIR)/tpcc

# allow caller override: make TEST_FLAG=-s test
TEST_FLAG ?=

# Build everything (if need asm add EXE)
all: $(BIN_DIR)/tpcc 

# Object list 
OBJECTS = obj/$(LEXER).o obj/$(PARSER).o obj/tree.o obj/nasm.o obj/symbol.o

$(OBJ_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

# Link final executable 
$(EXECUTABLE): $(OBJECTS) | $(BIN_DIR)
	$(CC) -o $@ $^

# Compile modules 
obj/tree.o: $(SRC_DIR)/tree.c $(SRC_DIR)/tree.h | $(OBJ_DIR)
	$(CC) -c -o $@ $< $(CFLAGS)

obj/nasm.o: $(SRC_DIR)/nasm.c $(SRC_DIR)/nasm.h $(SRC_DIR)/tree.h | $(OBJ_DIR)
	$(CC) -c -o $@ $< $(CFLAGS)

obj/symbol.o: $(SRC_DIR)/symbol.c $(SRC_DIR)/symbol.h $(SRC_DIR)/tree.h | $(OBJ_DIR)
	$(CC) -c -o $@ $< $(CFLAGS)

obj/$(PARSER).o: obj/$(PARSER).c $(SRC_DIR)/tree.h $(SRC_DIR)/nasm.h $(SRC_DIR)/symbol.h | $(OBJ_DIR)
	$(CC) -c -o $@ $< $(CFLAGS)

obj/$(PARSER).c obj/$(PARSER).h: $(SRC_DIR)/$(PARSER).y | $(OBJ_DIR)
	bison -d -o $@ $<

obj/$(LEXER).o: obj/$(LEXER).c obj/$(PARSER).h | $(OBJ_DIR)
	$(CC) -c -o $@ $< $(CFLAGS)

obj/$(LEXER).c: $(SRC_DIR)/$(LEXER).l obj/$(PARSER).h | $(OBJ_DIR)
	flex -o $@ $<

# $(OBJ): $(ASM)
# 	nasm -f elf64 $(ASM) -o $(OBJ)

# $(EXE): $(OBJ)
# 	ld $(OBJ) -o $(EXE)

#  Clean 
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(OBJ) $(EXE) nasm

# Help 
help:
	@echo "make all              : Build everything."
	@echo "make test             : Run tests (TEST_FLAG can be -s|-t|-d|-h)."
	@echo "make tree             : Shortcut for 'make TEST_FLAG=-t test'."
	@echo "make clean            : Remove build & report artifacts."
	@echo "make help             : Show this help."

# Unified test target 
test: $(EXECUTABLE)
	@mkdir -p $(REP_DIR)
	@echo "Running tests (flag: '$(TEST_FLAG)')..." | tee $(RESULT_FILE)

	@pass_good=0; total_good=0; \
	 pass_warn=0; total_warn=0; \
	 pass_syn=0;  total_syn=0;  \
	 pass_sem=0;  total_sem=0;  \
	 \
	 run_one() { \
	    dir=$$1; exp=$$2;                                      \
	    for f in $$dir/*.tpc; do                               \
	      [ -e "$$f" ] || continue;                            \
	      output=$$($(EXECUTABLE) $(TEST_FLAG) < $$f 2>&1);    \
	      ret=$$?;                                             \
	      echo "Testing $$f (exp $$exp, got $$ret)" >> $(RESULT_FILE); \
	      echo "$$output"          >> $(RESULT_FILE);          \
	      echo "--------------------" >> $(RESULT_FILE);       \
	      case "$$dir" in                                          \
	        $(GOOD_DIR))    total_good=$$((total_good+1)); [ $$ret -eq $$exp ] && pass_good=$$((pass_good+1)); ;; \
	        $(WARN_DIR))    total_warn=$$((total_warn+1)); [ $$ret -eq $$exp ] && pass_warn=$$((pass_warn+1)); ;; \
	        $(SYN_ERR_DIR)) total_syn=$$((total_syn+1));  [ $$ret -eq $$exp ] && pass_syn=$$((pass_syn+1));  ;; \
	        $(SEM_ERR_DIR)) total_sem=$$((total_sem+1));  [ $$ret -eq $$exp ] && pass_sem=$$((pass_sem+1));  ;; \
	      esac;                                                \
	    done;                                                  \
	 };                                                        \
	 \
	 run_one $(GOOD_DIR)    0; \
	 run_one $(WARN_DIR)    0; \
	 run_one $(SYN_ERR_DIR) 1; \
	 run_one $(SEM_ERR_DIR) 2; \
	 \
	 echo; \
	 printf "good    : %d / %d\n" $$pass_good $$total_good | tee -a $(RESULT_FILE); \
	 printf "warn    : %d / %d\n" $$pass_warn $$total_warn | tee -a $(RESULT_FILE); \
	 printf "syn-err : %d / %d\n" $$pass_syn  $$total_syn  | tee -a $(RESULT_FILE); \
	 printf "sem-err : %d / %d\n" $$pass_sem  $$total_sem  | tee -a $(RESULT_FILE); \
	 echo "Results saved in $(RESULT_FILE)"

#  Alias: tree view (AST) 
tree:
	@$(MAKE) TEST_FLAG=-t test

.PHONY: all clean help test tree
