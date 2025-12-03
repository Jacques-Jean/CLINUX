#include "mainwindowex4.h"
#include "ui_mainwindowex4.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>       // Pour sigaction, sigemptyset, SIGCHLD
#include <sys/wait.h>    // Pour waitpid et macros WIFEXITED, WEXITSTATUS
#include <string.h>

extern MainWindowEx4 *w;

int idFils1,idFils2,idFils3;   // PID du fils 1
void HandlerSIGCHLD(int);
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
MainWindowEx4::MainWindowEx4(QWidget *parent)
: QMainWindow(parent), ui(new Ui::MainWindowEx4)
{
ui->setupUi(this);
ui->pushButtonAnnulerTous->setVisible(false);


// Armement de SIGCHLD
struct sigaction A;
A.sa_handler = HandlerSIGCHLD;   // <-- maintenant ça compile
sigemptyset(&A.sa_mask);
A.sa_flags = 0;
if (sigaction(SIGCHLD, &A, NULL) == -1)
{
    perror("Erreur de sigaction");
    exit(1);
}


}

MainWindowEx4::~MainWindowEx4()
{
delete ui;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions utiles /////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindowEx4::setGroupe1(const char* Text)
{
if (strlen(Text) == 0 )
{
ui->lineEditGroupe1->clear();
return;
}
ui->lineEditGroupe1->setText(Text);
}

void MainWindowEx4::setGroupe2(const char* Text)
{
if (strlen(Text) == 0 )
{
ui->lineEditGroupe2->clear();
return;
}
ui->lineEditGroupe2->setText(Text);
}

void MainWindowEx4::setGroupe3(const char* Text)
{
if (strlen(Text) == 0 )
{
ui->lineEditGroupe3->clear();
return;
}
ui->lineEditGroupe3->setText(Text);
}

void MainWindowEx4::setResultat1(int nb)
{
char Text[20];
sprintf(Text,"%d",nb);
ui->lineEditResultat1->setText(Text);
}

void MainWindowEx4::setResultat2(int nb)
{
char Text[20];
sprintf(Text,"%d",nb);
ui->lineEditResultat2->setText(Text);
}

void MainWindowEx4::setResultat3(int nb)
{
char Text[20];
sprintf(Text,"%d",nb);
ui->lineEditResultat3->setText(Text);
}

bool MainWindowEx4::traitement1Selectionne()
{
return ui->checkBoxTraitement1->isChecked();
}

bool MainWindowEx4::traitement2Selectionne()
{
return ui->checkBoxTraitement2->isChecked();
}

bool MainWindowEx4::traitement3Selectionne()
{
return ui->checkBoxTraitement3->isChecked();
}

const char* MainWindowEx4::getGroupe1()
{
static char groupe1[100];
if (ui->lineEditGroupe1->text().size())
{
strcpy(groupe1, ui->lineEditGroupe1->text().toStdString().c_str());
return groupe1;
}
return NULL;
}

const char* MainWindowEx4::getGroupe2()
{
static char groupe2[100];
if (ui->lineEditGroupe2->text().size())
{
strcpy(groupe2, ui->lineEditGroupe2->text().toStdString().c_str());
return groupe2;
}
return NULL;
}

const char* MainWindowEx4::getGroupe3()
{
static char groupe3[100];
if (ui->lineEditGroupe3->text().size())
{
strcpy(groupe3, ui->lineEditGroupe3->text().toStdString().c_str());
return groupe3;
}
return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Clic sur le bouton « Démarrer » ////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindowEx4::on_pushButtonDemarrerTraitements_clicked() {
if (traitement1Selectionne()) {
    if ((idFils1 = fork()) == -1) { perror("Erreur de fork"); exit(1); }
    if (idFils1 == 0) { // fils 1
        const char* groupe = getGroupe1();
        execl("./Traitement", "Traitement", groupe, "200", (char*)NULL);
        perror("Erreur exec");
        exit(1);
    }
}

if (traitement2Selectionne()) {
    if ((idFils2 = fork()) == -1) { perror("Erreur de fork"); exit(1); }
    if (idFils2 == 0) { // fils 2
        const char* groupe = getGroupe2();
        execl("./Traitement", "Traitement", groupe, "350", (char*)NULL);
        perror("Erreur exec");
        exit(1);
    }
}


if (traitement3Selectionne()) {
    if ((idFils3 = fork()) == -1) { perror("Erreur de fork"); exit(1); }
    if (idFils3 == 0) { // fils 2
        const char* groupe = getGroupe3();
        execl("./Traitement", "Traitement", groupe, "700", (char*)NULL);
        perror("Erreur exec");
        exit(1);
    }
}



}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Autres boutons //////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindowEx4::on_pushButtonVider_clicked()
{
  setGroupe1("");
  setGroupe2("");
  setGroupe3("");


  ui->lineEditResultat1->clear();
  ui->lineEditResultat2->clear();
  ui->lineEditResultat3->clear();

  ui->lineEditGroupe1->clear();
  ui->lineEditGroupe2->clear();
  ui->lineEditGroupe3->clear();


}

void MainWindowEx4::on_pushButtonQuitter_clicked()
{

  
  close();
}

void MainWindowEx4::on_pushButtonAnnuler1_clicked()
{
  if (idFils1 > 0)
  {
    kill(idFils1, SIGKILL);
    printf("Processus fils 1 tué\n");
  }
}

void MainWindowEx4::on_pushButtonAnnuler2_clicked()
{
  if (idFils2 > 0)
  {
    kill(idFils2, SIGKILL);
  }
}

void MainWindowEx4::on_pushButtonAnnuler3_clicked()
{
  if (idFils3 > 0)
  {
    kill(idFils3, SIGKILL);
  }
}

void MainWindowEx4::on_pushButtonAnnulerTous_clicked()
{
  if (idFils1 > 0)
  {
    kill(idFils1, SIGKILL);
  }
  
  if (idFils2 > 0)
  {
    kill(idFils2, SIGKILL);
  }
  
  if (idFils3 > 0)
  {
    kill(idFils3, SIGKILL);
  }


}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Handler SIGCHLD /////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void HandlerSIGCHLD(int)
{
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (WIFEXITED(status))
        {
            int code = WEXITSTATUS(status);
            printf("Processus %d terminé avec code %d\n", pid, code);

            if ((pid == idFils1)&& (pid != 0))
                w->setResultat1(code);
            if ((pid == idFils2) && (pid != 0))
                w->setResultat2(code);

            if ((pid == idFils3) && (pid != 0))
                w->setResultat3(code);
        }
    }
}
