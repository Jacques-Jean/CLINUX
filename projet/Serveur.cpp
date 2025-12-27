#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <mysql.h>
#include <setjmp.h>
#include "protocole.h" // contient la cle et la structure d'un message
#include "FichierUtilisateur.h"
#include <errno.h> 

int idQ,idShm,idSem;
TAB_CONNEXIONS *tab;

void afficheTab();


struct sembuf semBD_P    = {0, -1, 0};  // bloquant
struct sembuf semBD_V    = {0,  1, 0};
struct sembuf semTab_P   = {1, -1, 0};  // bloquant
struct sembuf semTab_V   = {1,  1, 0};
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
} arg;


MYSQL* connexion;

void handlerSIGINT(int sig)
{
    (void)sig;

    fprintf(stderr, "\n(SERVEUR %d) Arret du serveur...\n", getpid());

    /* Prendre sémaphore table pour nettoyer proprement */
    semop(idSem, &semTab_P, 1);

    /* Nettoyer pidPublicite si c'est moi qui l'ai créé */
    if (tab->pidPublicite == getpid())
    {
        kill(tab->pidPublicite, SIGTERM);
        waitpid(tab->pidPublicite, NULL, 0);
        tab->pidPublicite = 0;
    }

    /* Nettoyer mon pidServeur */
    if (tab->pidServeur1 == getpid())
        tab->pidServeur1 = 0;
    else if (tab->pidServeur2 == getpid())
        tab->pidServeur2 = 0;

    /* Nettoyer pidAdmin si mort */
    if (tab->pidAdmin > 0 && kill(tab->pidAdmin, 0) == -1)
        tab->pidAdmin = 0;

    semop(idSem, &semTab_V, 1);

    /* Détacher mémoire */
    shmdt(tab);

    /* SUPPRIMER IPC UNIQUEMENT si dernier serveur actif */
    if (tab->pidServeur1 == 0 && tab->pidServeur2 == 0)
    {
        fprintf(stderr, "(SERVEUR) Dernier serveur actif → suppression IPC\n");
        shmctl(idShm, IPC_RMID, NULL);
        msgctl(idQ, IPC_RMID, NULL);
        semctl(idSem, 0, IPC_RMID);  // supprime TOUS les sémaphores
    }

    fprintf(stderr,"(SERVEUR %d) Arret complet\n", getpid());
    exit(0);
}
void handlerSIGCHLD(int sig)
{
    (void)sig;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        fprintf(stderr,"(SERVEUR) Suppression du fils zombi %d\n", pid);

        /* Prendre sémaphore table pour nettoyer pidModification */
        semop(idSem, &semTab_P, 1);
        for (int i = 0; i < 6; i++)
            if (tab->connexions[i].pidModification == pid)
                tab->connexions[i].pidModification = 0;
        semop(idSem, &semTab_V, 1);
    }
}

