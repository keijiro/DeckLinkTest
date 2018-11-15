#include "DeckLinkTest.h"

int main()
{
    AssertSuccess(CoInitialize(nullptr));
    DeckLinkTest().Run();
    return 0;
}
