#include <stdio.h>
#include "FichierUtilisateur.h"

int main()
{
    UTILISATEUR v[100];
    int n = listeUtilisateurs(v);
    printf("n = %d\n", n);
    for (int i = 0; i < n; i++)
        printf("%s %d\n", v[i].nom, v[i].hash);

    return 0;
}
