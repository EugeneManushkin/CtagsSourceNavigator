#include <resource.h>
#include <stdio.h>

int main()
{
  printf("%d.%d.%d.%d\n", CTAGS_VERSION_MAJOR, CTAGS_VERSION_MINOR, 0, CTAGS_BUILD);
}