#include <stdio.h>
#include "settings.h"


int main()
{
    uint32_t val;
    char str[C2_SIZE];

    printf("*** Init ***\n");

    initSettings(0);

    printf("*** Reading ***\n");

    val = settings_ReadI32(pGroup_B0, b0param_C0);
    printf("C0 value is: %d\n", val);

    val = settings_ReadI32(pGroup_B0, b0param_C1);
    printf("C1 value is: %d\n", val);

    // Reading of C2[10]
    settings_ReadStr(pGroup_B1, 10, str);
    printf("C2 value is: %s\n", str);


    printf("*** Modifying ***\n");

    settings_WriteI32(pGroup_B0, b0param_C0, 9000);
    settings_WriteI32(pGroup_B0, b0param_C1, 45);
    sprintf(str, "Modified text");
    settings_WriteStr(pGroup_B1, 10, str);


    printf("*** Reading again ***\n");

    val = settings_ReadI32(pGroup_B0, b0param_C0);
    printf("C0 value is: %d\n", val);

    val = settings_ReadI32(pGroup_B0, b0param_C1);
    printf("C1 value is: %d\n", val);

    // Reading of C2[10]
    settings_ReadStr(pGroup_B1, 10, str);
    printf("C2 value is: %s\n", str);


    return 0;
}


void assert_true(int x)
{
    if (!x)
    {
        // TODO
        printf("Assert failed");
        while(1);
    }
}
