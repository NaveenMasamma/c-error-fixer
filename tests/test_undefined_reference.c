#include <stdio.h>

// Function declared but not defined - will cause linker error
void myFunction(int x);

int main() {
    myFunction(42);
    return 0;
}
