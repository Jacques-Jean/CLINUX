#include "mainwindowtraitement.h"
#include "ui_mainwindowtraitement.h"
#include <signal.h>
#include <unistd.h>
#include <mysql.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>

extern char groupe[30];
extern int  position;
extern MainWindowTraitement* w;

MYSQL      *connexion = nullptr;
MYSQL_RES  *resultat  = nullptr;
MYSQL_ROW  tuple;

void handlerSIGALRM(int sig);
void handlerSIGUSR1(int sig);

volatile sig_atomic_t compteur = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
MainWindowTraitement::MainWindowTraitement(QWidget *parent)
  : QMainWindow(parent), ui(new Ui::MainWindowTraitement)
{
  ui->setupUi(this);
  this->move(1000, position);
  setGroupeTraite(groupe);

  // connexion à la BD et récupération de tous les tuples
  fprintf(stderr, "(Traitement %d) Connection a la BD\n", getpid());
  connexion = mysql_init(NULL);
  mysql_real_connect(connexion, "localhost", "Student", "PassStudent1_",
                     "PourStudent", 0, 0, 0);

  char requete[200];
  sprintf(requete,
          "select * from UNIX_EX3 where groupe='%s' order by nom;",
          groupe);

  mysql_query(connexion, requete);
  resultat = mysql_store_result(connexion);

  // armement de SIGALRM
  fprintf(stderr, "(Traitement %d) Armement du signal SIGALRM\n", getpid());
  struct sigaction A;
  A.sa_handler = handlerSIGALRM;
  A.sa_flags   = 0;
  sigemptyset(&A.sa_mask);
  sigaction(SIGALRM, &A, NULL);

  // armement de SIGUSR1
  fprintf(stderr, "(Traitement %d) Armement du signal SIGUSR1\n", getpid());
  struct sigaction B;
  B.sa_handler = handlerSIGUSR1;
  B.sa_flags   = 0;
  sigemptyset(&B.sa_mask);
  sigaction(SIGUSR1, &B, NULL);

  // Demande d'envoi de SIGALRM dans 1 seconde
  alarm(1);
}

MainWindowTraitement::~MainWindowTraitement()
{
  delete ui;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindowTraitement::setGroupeTraite(const char* Text)
{
  if (strlen(Text) == 0)
  {
    ui->lineEditGroupeTraite->clear();
    return;
  }
  ui->lineEditGroupeTraite->setText(Text);
}

void MainWindowTraitement::setEtudiantTraite(const char* Text)
{
  if (strlen(Text) == 0)
  {
    ui->lineEditEtudiantTraite->clear();
    return;
  }
  ui->lineEditEtudiantTraite->setText(Text);
}

void MainWindowTraitement::setDejaTraites(int nb)
{
  ui->lcdNumberDejaTraites->display(nb);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handlerSIGALRM(int sig)
{
  fprintf(stderr,
          "(Traitement %d) Reception du signal SIGALRM (%d)\n",
          getpid(), sig);

  if ((tuple = mysql_fetch_row(resultat)) == NULL)
    exit(compteur);

  compteur++;
  w->setEtudiantTraite(tuple[1]);
  w->setDejaTraites(compteur);

  alarm(1);
}

void handlerSIGUSR1(int sig)
{
  fprintf(stderr,
          "(Traitement %d) Reception du signal SIGUSR1 (%d) - Arret du traitement\n",
          getpid(), sig);

  alarm(0); // stop SIGALRM

  if (resultat) mysql_free_result(resultat);
  if (connexion) mysql_close(connexion);

  _exit(compteur % 256); // return usable value to parent
}
