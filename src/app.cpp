// ============================================================================
// app.cpp
// Smart Library Management System - CPSC 315
//
// Entry point for the interactive application. The test driver lives in
// main.cpp and builds as a separate executable, so the logic can be verified
// without simulating keystrokes.
// ============================================================================
#include "LibraryUI.h"

int main() {
    LibrarySystem library;
    LibraryUI ui(library);
    ui.seedDemoData();
    ui.run();
    return 0;
}
