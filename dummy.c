#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(void) {
    const char *message = "Hello world\n";
    size_t length = strlen(message);

    if (write(STDOUT_FILENO, message, length) != (ssize_t)length) {
        perror("write");
        return 1;
    }

    return 0;
}
