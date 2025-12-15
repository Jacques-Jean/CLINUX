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
int idQ,idShm,idSem;
TAB_CONNEXIONS *tab;

void afficheTab();

MYSQL* connexion;

void handlerSIGINT(int sig)
{
    (void)sig; 
    fprintf(stderr, "\n(SERVEUR) Suppression de la file de messages\n");

    if (msgctl(idQ, IPC_RMID, NULL) == -1)
    {
        perror("msgctl");
        exit(1);
    }

    fprintf(stderr, "(SERVEUR) File supprimee, arret du serveur\n");
    exit(0);
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

  // Creation des ressources
  fprintf(stderr,"(SERVEUR %d) Creation de la file de messages\n",getpid());
  idQ = msgget(CLE, IPC_CREAT | 0600);
    if (idQ == -1)
    {
        perror("msgget");
        exit(1);
    }

  // Initialisation du tableau de connexions
  fprintf(stderr,"(SERVEUR %d) Initialisation de la table des connexions\n",getpid());
  tab = (TAB_CONNEXIONS*) malloc(sizeof(TAB_CONNEXIONS)); 

  for (int i=0 ; i<6 ; i++)
  {
    tab->connexions[i].pidFenetre = 0;
    strcpy(tab->connexions[i].nom,"");
    for (int j=0 ; j<5 ; j++) tab->connexions[i].autres[j] = 0;
    tab->connexions[i].pidModification = 0;
  }
  tab->pidServeur1 = getpid();
  tab->pidServeur2 = 0;
  tab->pidAdmin = 0;
  tab->pidPublicite = 0;

  afficheTab();

  // Creation du processus Publicite

  int i,k,j;
  MESSAGE m;
  MESSAGE reponse;
  char requete[200];
  MYSQL_RES  *resultat;
  MYSQL_ROW  tuple;
  PUBLICITE pub;

  while(1)
  {
  	fprintf(stderr,"(SERVEUR %d) Attente d'une requete...\n",getpid());
    if (msgrcv(idQ,&m,sizeof(MESSAGE)-sizeof(long),1,0) == -1)
    {
      perror("(SERVEUR) Erreur de msgrcv");
      msgctl(idQ,IPC_RMID,NULL);
      exit(1);
    }

    switch(m.requete)
    {
      case CONNECT :  
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

      case DECONNECT :  
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

      case LOGIN:
                { 

                      fprintf(stderr,
                        "(SERVEUR %d) Requete LOGIN reçue de %d : --%s--%s--%s--\n",
                        getpid(), m.expediteur, m.data1, m.data2, m.texte);

                      char nom[20], motDePasse[20];
                      strcpy(nom, m.data2);
                      strcpy(motDePasse, m.texte);

                      int nouvelUtilisateur = m.data1[0] - '0';
                      bool flag = false;

                      int pos = estPresent(nom);
                      if (pos==-1){
                        fprintf(stderr,"problème avec estPresent\n");

                      }
                      if (nouvelUtilisateur == 1)
                      {
                          if (pos == 0 || pos == -1)
                          {
                              
                              ajouteUtilisateur(nom, motDePasse);
                              strcpy(reponse.texte, "Nouvel utilisateur créé : bienvenue !");
                              flag = true;
                          }

                          else
                              strcpy(reponse.texte, "Utilisateur déjà existant !");
                      }
                      else
                      {
                          if (pos == 0 || pos == -1)
                              strcpy(reponse.texte, "Utilisateur inconnu…");
                          else if (verifieMotDePasse(pos, motDePasse))
                          {
                              strcpy(reponse.texte, "Login réussi !");
                              flag = true;
                          }
                          else
                              strcpy(reponse.texte, "Mot de passe incorrect…");
                      }

                      if (flag)
                      {
                          for (int i = 0; i < 6; i++)
                          {
                              if (tab->connexions[i].pidFenetre == m.expediteur)
                              {
                                  strcpy(tab->connexions[i].nom, nom);
                                  break;
                              }
                          }

                          for (int i = 0; i < 6; i++)
                          {
                              if (tab->connexions[i].pidFenetre != 0 &&
                                  strcmp(tab->connexions[i].nom, "") != 0)
                              {
                                  MESSAGE add;

                                  
                                  if (tab->connexions[i].pidFenetre != m.expediteur)
                                  {
                                      add.type = tab->connexions[i].pidFenetre;
                                      add.expediteur = getpid();
                                      add.requete = ADD_USER;
                                      strcpy(add.data1, nom);

                                      msgsnd(idQ, &add, sizeof(add)-sizeof(long), 0);
                                      kill(tab->connexions[i].pidFenetre, SIGUSR1);
                                  }

                                  else
                                  {
                                      for (int j = 0; j < 6; j++)
                                      {
                                          if (tab->connexions[j].pidFenetre != 0 &&
                                              strcmp(tab->connexions[j].nom, "") != 0 &&
                                              tab->connexions[j].pidFenetre != m.expediteur)
                                          {
                                              add.type = m.expediteur;
                                              add.expediteur = getpid();
                                              add.requete = ADD_USER;
                                              strcpy(add.data1, tab->connexions[j].nom);

                                              msgsnd(idQ, &add, sizeof(add)-sizeof(long), 0);
                                              kill(m.expediteur, SIGUSR1);
                                          }
                                      }
                                  }
                              }
                          }

                          strcpy(reponse.data1, "OK");
                      }

                      
                      else
                          strcpy(reponse.data1, "KO");

                      reponse.type = m.expediteur;
                      reponse.requete = LOGIN;

                      msgsnd(idQ, &reponse, sizeof(reponse)-sizeof(long), 0);
                      kill(m.expediteur, SIGUSR1);   
                      break;
                  }


      case LOGOUT :  
                      fprintf(stderr,"(SERVEUR %d) Requete LOGOUT reçue de %d\n",getpid(),m.expediteur);
                      
                      for (int i = 0; i < 6; i++)
                          {
                              if (tab->connexions[i].pidFenetre == m.expediteur)
                              {
                                  strcpy(tab->connexions[i].nom, "");
                                  break;
                              }
                          }


                      break;

      case ACCEPT_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete ACCEPT_USER reçue de %d\n",getpid(),m.expediteur);
                      break;

      case REFUSE_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete REFUSE_USER reçue de %d\n",getpid(),m.expediteur);
                      break;

      case SEND :  
                      fprintf(stderr,"(SERVEUR %d) Requete SEND reçue de %d\n",getpid(),m.expediteur);
                      break; 

      case UPDATE_PUB :
                      fprintf(stderr,"(SERVEUR %d) Requete UPDATE_PUB reçue de %d\n",getpid(),m.expediteur);
                      break;

      case CONSULT :
                      fprintf(stderr,"(SERVEUR %d) Requete CONSULT reçue de %d\n",getpid(),m.expediteur);
                      break;

      case MODIF1 :
                      fprintf(stderr,"(SERVEUR %d) Requete MODIF1 reçue de %d\n",getpid(),m.expediteur);
                      break;

      case MODIF2 :
                      fprintf(stderr,"(SERVEUR %d) Requete MODIF2 reçue de %d\n",getpid(),m.expediteur);
                      break;

      case LOGIN_ADMIN :
                      fprintf(stderr,"(SERVEUR %d) Requete LOGIN_ADMIN reçue de %d\n",getpid(),m.expediteur);
                      break;

      case LOGOUT_ADMIN :
                      fprintf(stderr,"(SERVEUR %d) Requete LOGOUT_ADMIN reçue de %d\n",getpid(),m.expediteur);
                      break;

      case NEW_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete NEW_USER reçue de %d : --%s--%s--\n",getpid(),m.expediteur,m.data1,m.data2);
                      break;

      case DELETE_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete DELETE_USER reçue de %d : --%s--\n",getpid(),m.expediteur,m.data1);
                      break;

      case NEW_PUB :
                      fprintf(stderr,"(SERVEUR %d) Requete NEW_PUB reçue de %d\n",getpid(),m.expediteur);
                      break;
    }
    afficheTab();
  }
}

void afficheTab()
{
  fprintf(stderr,"Pid Serveur 1 : %d\n",tab->pidServeur1);
  fprintf(stderr,"Pid Serveur 2 : %d\n",tab->pidServeur2);
  fprintf(stderr,"Pid Publicite : %d\n",tab->pidPublicite);
  fprintf(stderr,"Pid Admin     : %d\n",tab->pidAdmin);
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

