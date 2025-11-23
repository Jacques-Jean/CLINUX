#include "mainwindowex3.h"
#include "ui_mainwindowex3.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

MainWindowEx3::MainWindowEx3(QWidget *parent):QMainWindow(parent),ui(new Ui::MainWindowEx3)
{
    ui->setupUi(this);
}

MainWindowEx3::~MainWindowEx3()
{
    delete ui;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions utiles : ne pas modifier /////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindowEx3::setGroupe1(const char* Text)
{
  fprintf(stderr,"---%s---\n",Text);
  if (strlen(Text) == 0 )
  {
    ui->lineEditGroupe1->clear();
    return;
  }
  ui->lineEditGroupe1->setText(Text);
}

void MainWindowEx3::setGroupe2(const char* Text)
{
  fprintf(stderr,"---%s---\n",Text);
  if (strlen(Text) == 0 )
  {
    ui->lineEditGroupe2->clear();
    return;
  }
  ui->lineEditGroupe2->setText(Text);
}

void MainWindowEx3::setGroupe3(const char* Text)
{
  fprintf(stderr,"---%s---\n",Text);
  if (strlen(Text) == 0 )
  {
    ui->lineEditGroupe3->clear();
    return;
  }
  ui->lineEditGroupe3->setText(Text);
}

void MainWindowEx3::setResultat1(int nb)
{
  char Text[20];
  sprintf(Text,"%d",nb);
  fprintf(stderr,"---%s---\n",Text);
  if (strlen(Text) == 0 )
  {
    ui->lineEditResultat1->clear();
    return;
  }
  ui->lineEditResultat1->setText(Text);
}

void MainWindowEx3::setResultat2(int nb)
{
  char Text[20];
  sprintf(Text,"%d",nb);
  fprintf(stderr,"---%s---\n",Text);
  if (strlen(Text) == 0 )
  {
    ui->lineEditResultat2->clear();
    return;
  }
  ui->lineEditResultat2->setText(Text);
}

void MainWindowEx3::setResultat3(int nb)
{
  char Text[20];
  sprintf(Text,"%d",nb);
  fprintf(stderr,"---%s---\n",Text);
  if (strlen(Text) == 0 )
  {
    ui->lineEditResultat3->clear();
    return;
  }
  ui->lineEditResultat3->setText(Text);
}

bool MainWindowEx3::recherche1Selectionnee()
{
  return ui->checkBoxRecherche1->isChecked();
}

bool MainWindowEx3::recherche2Selectionnee()
{
  return ui->checkBoxRecherche2->isChecked();
}

bool MainWindowEx3::recherche3Selectionnee()
{
  return ui->checkBoxRecherche3->isChecked();
}

const char* MainWindowEx3::getGroupe1()
{
  if (ui->lineEditGroupe1->text().size())
  { 
    strcpy(groupe1,ui->lineEditGroupe1->text().toStdString().c_str());
    return groupe1;
  }
  return NULL;
}

const char* MainWindowEx3::getGroupe2()
{
  if (ui->lineEditGroupe2->text().size())
  { 
    strcpy(groupe2,ui->lineEditGroupe2->text().toStdString().c_str());
    return groupe2;
  }
  return NULL;
}

const char* MainWindowEx3::getGroupe3()
{
  if (ui->lineEditGroupe3->text().size())
  { 
    strcpy(groupe3,ui->lineEditGroupe3->text().toStdString().c_str());
    return groupe3;
  }
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions clics sur les boutons ////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>     
#include <unistd.h>

 void MainWindowEx3::on_pushButtonLancerRecherche_clicked()
{


    int fd = open("Trace.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
if (fd == -1) {
    perror("Erreur open Trace.log");
    return;
}

dup2(fd, STDERR_FILENO);
::close(fd);   // IMPORTANT : éviter le conflit avec QWidget::close()
    // ----------------------------------------

    fprintf(stderr,"Clic sur le bouton Lancer Recherche\n");

    int pidFils[3] = {-1, -1, -1};
    const char* groupe[3];
    int nbFils = 0;

    // RECHERCHE 1
    if (recherche1Selectionnee())
    {
        groupe[0] = getGroupe1();
        pidFils[0] = fork();

        if (pidFils[0] == 0) {
            execl("./Lecture", "Lecture", groupe[0], (char*)NULL);
            perror("exec 1");
            exit(1);
        }
        nbFils++;
    }

    // RECHERCHE 2
    if (recherche2Selectionnee())
    {
        groupe[1] = getGroupe2();
        pidFils[1] = fork();

        if (pidFils[1] == 0) {
            execl("./Lecture", "Lecture", groupe[1], (char*)NULL);
            perror("exec 2");
            exit(1);
        }
        nbFils++;
    }

    // RECHERCHE 3
    if (recherche3Selectionnee())
    {
        groupe[2] = getGroupe3();
        pidFils[2] = fork();

        if (pidFils[2] == 0) {
            execl("./Lecture", "Lecture", groupe[2], (char*)NULL);
            perror("exec 3");
            exit(1);
        }
        nbFils++;
    }

    // ---------- LE PÈRE ATTEND SES FILS ----------
    for (int i = 0; i < nbFils; i++)
    {
        int status;
        int pid = wait(&status);
        int exitValue = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

        // Associer en testant si le pid correspond
        if (pid == pidFils[0]) setResultat1(exitValue);
        if (pid == pidFils[1]) setResultat2(exitValue);
        if (pid == pidFils[2]) setResultat3(exitValue);
    }
}


void MainWindowEx3::on_pushButtonVider_clicked()
{
    setGroupe1("");
    setGroupe2("");
    setGroupe3("");

    ui->lineEditResultat1->clear();
    ui->lineEditResultat2->clear();
    ui->lineEditResultat3->clear();
}



void MainWindowEx3::on_pushButtonQuitter_clicked()
{
    close();   // ferme proprement l’application Qt
}
