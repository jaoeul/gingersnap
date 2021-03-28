BUILD_DIR      = build
SRC_DIR        = src
CFLAGS_DEBUG   = -Wall -Werror -pedantic -g -fsanitize=address
CFLAGS_RELEASE = -Wall -Werror -pedantic -O2

SOURCES = $(shell find $(SRC_DIR)/ -name "*.c" | grep -v test)
DEBUG_TARGET   = debug_gingersnap
RELEASE_TARGET = release_gingersnap
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

TEST_TARGET  = test_gingersnap
TEST_SOURCES = $(shell find $(SRC_DIR)/ -name "*.c" | grep -v main)
TEST_OBJECTS = $(TEST_SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

debug: $(DEBUG_TARGET)
.SECONDEXPANSION:
$(OBJECTS) : $$(patsubst $(BUILD_DIR)/%.o,$(SRC_DIR)/%.c,$$@)
	mkdir -p $(@D)
	$(CC) -c -o $@ $(CFLAGS_DEBUG) $<
$(DEBUG_TARGET): $(OBJECTS)
	$(CC) -o $@ $(CFLAGS_DEBUG) $^
.PHONY: debug

release: $(RELEASE_TARGET)
.SECONDEXPANSION:
$(OBJECTS) : $$(patsubst $(BUILD_DIR)/%.o,$(SRC_DIR)/%.c,$$@)
	mkdir -p $(@D)
	$(CC) -c -o $@ $(CFLAGS_RELEASE) $<
$(RELEASE_TARGET): $(OBJECTS)
	$(CC) -o $@ $(CFLAGS_RELEASE) $^
.PHONY: release

test: $(TEST_TARGET)
.SECONDEXPANSION:
$(TEST_OBJECTS) : $$(patsubst $(BUILD_DIR)/%.o,$(SRC_DIR)/%.c,$$@)
	mkdir -p $(@D)
	$(CC) -c -o $@ $(CFLAGS) $<
$(TEST_TARGET): $(TEST_OBJECTS)
	$(CC) -o $@ $(CFLAGS) $^
.PHONY: test

clean:
	rm -r ./build/*
	rm ./debug_gingersnap
	rm ./release_gingersnap
	rm ./test_gingersnap
