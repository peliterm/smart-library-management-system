// app.cpp - interactive entry point.

#include "LibraryUI.h"

int main() {
    LibrarySystem library;
    LibraryUI ui(library);
    ui.seedDemoData();
    ui.run();
    return 0;
}
