#include "util.h"

// Print message STR and exit with code 1.
void fatal(const char * str)
{
    perror(str);
    exit(1);
}
