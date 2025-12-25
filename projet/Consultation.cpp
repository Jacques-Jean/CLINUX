#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <mysql.h>
#include "protocole.h"

int idQ, idSem;

struct sembuf P = {0, -1, 0};
struct sembuf V = {0,  1, 0};

int main()
{
    MESSAGE req, rep;

    fprintf(stderr, "(CONSULTATION %d) Démarrage\n", getpid());

    // File de messages
    idQ = msgget(CLE, 0);
    if (idQ == -1)
    {
        perror("msgget");
        exit(1);
    }

    // Sémaphore
    idSem = semget(CLE, 1, 0);
    if (idSem == -1)
    {
        perror("semget");
        exit(1);
    }
    semctl(idSem, 0, SETVAL, 1);

    // Réception de la requête CONSULT
    msgrcv(idQ, &req, sizeof(MESSAGE)-sizeof(long), 1, 0);

    fprintf(stderr, "(CONSULTATION %d) Requête reçue pour [%s]\n",
            getpid(), req.data1);

    // Prise du sémaphore
    semop(idSem, &P, 1);

    // Connexion MySQL
    MYSQL *connexion = mysql_init(NULL);
    if (!mysql_real_connect(connexion,
                            "localhost",
                            "Student",
                            "PassStudent1_",
                            "PourStudent",
                            0, 0, 0))
    {
        fprintf(stderr, "Erreur MySQL\n");
        exit(1);
    }

    // Requête SQL
    char sql[256];
    sprintf(sql,
        "SELECT gsm, email FROM utilisateurs WHERE nom='%s'",
        req.data1);

    mysql_query(connexion, sql);
    MYSQL_RES *res = mysql_store_result(connexion);
    MYSQL_ROW row = mysql_fetch_row(res);

    // Construction réponse
    rep.type = req.expediteur;
    rep.expediteur = getpid();
    rep.requete = CONSULT;

    if (row != NULL)
    {
        strcpy(rep.data1, "OK");
        strcpy(rep.data2, row[0]);   // GSM
        strcpy(rep.texte, row[1]);   // Email
    }
    else
    {
        strcpy(rep.data1, "KO");
        rep.data2[0] = '\0';
        rep.texte[0] = '\0';
    }

    // Envoi réponse
    msgsnd(idQ, &rep, sizeof(MESSAGE)-sizeof(long), 0);

    // Signal au client
    kill(req.expediteur, SIGUSR1);

    // Nettoyage
    mysql_free_result(res);
    mysql_close(connexion);

    // Libération sémaphore
    semop(idSem, &V, 1);

    fprintf(stderr, "(CONSULTATION %d) Fin\n", getpid());
    exit(0);
}
