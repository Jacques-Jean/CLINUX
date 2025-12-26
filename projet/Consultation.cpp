
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

    idQ = msgget(CLE, 0);
    if (idQ == -1)
    {
        perror("Consultation msgget");
        exit(1);
    }

    idSem = semget(CLE, 1, 0);
    if (idSem == -1)
    {
        perror("Consultation semget");
        exit(1);
    }

    /* NE PAS réinitialiser le sémaphore ici ! (uniquement dans le serveur)
       ==> suppression de semctl(idSem, 0, SETVAL, 1); */

    /* Réception de la requête CONSULT envoyée par le serveur : type = PID Consultation */
    if (msgrcv(idQ, &req, sizeof(MESSAGE)-sizeof(long), getpid(), 0) == -1)
    {
        perror("Consultation msgrcv");
        exit(1);
    }

    fprintf(stderr, "(CONSULTATION %d) Requête reçue pour [%s]\n",
            getpid(), req.data1);

    /* Prise BLOQUANTE du sémaphore */
    if (semop(idSem, &P, 1) == -1)
    {
        perror("Consultation semop P");
        exit(1);
    }

    /* Connexion MySQL */
    MYSQL *connexion = mysql_init(NULL);
    if (!mysql_real_connect(connexion,
                            "localhost",
                            "Student",
                            "PassStudent1_",
                            "PourStudent",
                            0, 0, 0))
    {
        fprintf(stderr, "(CONSULTATION %d) Erreur MySQL: %s\n",
                getpid(), mysql_error(connexion));
        /* Libérer le sémaphore avant de sortir */
        semop(idSem, &V, 1);
        exit(1);
    }

    /* Requête SQL : table UNIX_FINAL comme dans le serveur */
    char sql[256];
    sprintf(sql,
        "SELECT gsm, email FROM UNIX_FINAL WHERE nom='%s';",
        req.data1);

    if (mysql_query(connexion, sql) == -1)
    {
        fprintf(stderr, "(CONSULTATION %d) Erreur MySQL query: %s\n",
                getpid(), mysql_error(connexion));
        mysql_close(connexion);
        semop(idSem, &V, 1);
        exit(1);
    }

    MYSQL_RES *res = mysql_store_result(connexion);
    MYSQL_ROW row  = mysql_fetch_row(res);

    rep.type       = req.expediteur;   // type = PID client
    rep.expediteur = getpid();         // expéditeur = Consultation
    rep.requete    = CONSULT;

    if (row != NULL)
    {
        strcpy(rep.data1, "OK");
        strcpy(rep.data2, row[0] ? row[0] : "");
        strcpy(rep.texte, row[1] ? row[1] : "");
    }
    else
    {
        strcpy(rep.data1, "KO");
        rep.data2[0] = '\0';
        rep.texte[0] = '\0';
    }

    mysql_free_result(res);
    mysql_close(connexion);

    /* Libération du sémaphore avant l’envoi (ou après, au choix, mais avant exit) */
    if (semop(idSem, &V, 1) == -1)
        perror("Consultation semop V");

    /* Envoi réponse au client */
    if (msgsnd(idQ, &rep, sizeof(MESSAGE)-sizeof(long), 0) == -1)
        perror("Consultation msgsnd");

    /* Signal au client */
    kill(req.expediteur, SIGUSR1);

    fprintf(stderr, "(CONSULTATION %d) Fin\n", getpid());
    exit(0);
}
