#include <assert.h>
#include <stdio.h>
#include "../package/LmiproDisplayPolicy.h"
int main()
{
    uint64_t bytes=0;
    assert(lmiproDisplayHandoff(0x9c000000,1080,2400,32,4320,0,0,&bytes));
    assert(bytes==10368000);
    // A valid-looking geometry at another physical address is not this board's resource.
    assert(!lmiproDisplayHandoff(0x3c000000,1080,2400,32,4320,0,0,&bytes) && !bytes);
    assert(!lmiproDisplayHandoff(0x9c000000,1080,2400,32,4320,4096,0,&bytes) && !bytes);
    assert(!lmiproDisplayHandoff(0x9c000000,1080,2400,32,4352,0,0,&bytes) && !bytes);
    assert(!lmiproDisplayHandoff(0x9c000000,1080,2400,32,4320,0,1,&bytes) && !bytes);
    assert(!lmiproDisplayHandoff(0x9c000000,1080,0,32,UINT64_MAX,0,0,&bytes) && !bytes);
    assert(!lmiproDisplayHandoff(UINT64_MAX-3,1080,2400,32,4320,0,0,&bytes) && !bytes);
    puts("PASS: observed board allocation accepted; wrong resource, padding, rotation and overflow refused");
}
