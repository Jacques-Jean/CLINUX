#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "protocole.h"

int idQ, idShm;
char* pShm;
int fd;
volatile sig_atomic_t stop = 0;

void handlerSIGTERM(int sig)
{
    if (pShm != nullptr && pShm != (char*)-1)
        shmdt(pShm);
    fprintf(stderr, "(CLIENT) Mémoire détachée, fin du client\n");
    exit(0);
}



int main()
{
    signal(SIGTERM, handlerSIGTERM);
    signal(SIGINT, SIG_IGN);

    fprintf(stderr,"(PUBLICITE %d) Initialisation\n", getpid());

    // 🔑 récupération file de messages
    idQ = msgget(CLE, 0);
    if (idQ == -1) { perror("msgget"); exit(1); }

    // 🔑 récupération shm
    idShm = shmget(CLE, 200, 0);
    if (idShm == -1) { perror("shmget"); exit(1); }

    pShm = (char*)shmat(idShm, NULL, 0);
    if (pShm == (char*)-1) { perror("shmat"); exit(1); }

    fd = open("./publicites.dat", O_RDONLY);
    if (fd == -1) { perror("open"); exit(1); }

    while (!stop)
    {
        PUBLICITE pub;
        if (read(fd, &pub, sizeof(pub)) != sizeof(pub))
        {
            lseek(fd, 0, SEEK_SET);
            continue;
        }

        strncpy(pShm, pub.texte, 199);
        pShm[199] = 0;

        MESSAGE m;
        m.type = 1;
        m.expediteur = getpid();
        m.requete = UPDATE_PUB;

        msgsnd(idQ, &m, sizeof(m)-sizeof(long), 0);
        fprintf(stderr,"(PUBLICITE) UPDATE_PUB envoyé\n");

        sleep(pub.nbSecondes);
    }

    shmdt(pShm);
    close(fd);
    fprintf(stderr,"(PUBLICITE) Arrêt propre\n");
    return 0;
}