int main()
{

    // Connection à la BD
    connexion = mysql_init(NULL);
    if (mysql_real_connect(connexion,"localhost","Student","PassStudent1_","PourStudent",0,0,0) == NULL)
    {
        fprintf(stderr,"(SERVEUR) Erreur de connexion à la base de données...\n");
        exit(1);    
    }

    // Armement des signaux
    signal(SIGINT, handlerSIGINT);
    signal(SIGTERM, handlerSIGINT);


    // Armement SIGCHLD
    struct sigaction A;
    A.sa_handler = handlerSIGCHLD;
    sigemptyset(&A.sa_mask);
    A.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &A, NULL);

     /* ========================================== */
    /* NOUVEAU : DOUBLE SERVEUR CORRECT          */
    /* ========================================== */
    pid_t monPid = getpid();
    
    /* Test si 1er serveur */
    idShm = shmget(CLE, sizeof(TAB_CONNEXIONS), IPC_CREAT | IPC_EXCL | 0600);
    bool premierServeur = (idShm != -1);

    if (premierServeur)
    {
        fprintf(stderr,"(SERVEUR %d) PREMIER : création IPC\n", monPid);
        idQ = msgget(CLE, IPC_CREAT | 0600);
        idSem = semget(CLE, 2, IPC_CREAT | 0600);  // 2 SÉMAPHORES
        
        arg.val = 1; semctl(idSem, 0, SETVAL, arg);  // BD
        arg.val = 1; semctl(idSem, 1, SETVAL, arg);  // Table   
    }
    else
    {
        fprintf(stderr,"(SERVEUR %d) SECOND : connexion IPC\n", monPid);
        idShm = shmget(CLE, sizeof(TAB_CONNEXIONS), 0);
        idQ = msgget(CLE, 0);
        idSem = semget(CLE, 2, 0);
    }

    tab = (TAB_CONNEXIONS*)shmat(idShm, NULL, 0);
    
    /* Protéger init table */
    semop(idSem, &semTab_P, 1);
    
    if (premierServeur)
    {
        memset(tab, 0, sizeof(TAB_CONNEXIONS));
        tab->pidServeur1 = monPid;
        pid_t pidPub = fork();
        if (pidPub == 0) { execl("./Publicite", "Publicite", NULL); exit(1); }
        tab->pidPublicite = pidPub;
    }
    else
    {
        /* Vérifier place */
        bool placeOK = false;
        if (tab->pidServeur1 == 0 || kill(tab->pidServeur1, 0) == -1)
            { tab->pidServeur1 = monPid; placeOK = true; }
        else if (tab->pidServeur2 == 0 || kill(tab->pidServeur2, 0) == -1)
            { tab->pidServeur2 = monPid; placeOK = true; }
        if (!placeOK) { semop(idSem, &semTab_V, 1); shmdt(tab); exit(1); }
    }
    
    semop(idSem, &semTab_V, 1);
    afficheTab();


    int i,k,j;
    MESSAGE m;
    MESSAGE reponse;
    char requete[200];
    MYSQL_RES    *resultat;
    MYSQL_ROW    tuple;
    PUBLICITE pub;



    while(1)
        {
            fprintf(stderr,"(SERVEUR %d) Attente d'une requete...\n",getpid());

            if (msgrcv(idQ, &m, sizeof(MESSAGE)-sizeof(long), 1, 0) == -1)
            {
                if (errno == EINTR)
                    continue;          // interruption par un signal (SIGCHLD) -> on recommence l'attente

                perror("(SERVEUR) Erreur de msgrcv");
                msgctl(idQ, IPC_RMID, NULL);
                exit(1);
            }
        switch(m.requete)
        {
            case CONNECT :    
                                         
                                        {
                                            fprintf(stderr,"(SERVEUR %d) Requete CONNECT reçue de %d\n",getpid(),m.expediteur);
                                            for (int i=0 ; i<6 ; i++)
                                            {
                                                if (tab->connexions[i].pidFenetre ==0) {
                                                    tab->connexions[i].pidFenetre = m.expediteur;
                                                    strcpy(tab->connexions[i].nom,"");
                                                    for (int j=0 ; j<5 ; j++) tab->connexions[i].autres[j] = 0;
                                                    tab->connexions[i].pidModification = 0;
                                                    break;
                                                }
                                            }

                                            break;
                                        }

            case DECONNECT :    
                                         
                                        {
                                            fprintf(stderr,"(SERVEUR %d) Requete DECONNECT reçue de %d\n",getpid(),m.expediteur);
                                            for (int i=0 ; i<6 ; i++)
                                            {
                                                if (tab->connexions[i].pidFenetre ==m.expediteur) {
                                                    tab->connexions[i].pidFenetre = 0;
                                                    strcpy(tab->connexions[i].nom,"");
                                                    for (int j=0 ; j<5 ; j++) tab->connexions[i].autres[j] = 0;
                                                    tab->connexions[i].pidModification = 0;
                                                    break;
                                                }
                                            }



                                            break; 
                                        }
            case LOGIN:
            {
                fprintf(stderr,
                    "(SERVEUR %d) Requete LOGIN reçue de %d : --%s--%s--%s--\n",
                    getpid(), m.expediteur, m.data1, m.data2, m.texte);

                char nom[20], motDePasse[20];
                strcpy(nom, m.data2);
                strcpy(motDePasse, m.texte);

                int nouvelUtilisateur = m.data1[0] - '0';
                bool ok = false;

                int pos = estPresent(nom);

                // ===== NOUVEL UTILISATEUR =====
                if (nouvelUtilisateur == 1)
                {
                    if (pos == 0 || pos == -1)   // utilisateur absent
                    {
                        ajouteUtilisateur(nom, motDePasse);

                        char requete[256];
                        sprintf(requete,
                            "INSERT INTO UNIX_FINAL (nom, gsm, email) VALUES ('%s', '---', '---');",
                            nom);
                        mysql_query(connexion, requete);

                        strcpy(reponse.texte, "Nouvel utilisateur créé : bienvenue !");
                        ok = true;
                    }
                    else
                    {
                        strcpy(reponse.texte, "Utilisateur déjà existant !");
                    }
                }

                // ===== UTILISATEUR EXISTANT =====
                else
                {
                    if (pos == -1)
                    {
                        strcpy(reponse.texte, "Aucun utilisateur enregistré.");
                    }
                    else if (pos == 0)
                    {
                        strcpy(reponse.texte, "Utilisateur inconnu…");
                    }
                    else if (verifieMotDePasse(pos, motDePasse))
                    {
                        strcpy(reponse.texte, "Login réussi !");
                        ok = true;
                    }
                    else
                    {
                        strcpy(reponse.texte, "Mot de passe incorrect…");
                    }
                }

                reponse.type = m.expediteur;
                reponse.requete = LOGIN;

                if (ok)
                {
                    strcpy(reponse.data1, "OK");

                    for (int i = 0; i < 6; i++)
                    {
                        if (tab->connexions[i].pidFenetre == m.expediteur)
                        {
                            strcpy(tab->connexions[i].nom, nom);
                            break;
                        }
                    }
                }
                else
                {
                    strcpy(reponse.data1, "KO");
                }

                msgsnd(idQ, &reponse, sizeof(reponse)-sizeof(long), 0);
                kill(m.expediteur, SIGUSR1);

                if (!ok) break;

                // prévenir les autres
                for (int i = 0; i < 6; i++)
                {
                    if (tab->connexions[i].pidFenetre != 0 &&
                        tab->connexions[i].pidFenetre != m.expediteur &&
                        strlen(tab->connexions[i].nom) > 0)
                    {
                        MESSAGE r;
                        r.type = tab->connexions[i].pidFenetre;
                        r.requete = ADD_USER;
                        strcpy(r.data1, nom);

                        msgsnd(idQ, &r, sizeof(r)-sizeof(long), 0);
                        kill(r.type, SIGUSR1);
                    }
                }

                // envoyer la liste au nouveau
                for (int i = 0; i < 6; i++)
                {
                    if (tab->connexions[i].pidFenetre != 0 &&
                        tab->connexions[i].pidFenetre != m.expediteur &&
                        strlen(tab->connexions[i].nom) > 0)
                    {
                        MESSAGE r;
                        r.type = m.expediteur;
                        r.requete = ADD_USER;
                        strcpy(r.data1, tab->connexions[i].nom);

                        msgsnd(idQ, &r, sizeof(r)-sizeof(long), 0);
                        kill(m.expediteur, SIGUSR1);
                    }
                }

                break;
            }



            case LOGOUT:
                                        {
                                                fprintf(stderr,
                                                    "(SERVEUR %d) Requete LOGOUT reçue de %d\n",
                                                    getpid(), m.expediteur);

                                                char tempnom[20] = "";
                                                pid_t pidLogout = m.expediteur;
                                                int idx = -1;

                                                
                                                for (int i = 0; i < 6; i++)
                                                {
                                                        if (tab->connexions[i].pidFenetre == pidLogout)
                                                        {
                                                                strcpy(tempnom, tab->connexions[i].nom);
                                                                idx = i;
                                                                break;
                                                        }
                                                }

                                                if (idx == -1) 
                                                    break;

                                                
                                                strcpy(tab->connexions[idx].nom, "");
                                                for (int i = 0; i < 5; i++)
                                                        tab->connexions[idx].autres[i] = 0;

                                                
                                                for (int i = 0; i < 6; i++)
                                                {
                                                        for (int j = 0; j < 5; j++)
                                                        {
                                                                if (tab->connexions[i].autres[j] == pidLogout)
                                                                        tab->connexions[i].autres[j] = 0;
                                                        }
                                                }

                                                
                                                for (int i = 0; i < 6; i++)
                                                {
                                                        if (tab->connexions[i].pidFenetre != 0)
                                                        {
                                                                MESSAGE r;
                                                                r.type = tab->connexions[i].pidFenetre;
                                                                r.requete = REMOVE_USER;
                                                                strcpy(r.data1, tempnom);

                                                                msgsnd(idQ, &r, sizeof(r)-sizeof(long), 0);
                                                                kill(r.type, SIGUSR1);
                                                        }
                                                }

                                                break;
                                        }


            case ACCEPT_USER:
                                        {
                                                fprintf(stderr,
                                                    "(SERVEUR %d) Requete ACCEPT_USER reçue de %d pour %s\n",
                                                    getpid(), m.expediteur, m.data1);

                                                int idxExp = -1;
                                                pid_t pidCible = 0;

                                                for (int i = 0; i < 6; i++)
                                                        if (tab->connexions[i].pidFenetre == m.expediteur)
                                                                idxExp = i;

                                                for (int i = 0; i < 6; i++)
                                                        if (strcmp(tab->connexions[i].nom, m.data1) == 0)
                                                                pidCible = tab->connexions[i].pidFenetre;

                                                if (idxExp == -1 || pidCible == 0)
                                                        break;

                                                for (int i = 0; i < 5; i++)
                                                {
                                                        if (tab->connexions[idxExp].autres[i] == pidCible)
                                                                break; 
                                                }
                                                for (int i = 0; i < 5; i++)
                                                {

                                                        if (tab->connexions[idxExp].autres[i] == 0)
                                                        {
                                                                tab->connexions[idxExp].autres[i] = pidCible;
                                                                break;
                                                        }
                                                }

                                                break;
                                        }


            case REFUSE_USER :
                                        {
                                            fprintf(stderr,"(SERVEUR %d) Requete REFUSE_USER reçue de %d\n",getpid(),m.expediteur);
                                        
                                                int idxExp = -1;
                                                pid_t pidCible = 0;

                                                for (int i = 0; i < 6; i++)
                                                        if (tab->connexions[i].pidFenetre == m.expediteur)
                                                                idxExp = i;

                                                for (int i = 0; i < 6; i++)
                                                        if (strcmp(tab->connexions[i].nom, m.data1) == 0)
                                                                pidCible = tab->connexions[i].pidFenetre;

                                                if (idxExp == -1 || pidCible == 0)
                                                        break;

                                                for (int i = 0; i < 5; i++)
                                                {
                                                        if (tab->connexions[idxExp].autres[i] == pidCible)
                                                                break; 
                                                }
                                                for (int i = 0; i < 5; i++)
                                                {

                                                        if (tab->connexions[idxExp].autres[i] == pidCible)
                                                        {
                                                                tab->connexions[idxExp].autres[i] = 0;
                                                                break;
                                                        }
                                                }

                                                break;
                                        }
            case SEND:
                                            {
                                                    fprintf(stderr,"(SERVEUR %d) Requete SEND reçue de %d : --%s--\n",getpid(), m.expediteur, m.texte);

                                                    char nomExpediteur[20] = "";

                                                    int idx = -1;
                                                    for (int i = 0; i < 6; i++)
                                                    {
                                                            if (tab->connexions[i].pidFenetre == m.expediteur)
                                                            {
                                                                    strcpy(nomExpediteur, tab->connexions[i].nom);
                                                                    idx = i;
                                                                    break;
                                                            }
                                                    }

                                                    if (idx == -1) 
                                                        break;

                                                    for (int i = 0; i < 5; i++)
                                                    {
                                                            int pidDest = tab->connexions[idx].autres[i];
                                                            if (pidDest != 0)
                                                            {
                                                                    MESSAGE r;
                                                                    r.type = pidDest;
                                                                    r.requete = SEND;
                                                                    strcpy(r.data1, nomExpediteur);
                                                                    strcpy(r.texte, m.texte);

                                                                    msgsnd(idQ, &r, sizeof(r)-sizeof(long), 0);
                                                                    kill(pidDest, SIGUSR1);
                                                            }
                                                    }

                                                    break;
                                            }
 

            case UPDATE_PUB :
                                            fprintf(stderr,"(SERVEUR %d) Requete UPDATE_PUB reçue de %d\n",getpid(),m.expediteur);

                                            for (int i = 0; i < 6; i++)
                                                if (tab->connexions[i].pidFenetre != 0)
                                                    kill(tab->connexions[i].pidFenetre, SIGUSR2);
                                            break;

            case CONSULT:
            {
                fprintf(stderr,"(SERVEUR %d) Requete CONSULT reçue de %d pour %s\n",
                        getpid(), m.expediteur, m.data1);

                /* Création d’un processus Consultation dédié */
                pid_t pid = fork();
                if (pid == -1)
                {
                    perror("(SERVEUR) fork CONSULT");
                    break;
                }
                if (pid == 0)
                {
                    execl("./Consultation", "Consultation", NULL);
                    perror("execl Consultation");
                    exit(1);
                }

                /* Transfert de la requête au processus Consultation */
                MESSAGE r = m;
                r.type       = pid;            // destinataire = processus Consultation
                r.expediteur = m.expediteur;   // PID du client
                if (msgsnd(idQ, &r, sizeof(MESSAGE)-sizeof(long), 0) == -1)
                    perror("(SERVEUR) msgsnd CONSULT->Consultation");

                break;
            }             


            case MODIF1:
            {
                fprintf(stderr,"(SERVEUR %d) Requete MODIF1 reçue de %d\n",
                        getpid(), m.expediteur);

                int idx = -1;
                for (int i = 0; i < 6; i++)
                    if (tab->connexions[i].pidFenetre == m.expediteur)
                        idx = i;

                if (idx == -1)
                    break;

                // modification déjà en cours
                if (tab->connexions[idx].pidModification != 0)
                {
                    MESSAGE r;
                    r.type = m.expediteur;
                    r.requete = MODIF1;
                    strcpy(r.data1, "KO");
                    strcpy(r.data2, "KO");
                    strcpy(r.texte, "KO");
                    msgsnd(idQ, &r, sizeof(r)-sizeof(long), 0);
                    kill(m.expediteur, SIGUSR1);
                    break;
                }

                pid_t pid = fork();
                if (pid == 0)
                {
                    execl("./Modification", "Modification", NULL);
                    perror("execl Modification");
                    exit(1);
                }

                tab->connexions[idx].pidModification = pid;

                MESSAGE r;
                r.type       = pid;                     // type = PID du processus Modification
                r.expediteur = m.expediteur;           // PID du client
                r.requete    = MODIF1;
                strcpy(r.data1, tab->connexions[idx].nom);
                msgsnd(idQ, &r, sizeof(MESSAGE)-sizeof(long), 0);

                break;
            }

                      
            case MODIF2:
            {
                fprintf(stderr,"(SERVEUR %d) Requete MODIF2 reçue de %d\n",
                        getpid(), m.expediteur);

                int idx = -1;
                for (int i = 0; i < 6; i++)
                    if (tab->connexions[i].pidFenetre == m.expediteur)
                        idx = i;

                if (idx == -1)               break;
                if (tab->connexions[idx].pidModification == 0) break;

                MESSAGE r = m;
                r.type = tab->connexions[idx].pidModification;  // destinataire = Modification
                msgsnd(idQ, &r, sizeof(MESSAGE)-sizeof(long), 0);

                /* NE PAS remettre pidModification à 0 ici :
                   il sera remis à 0 dans handlerSIGCHLD quand le fils se termine */

                break;
            }


            case LOGIN_ADMIN:
                        {
                            fprintf(stderr,"(SERVEUR) LOGIN_ADMIN de %d\n", m.expediteur);
                            
                            bool adminOK = false;
                            if (tab->pidAdmin == 0 || kill(tab->pidAdmin, 0) == -1)
                            {
                                tab->pidAdmin = m.expediteur;
                                adminOK = true;
                            }

                            reponse.type = m.expediteur;
                            reponse.requete = LOGIN_ADMIN;
                            strcpy(reponse.data1, adminOK ? "OK" : "KO");
                            strcpy(reponse.texte, adminOK ? "Admin connecté" : "Admin déjà connecté");
                            msgsnd(idQ, &reponse, sizeof(reponse)-sizeof(long), 0);
                            kill(m.expediteur, SIGUSR1);
                            break;
                        }

                        case LOGOUT_ADMIN:
                                                    {
                                                        fprintf(stderr,"(SERVEUR) LOGOUT_ADMIN de %d\n", m.expediteur);
                                                        if (tab->pidAdmin == m.expediteur)
                                                            tab->pidAdmin = 0;
                                                        break;
                                                    }

                                                    case NEW_USER:
                                                    {
                                                        fprintf(stderr,"(SERVEUR) NEW_USER %s / %s\n", m.data1, m.data2);
                                                        
                                                        char nom[20], mdp[20];
                                                        strcpy(nom, m.data1);
                                                        strcpy(mdp, m.data2);
                                                        
                                                        bool ok = false;
                                                        int pos = estPresent(nom);
                                                        if (pos == 0 || pos == -1)  // absent
                                                        {
                                                            ajouteUtilisateur(nom, mdp);
                                                            
                                                            char sql[256];
                                                            sprintf(sql, "INSERT INTO UNIX_FINAL (nom, gsm, email) VALUES ('%s', '---', '---');", nom);
                                                            if (mysql_query(connexion, sql) == 0)
                                                                ok = true;
                                                        }

                                                        reponse.type = m.expediteur;
                                                        reponse.requete = NEW_USER;
                                                        strcpy(reponse.data1, ok ? "OK" : "KO");
                                                        strcpy(reponse.texte, ok ? "Utilisateur ajouté" : "Utilisateur existe déjà");
                                                        msgsnd(idQ, &reponse, sizeof(reponse)-sizeof(long), 0);
                                                        kill(m.expediteur, SIGUSR1);
                                                        break;
                                                    }

                        case DELETE_USER:
                                                    {
                                                        fprintf(stderr,"(SERVEUR) DELETE_USER %s\n", m.data1);
                                                        
                                                        char nom[20];
                                                        strcpy(nom, m.data1);
                                                        bool ok = false;

                                                        /* Supprimer fichier */
                                                        supprimeUtilisateur(nom);

                                                        /* Supprimer BD */
                                                        char sql[256];
                                                        sprintf(sql, "DELETE FROM UNIX_FINAL WHERE nom='%s';", nom);
                                                        if (mysql_query(connexion, sql) == 0)
                                                            ok = true;

                                                        reponse.type = m.expediteur;
                                                        reponse.requete = DELETE_USER;
                                                        strcpy(reponse.data1, ok ? "OK" : "KO");
                                                        strcpy(reponse.texte, ok ? "Utilisateur supprimé" : "Utilisateur inexistant");
                                                        msgsnd(idQ, &reponse, sizeof(reponse)-sizeof(long), 0);
                                                        kill(m.expediteur, SIGUSR1);
                                                        break;
                                                    }

                        case NEW_PUB:
                                                    {
                                                        fprintf(stderr,"(SERVEUR) NEW_PUB %s (%d sec)\n", m.texte, atoi(m.data1));
                                                        
                                                        int nbSec = atoi(m.data1);
                                                        FILE *f = fopen("publicites.dat", "a");
                                                        if (f)
                                                        {
                                                            PUBLICITE pub;
                                                            strcpy(pub.texte, m.texte);
                                                            pub.nbSecondes = nbSec;
                                                            fwrite(&pub, sizeof(PUBLICITE), 1, f);
                                                            fclose(f);

                                                            /* Réveiller Publicite si fichier créé */
                                                            if (tab->pidPublicite > 0)
                                                                kill(tab->pidPublicite, SIGUSR1);
                                                        }

                                                        reponse.type = m.expediteur;
                                                        reponse.requete = NEW_PUB;
                                                        strcpy(reponse.data1, "OK");
                                                        strcpy(reponse.texte, "Publicité ajoutée");
                                                        msgsnd(idQ, &reponse, sizeof(reponse)-sizeof(long), 0);
                                                        kill(m.expediteur, SIGUSR1);
                                                        break;
                                                    }

                    }

                    semop(idSem, &semTab_V, 1);  // libérer table
                    afficheTab();
                }

    mysql_close(connexion);
    return 0;
}


void afficheTab()
{
    fprintf(stderr,"Pid Serveur 1 : %d\n",tab->pidServeur1);
    fprintf(stderr,"Pid Serveur 2 : %d\n",tab->pidServeur2);
    fprintf(stderr,"Pid Publicite : %d\n",tab->pidPublicite);
    fprintf(stderr,"Pid Admin         : %d\n",tab->pidAdmin);
    for (int i=0 ; i<6 ; i++)
        fprintf(stderr,"%6d -%20s- %6d %6d %6d %6d %6d - %6d\n",tab->connexions[i].pidFenetre,
                                                                                                            tab->connexions[i].nom,
                                                                                                            tab->connexions[i].autres[0],
                                                                                                            tab->connexions[i].autres[1],
                                                                                                            tab->connexions[i].autres[2],
                                                                                                            tab->connexions[i].autres[3],
                                                                                                            tab->connexions[i].autres[4],
                                                                                                            tab->connexions[i].pidModification);
    fprintf(stderr,"\n");
}

