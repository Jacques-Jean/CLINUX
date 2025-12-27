#include "windowadmin.h"
#include <QApplication>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdio.h>

int idQ;
WindowAdmin *wAdmin = nullptr;  // la même globale que dans windowadmin.cpp

// Handler pour recevoir la réponse du serveur (LOGIN_ADMIN + réponses Admin)
void handlerSIGUSR1(int sig)
{
    (void)sig;
    MESSAGE m;

    // Lire tous les messages qui nous sont destinés
    while (msgrcv(idQ, &m, sizeof(MESSAGE) - sizeof(long), getpid(), IPC_NOWAIT) != -1)
    {
        switch (m.requete)
        {
            case LOGIN_ADMIN:
                if (strcmp(m.data1, "OK") == 0)
                {
                    // Connexion admin acceptée
                    fprintf(stderr,"(ADMINISTRATEUR %d) Login admin OK : %s\n", getpid(), m.texte);
                    if (wAdmin)
                        wAdmin->dialogueMessage("Admin", m.texte);
                }
                else
                {
                    // Refus : déjà un admin → on affiche et on quitte
                    fprintf(stderr,"(ADMINISTRATEUR %d) Login admin KO : %s\n", getpid(), m.texte);
                    if (wAdmin)
                        wAdmin->dialogueErreur("Admin", m.texte);
                    _exit(0);
                }
                break;

            case NEW_USER:
            case DELETE_USER:
            case NEW_PUB:
                if (wAdmin)
                    wAdmin->dialogueMessage("Opération", m.texte);
                break;
        }
    }
}

int main(int argc, char *argv[])
{
    // Récupération de la file de messages
    fprintf(stderr,"(ADMINISTRATEUR %d) Recuperation de l'id de la file de messages\n", getpid());
    idQ = msgget(CLE, 0);
    if (idQ == -1)
    {
        perror("(ADMINISTRATEUR) msgget");
        return 1;
    }

    // Armement SIGUSR1 pour recevoir la réponse LOGIN_ADMIN
    struct sigaction sa;
    sa.sa_handler = handlerSIGUSR1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    // Envoi de la requête LOGIN_ADMIN au serveur
    MESSAGE m;
    m.type       = 1;          // le serveur lit sur le type 1
    m.expediteur = getpid();   // pour pouvoir nous répondre
    m.requete    = LOGIN_ADMIN;
    memset(m.data1, 0, sizeof(m.data1));
    memset(m.data2, 0, sizeof(m.data2));
    memset(m.texte, 0, sizeof(m.texte));

    if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
    {
        perror("(ADMINISTRATEUR) msgsnd LOGIN_ADMIN");
        return 1;
    }

    fprintf(stderr,"(ADMINISTRATEUR %d) Attente reponse\n", getpid());

    // Lancement de l'UI Qt
    QApplication a(argc, argv);
    WindowAdmin w;
    wAdmin = &w;   // pour que le handler SIGUSR1 puisse afficher des boîtes de dialogue
    w.show();

    return a.exec();
}
