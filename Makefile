# Smart Library Management System - CPSC 315 - Michael Pelitera
#
#   make            build both binaries
#   make test       build and run the test suite
#   make demo       build and replay the scripted demonstration
#   make verify     tests, sanitizers, and a demo determinism check
#   make clean      remove build products
#
# -O2 matters here rather than being a habit: password verification runs
# PBKDF2 at 600,000 iterations, and an unoptimized build makes that about six
# times slower.

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic

HEADERS := $(wildcard src/*.h)

SHARED := src/BookInventory.cpp src/UserAccounts.cpp src/DueDateHeap.cpp \
          src/Recommender.cpp src/LibrarySystem.cpp src/DemoFixture.cpp \
          src/ActivityLog.cpp src/ReservationQueue.cpp \
          src/PasswordHash.cpp src/Huffman.cpp src/Sha256.cpp

all: slms_app slms_tests

slms_app: src/app.cpp src/LibraryUI.cpp $(SHARED) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ src/app.cpp src/LibraryUI.cpp $(SHARED)

slms_tests: src/main.cpp $(SHARED) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ src/main.cpp $(SHARED)

test: slms_tests
	./slms_tests

demo: slms_app
	sh demo/run_demo.sh

# Everything the submission claims, checked in one command.
#
# Every step must fail the build when it fails. An earlier version piped the
# test output through `tail`, which discards the test program's exit status,
# and reported sanitizer and demo problems with `echo` instead of a non-zero
# exit. `make verify` could then print failures and still succeed, which is
# worse than having no check at all.
verify: slms_tests slms_app
	@mkdir -p .verify
	@echo "--- test suite ---"
	./slms_tests
	@echo "--- sanitizers ---"
	$(CXX) -std=c++17 -O1 -Wall -Wextra -Wpedantic \
	    -fsanitize=address,undefined -fno-sanitize-recover=all -g \
	    -o .verify/slms_san src/main.cpp $(SHARED)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
	    UBSAN_OPTIONS=halt_on_error=1 ./.verify/slms_san > .verify/san.log
	@echo "sanitizers: clean"
	@echo "--- demo determinism ---"
	sh demo/run_demo.sh --fast > .verify/demo1.txt 2>&1
	sh demo/run_demo.sh --fast > .verify/demo2.txt 2>&1
	@cmp -s .verify/demo1.txt .verify/demo2.txt || { \
	    echo "FAIL: consecutive demo runs differ"; exit 1; }
	@cmp -s .verify/demo1.txt demo/recorded_run.txt || { \
	    echo "FAIL: live demo differs from the recorded fallback"; exit 1; }
	@echo "PASS: tests, sanitizers and demo determinism all verified"

clean:
	rm -rf slms_app slms_tests .verify

.PHONY: all test demo verify clean
