CC             = gcc
BUILD_DIR      = build
SRC_DIR        = src
CFLAGS_DEBUG   = -g -Wall -Werror -pedantic #-fsanitize=address
CFLAGS_RELEASE = -Wall -Werror -pedantic -O2 -s

SOURCES         = $(shell find $(SRC_DIR)/ -name "*.c" | grep -v test)
DEBUG_TARGET    = debug_gingersnap
RELEASE_TARGET  = release_gingersnap
OBJECTS         = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
OBJECTS_DEBUG   = $(OBJECTS)
OBJECTS_RELEASE = $(OBJECTS)
OBJECTS_TEST    = $(OBJECTS)

TEST_TARGET  = test_gingersnap
TEST_SOURCES = $(shell find $(SRC_DIR)/ -name "*.c" | grep -v main)
TEST_OBJECTS = $(TEST_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

debug: $(DEBUG_TARGET)
.SECONDEXPANSION:
$(OBJECTS_DEBUG) : $$(patsubst $(BUILD_DIR)/%.o,$(SRC_DIR)/%.c,$$@)
	mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS_DEBUG)
$(DEBUG_TARGET): $(OBJECTS_DEBUG)
	$(CC) -o $@ $^
.PHONY: debug

release: $(RELEASE_TARGET)
.SECONDEXPANSION:
$(OBJECTS_RELEASE) : $$(patsubst $(BUILD_DIR)/%.o,$(SRC_DIR)/%.c,$$@)
	mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS_RELEASE)
$(RELEASE_TARGET): $(OBJECTS_RELEASE)
	$(CC) -o $@ $^
.PHONY: release

test: $(TEST_TARGET)
.SECONDEXPANSION:
$(OBJECTS_TEST) : $$(patsubst $(BUILD_DIR)/%.o,$(SRC_DIR)/%.c,$$@)
	mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS_DEBUG)
$(TEST_TARGET): $(OBJECTS_TEST)
	$(CC) -o $@ $^
.PHONY: test

clean:
	rm -r ./build/*
	rm ./debug_gingersnap
	rm ./release_gingersnap
	rm ./test_gingersnap
