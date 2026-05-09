#include "Format.h"

#include <iostream>

int main() {
    std::wcout << L"Offline Image Converter core supports " << oic::supportedFormats().size() << L" target formats.\n";
    return 0;
}
