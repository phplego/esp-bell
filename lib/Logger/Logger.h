
#include <Arduino.h>

#define LOGGER_BUFFER_SIZE 5000

class Logger
{
public:
    Logger();
    void log_no_ln(String message);
    void log(String message);
    void print(String message);
    void ln();
    void println(String message);

    char buffer [LOGGER_BUFFER_SIZE + 1] = {0};

protected:
    int position = 0;
};