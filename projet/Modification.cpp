
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <mysql.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "protocole.h"
#include "FichierUtilisateur.h"

int idQ, idSem;

int main()
{
    MESSAGE m;

    idQ  = msgget(CLE, 0);
    idSem = semget(CLE, 1, 0);

    /* Réception MODIF1 */
    if (msgrcv(idQ, &m, sizeof(MESSAGE)-sizeof(long), getpid(), 0) == -1)
    {
        perror("Modification msgrcv MODIF1");
        exit(1);
    }

    char nom[20];
    strcpy(nom, m.data1);
    int pidClient = m.expediteur;

    /* Tentative de prise NON BLOQUANTE du sémaphore */
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op  = -1;
    op.sem_flg = IPC_NOWAIT;

    if (semop(idSem, &op, 1) == -1)
    {
        /* Sémaphore occupé : renvoi KO au client */
        MESSAGE r;
        r.type       = pidClient;
        r.expediteur = getpid();
        r.requete    = MODIF1;
        strcpy(r.data1, "KO");
        strcpy(r.data2, "KO");
        strcpy(r.texte, "KO");
        msgsnd(idQ, &r, sizeof(MESSAGE)-sizeof(long), 0);
        kill(pidClient, SIGUSR1);
        exit(0);
    }

    /* Connexion MySQL + lecture GSM/EMAIL */
    MYSQL *connexion = mysql_init(NULL);
    mysql_real_connect(connexion,"localhost","Student","PassStudent1_",
                       "PourStudent",0,0,0);

    char gsm[20]   = "---";
    char email[50] = "---";
    char requete[256];

    sprintf(requete,
            "SELECT gsm,email FROM UNIX_FINAL WHERE nom='%s';",
            nom);

    mysql_query(connexion, requete);
    MYSQL_RES *res = mysql_store_result(connexion);
    MYSQL_ROW row = mysql_fetch_row(res);

    if (row)
    {
        if (row[0]) strcpy(gsm, row[0]);
        if (row[1]) strcpy(email, row[1]);
    }
    mysql_free_result(res);

    /* Envoi réponse MODIF1 au client */
    MESSAGE r;
    r.type       = pidClient;
    r.expediteur = getpid();
    r.requete    = MODIF1;
    strcpy(r.data1, "OK");
    strcpy(r.data2, gsm);
    strcpy(r.texte, email);
    msgsnd(idQ, &r, sizeof(MESSAGE)-sizeof(long), 0);
    kill(pidClient, SIGUSR1);

    /* Réception MODIF2 (mot de passe/gsm/email) */
    if (msgrcv(idQ, &m, sizeof(MESSAGE)-sizeof(long), getpid(), 0) == -1)
    {
        perror("Modification msgrcv MODIF2");
        /* libérer sémaphore avant de mourir */
        op.sem_op = 1;
        semop(idSem, &op, 1);
        exit(1);
    }

    /* Mise à jour BD */
    sprintf(requete,
            "UPDATE UNIX_FINAL SET gsm='%s', email='%s' WHERE nom='%s';",
            m.data2, m.texte, nom);
    mysql_query(connexion, requete);

    /* MAJ mot de passe si non vide */
    if (strlen(m.data1) > 0)
        updateMotDePasse(nom, m.data1);

    mysql_close(connexion);

    /* Libération sémaphore */
    op.sem_op = 1;
    semop(idSem, &op, 1);

    exit(0);
}
