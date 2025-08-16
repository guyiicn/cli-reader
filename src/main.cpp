// Cli Ebook Reader
// Version 1.0
// MIT License
// Author: guyiicn@gmail.com

#include "AppController.h"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        AppController app;
        return app.Run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 2;
    }
}