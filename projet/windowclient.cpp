#include "windowclient.h"
#include "ui_windowclient.h"
#include <QMessageBox>
#include "dialogmodification.h"

#include <sys/msg.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <QTimer>

#include <signal.h>

extern WindowClient *w;

#include "protocole.h"

int idQ, idShm;
#define TIME_OUT 120
int timeOut = TIME_OUT;

void handlerSIGUSR1(int sig);
void handlerSIGUSR2(int sig);

void handlerSIGINT (int sig);

void handlerSIGALRM(int sig);
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
WindowClient::WindowClient(QWidget *parent):QMainWindow(parent),ui(new Ui::WindowClient)
{
    ui->setupUi(this);
    ::close(2);
    logoutOK();

    // Recuperation de l'identifiant de la file de messages
    fprintf(stderr,"(CLIENT %d) Recuperation de l'id de la file de messages\n",getpid());              

    idQ = msgget(CLE, 0);
    if (idQ == -1)
    {
        perror("msgget");
        exit(1);
    }

    idShm = shmget(CLE, 0, 0);
    if (idShm == -1) 
      { 
        perror("shmget client"); exit(1); 
      }

    

    // Recuperation de l'identifiant de la mémoire partagée
    fprintf(stderr,"(CLIENT %d) Recuperation de l'id de la mémoire partagée\n",getpid());

    // Attachement à la mémoire partagée

    // Armement des signaux
    struct sigaction A;
    A.sa_handler = handlerSIGUSR1;
    sigemptyset(&A.sa_mask);
    A.sa_flags = 0;
    sigaction(SIGUSR1, &A, NULL);


    struct sigaction B;
    B.sa_handler = handlerSIGUSR2;
    sigemptyset(&B.sa_mask);
    B.sa_flags = 0;
    sigaction(SIGUSR2, &B, NULL);

    struct sigaction SA;
    SA.sa_handler = handlerSIGINT;
    sigemptyset(&SA.sa_mask);
    SA.sa_flags = 0;
    sigaction(SIGINT, &SA, NULL);

    signal(SIGALRM, handlerSIGALRM);

    signal(SIGUSR2, handlerSIGUSR2);

    // Envoi d'une requete de connexion au serveur
    MESSAGE M;
    M.type = 1;                
    M.expediteur = getpid();
    M.requete = CONNECT;
    

    if (msgsnd(idQ, &M, sizeof(M) - sizeof(long), 0) == -1)
    {
        perror("msgsnd identification");
        exit(1);
    }
}

WindowClient::~WindowClient()
{
    delete ui;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions utiles : ne pas modifier /////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setNom(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditNom->clear();
    return;
  }
  ui->lineEditNom->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getNom()
{
  strcpy(connectes[0],ui->lineEditNom->text().toStdString().c_str());
  return connectes[0];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setMotDePasse(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditMotDePasse->clear();
    return;
  }
  ui->lineEditMotDePasse->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getMotDePasse()
{
  strcpy(motDePasse,ui->lineEditMotDePasse->text().toStdString().c_str());
  return motDePasse;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
int WindowClient::isNouveauChecked()
{
  if (ui->checkBoxNouveau->isChecked()) return 1;
  return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setPublicite(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditPublicite->clear();
    return;
  }
  ui->lineEditPublicite->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setTimeOut(int nb)
{
  ui->lcdNumberTimeOut->display(nb);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setAEnvoyer(const char* Text)
{
  //fprintf(stderr,"---%s---\n",Text);
  if (strlen(Text) == 0 )
  {
    ui->lineEditAEnvoyer->clear();
    return;
  }
  ui->lineEditAEnvoyer->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getAEnvoyer()
{
  strcpy(aEnvoyer,ui->lineEditAEnvoyer->text().toStdString().c_str());
  return aEnvoyer;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setPersonneConnectee(int i,const char* Text)
{
  if (strlen(Text) == 0 )
  {
    switch(i)
    {
        case 1 : ui->lineEditConnecte1->clear(); break;
        case 2 : ui->lineEditConnecte2->clear(); break;
        case 3 : ui->lineEditConnecte3->clear(); break;
        case 4 : ui->lineEditConnecte4->clear(); break;
        case 5 : ui->lineEditConnecte5->clear(); break;
    }
    return;
  }
  switch(i)
  {
      case 1 : ui->lineEditConnecte1->setText(Text); break;
      case 2 : ui->lineEditConnecte2->setText(Text); break;
      case 3 : ui->lineEditConnecte3->setText(Text); break;
      case 4 : ui->lineEditConnecte4->setText(Text); break;
      case 5 : ui->lineEditConnecte5->setText(Text); break;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getPersonneConnectee(int i)
{
  QLineEdit *tmp;
  switch(i)
  {
    case 1 : tmp = ui->lineEditConnecte1; break;
    case 2 : tmp = ui->lineEditConnecte2; break;
    case 3 : tmp = ui->lineEditConnecte3; break;
    case 4 : tmp = ui->lineEditConnecte4; break;
    case 5 : tmp = ui->lineEditConnecte5; break;
    default : return NULL;
  }

  strcpy(connectes[i],tmp->text().toStdString().c_str());
  return connectes[i];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::ajouteMessage(const char* personne,const char* message)
{
  // Choix de la couleur en fonction de la position
  int i=1;
  bool trouve=false;
  while (i<=5 && !trouve)
  {
      if (getPersonneConnectee(i) != NULL && strcmp(getPersonneConnectee(i),personne) == 0) trouve = true;
      else i++;
  }
  char couleur[40];
  if (trouve)
  {
      switch(i)
      {
        case 1 : strcpy(couleur,"<font color=\"red\">"); break;
        case 2 : strcpy(couleur,"<font color=\"blue\">"); break;
        case 3 : strcpy(couleur,"<font color=\"green\">"); break;
        case 4 : strcpy(couleur,"<font color=\"darkcyan\">"); break;
        case 5 : strcpy(couleur,"<font color=\"orange\">"); break;
      }
  }
  else strcpy(couleur,"<font color=\"black\">");
  if (strcmp(getNom(),personne) == 0) strcpy(couleur,"<font color=\"purple\">");

  // ajout du message dans la conversation
  char buffer[300];
  sprintf(buffer,"%s(%s)</font> %s",couleur,personne,message);
  ui->textEditConversations->append(buffer);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setNomRenseignements(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditNomRenseignements->clear();
    return;
  }
  ui->lineEditNomRenseignements->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getNomRenseignements()
{
  strcpy(nomR,ui->lineEditNomRenseignements->text().toStdString().c_str());
  return nomR;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setGsm(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditGsm->clear();
    return;
  }
  ui->lineEditGsm->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setEmail(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditEmail->clear();
    return;
  }
  ui->lineEditEmail->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setCheckbox(int i,bool b)
{
  QCheckBox *tmp;
  switch(i)
  {
    case 1 : tmp = ui->checkBox1; break;
    case 2 : tmp = ui->checkBox2; break;
    case 3 : tmp = ui->checkBox3; break;
    case 4 : tmp = ui->checkBox4; break;
    case 5 : tmp = ui->checkBox5; break;
    default : return;
  }
  tmp->setChecked(b);
  if (b) tmp->setText("Accepté");
  else tmp->setText("Refusé");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::loginOK()
{
  ui->pushButtonLogin->setEnabled(false);
  ui->pushButtonLogout->setEnabled(true);
  ui->lineEditNom->setReadOnly(true);
  ui->lineEditMotDePasse->setReadOnly(true);
  ui->checkBoxNouveau->setEnabled(false);
  ui->pushButtonEnvoyer->setEnabled(true);
  ui->pushButtonConsulter->setEnabled(true);
  ui->pushButtonModifier->setEnabled(true);
  ui->checkBox1->setEnabled(true);
  ui->checkBox2->setEnabled(true);
  ui->checkBox3->setEnabled(true);
  ui->checkBox4->setEnabled(true);
  ui->checkBox5->setEnabled(true);
  ui->lineEditAEnvoyer->setEnabled(true);
  ui->lineEditNomRenseignements->setEnabled(true);
  setTimeOut(TIME_OUT);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::logoutOK()
{
  ui->pushButtonLogin->setEnabled(true);
  ui->pushButtonLogout->setEnabled(false);
  ui->lineEditNom->setReadOnly(false);
  ui->lineEditNom->setText("");
  ui->lineEditMotDePasse->setReadOnly(false);
  ui->lineEditMotDePasse->setText("");
  ui->checkBoxNouveau->setEnabled(true);
  ui->pushButtonEnvoyer->setEnabled(false);
  ui->pushButtonConsulter->setEnabled(false);
  ui->pushButtonModifier->setEnabled(false);
  for (int i=1 ; i<=5 ; i++)
  {
      setCheckbox(i,false);
      setPersonneConnectee(i,"");
  }
  ui->checkBox1->setEnabled(false);
  ui->checkBox2->setEnabled(false);
  ui->checkBox3->setEnabled(false);
  ui->checkBox4->setEnabled(false);
  ui->checkBox5->setEnabled(false);
  setNomRenseignements("");
  setGsm("");
  setEmail("");
  ui->textEditConversations->clear();
  setAEnvoyer("");
  ui->lineEditAEnvoyer->setEnabled(false);
  ui->lineEditNomRenseignements->setEnabled(false);
  setTimeOut(TIME_OUT);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions permettant d'afficher des boites de dialogue /////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::dialogueMessage(const char* titre,const char* message)
{
   QMessageBox::information(this,titre,message);
}
//sdfq
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::dialogueErreur(const char* titre,const char* message)
{
   QMessageBox::critical(this,titre,message);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Clic sur la croix de la fenêtre ////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::closeEvent(QCloseEvent *event)
{

    MESSAGE mlogout ;
    mlogout.type=1;
    mlogout.expediteur= getpid();
    mlogout.requete = LOGOUT ;

    if (msgsnd(idQ, &mlogout, sizeof(mlogout) - sizeof(long), 0) == -1)
    {
        perror("msgsnd logout");
        exit(1);
    }

    MESSAGE M;
    M.type = 1;             
    M.expediteur = getpid(); 
    M.requete = DECONNECT;   

    if (msgsnd(idQ, &M, sizeof(M) - sizeof(long), 0) == -1)
    {
        perror("msgsnd DECONNECT");
    }
    QApplication::exit();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions clics sur les boutons ////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::on_pushButtonLogin_clicked()
{
    
  MESSAGE mconnect;
  mconnect.type = 1; 
  mconnect.expediteur = getpid();
  strcpy(mconnect.data2,getNom());
  strcpy(mconnect.texte,getMotDePasse());
  strcpy(mconnect.data1, isNouveauChecked() == 0 ? "0" : "1");


  mconnect.requete = LOGIN ;     
    
  if (msgsnd(idQ, &mconnect, sizeof(mconnect) - sizeof(long), 0) == -1)
    {
        perror("msgsnd login");
        exit(1);
    }

}

void WindowClient::on_pushButtonLogout_clicked()
{
    // TO DO
    MESSAGE mlogout ;
    mlogout.type=1;
    mlogout.expediteur= getpid();
    mlogout.requete = LOGOUT ;

    if (msgsnd(idQ, &mlogout, sizeof(mlogout) - sizeof(long), 0) == -1)
    {
        perror("msgsnd logout");
        exit(1);
    }

    logoutOK();
}

void WindowClient::on_pushButtonEnvoyer_clicked()
{
    w->resetTimeOut();


    if (strlen(getAEnvoyer()) == 0) 
      return;

    MESSAGE m;
    m.type = 1;
    m.expediteur = getpid();
    m.requete = SEND;
    strcpy(m.texte, getAEnvoyer());

    msgsnd(idQ, &m, sizeof(m)-sizeof(long), 0);

    setAEnvoyer(""); 
}


void WindowClient::on_pushButtonConsulter_clicked()
{
  w->resetTimeOut();
  MESSAGE m;
  m.type = 1;
  m.expediteur = getpid();
  m.requete = CONSULT;
  strcpy(m.data1, getNomRenseignements());
  msgsnd(idQ, &m, sizeof(m)-sizeof(long), 0);


}


void WindowClient::on_pushButtonModifier_clicked()
{
    w->resetTimeOut();

    MESSAGE m;
    m.type       = 1;
    m.expediteur = getpid();
    m.requete    = MODIF1;
    /* nom courant = getNom() */
    strcpy(m.data1, getNom());
    m.data2[0] = 0;
    m.texte[0] = 0;

    if (msgsnd(idQ, &m, sizeof(MESSAGE)-sizeof(long), 0) == -1)
    {
        perror("msgsnd MODIF1");
        return;
    }

    fprintf(stderr,"(CLIENT %d) Attente reponse MODIF1\n",getpid());
    if (msgrcv(idQ, &m, sizeof(MESSAGE)-sizeof(long), getpid(), 0) == -1)
    {
        perror("msgrcv MODIF1");
        return;
    }

    if (!strcmp(m.data1,"KO") &&
        !strcmp(m.data2,"KO") &&
        !strcmp(m.texte,"KO"))
    {
        QMessageBox::critical(w,"Problème...","Modification déjà en cours...");
        return;
    }

    DialogModification dialogue(this,getNom(),"",m.data2,m.texte);
    dialogue.exec();

    char motDePasse[40];
    char gsm[40];
    char email[40];

    strcpy(motDePasse, dialogue.getMotDePasse());
    strcpy(gsm,        dialogue.getGsm());
    strcpy(email,      dialogue.getEmail());

    MESSAGE m2;
    m2.type       = 1;
    m2.expediteur = getpid();
    m2.requete    = MODIF2;
    strcpy(m2.data1, motDePasse);  // mot de passe (vide = pas de changement)
    strcpy(m2.data2, gsm);
    strcpy(m2.texte, email);

    msgsnd(idQ, &m2, sizeof(MESSAGE)-sizeof(long), 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions clics sur les checkbox ///////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::on_checkBox1_clicked(bool checked)
{

    w->resetTimeOut();
    if (checked)
    {
        ui->checkBox1->setText("Accepté");
        MESSAGE mAccept;
        mAccept.type = 1;
        mAccept.expediteur = getpid();
        mAccept.requete = ACCEPT_USER;
        strcpy(mAccept.data1, getPersonneConnectee(1));

        msgsnd(idQ, &mAccept, sizeof(mAccept)-sizeof(long), 0);
    }
    else
    {
        ui->checkBox1->setText("Refusé");
        if (strlen(getPersonneConnectee(1)) > 0)
        {
            MESSAGE mrefu;
            mrefu.type = 1;
            mrefu.expediteur = getpid();
            mrefu.requete = REFUSE_USER;
            strcpy(mrefu.data1, getPersonneConnectee(1));

            msgsnd(idQ, &mrefu, sizeof(mrefu)-sizeof(long), 0);
        }
    }
}

void WindowClient::on_checkBox2_clicked(bool checked)
{

    w->resetTimeOut();
    if (checked)
    {
        ui->checkBox2->setText("Accepté");
        MESSAGE mAccept;
        mAccept.type = 1;
        mAccept.expediteur = getpid();
        mAccept.requete = ACCEPT_USER;
        strcpy(mAccept.data1, getPersonneConnectee(2));

        msgsnd(idQ, &mAccept, sizeof(mAccept)-sizeof(long), 0);
    }
    else
    {
        ui->checkBox2->setText("Refusé");
        if (strlen(getPersonneConnectee(2)) > 0)
        {
            MESSAGE mrefu;
            mrefu.type = 1;
            mrefu.expediteur = getpid();
            mrefu.requete = REFUSE_USER;
            strcpy(mrefu.data1, getPersonneConnectee(2));

            msgsnd(idQ, &mrefu, sizeof(mrefu)-sizeof(long), 0);
        }
    }
}

void WindowClient::on_checkBox3_clicked(bool checked)
{

    w->resetTimeOut();

    if (checked)
    {
        ui->checkBox3->setText("Accepté");
        MESSAGE mAccept;
        mAccept.type = 1;
        mAccept.expediteur = getpid();
        mAccept.requete = ACCEPT_USER;
        strcpy(mAccept.data1, getPersonneConnectee(3));

        msgsnd(idQ, &mAccept, sizeof(mAccept)-sizeof(long), 0);
    }
    else
    {
        ui->checkBox3->setText("Refusé");
        if (strlen(getPersonneConnectee(3)) > 0)
        {
            MESSAGE mrefu;
            mrefu.type = 1;
            mrefu.expediteur = getpid();
            mrefu.requete = REFUSE_USER;
            strcpy(mrefu.data1, getPersonneConnectee(3));

            msgsnd(idQ, &mrefu, sizeof(mrefu)-sizeof(long), 0);
        }
    }
}

void WindowClient::on_checkBox4_clicked(bool checked)
{

    w->resetTimeOut();

    if (checked)
    {
        ui->checkBox4->setText("Accepté");
        MESSAGE mAccept;
        mAccept.type = 1;
        mAccept.expediteur = getpid();
        mAccept.requete = ACCEPT_USER;
        strcpy(mAccept.data1, getPersonneConnectee(4));

        msgsnd(idQ, &mAccept, sizeof(mAccept)-sizeof(long), 0);
    }
    else
    {
        ui->checkBox4->setText("Refusé");
        if (strlen(getPersonneConnectee(4)) > 0)
        {
            MESSAGE mrefu;
            mrefu.type = 1;
            mrefu.expediteur = getpid();
            mrefu.requete = REFUSE_USER;
            strcpy(mrefu.data1, getPersonneConnectee(4));

            msgsnd(idQ, &mrefu, sizeof(mrefu)-sizeof(long), 0);
        }
    }
}

void WindowClient::on_checkBox5_clicked(bool checked)
{
    w->resetTimeOut();


    if (checked)
    {
        ui->checkBox5->setText("Accepté");
        MESSAGE mAccept;
        mAccept.type = 1;
        mAccept.expediteur = getpid();
        mAccept.requete = ACCEPT_USER;
        strcpy(mAccept.data1, getPersonneConnectee(5));

        msgsnd(idQ, &mAccept, sizeof(mAccept)-sizeof(long), 0);
    }
    else
    {
        ui->checkBox5->setText("Refusé");
        if (strlen(getPersonneConnectee(5)) > 0)
        {
            MESSAGE mrefu;
            mrefu.type = 1;
            mrefu.expediteur = getpid();
            mrefu.requete = REFUSE_USER;
            strcpy(mrefu.data1, getPersonneConnectee(5));

            msgsnd(idQ, &mrefu, sizeof(mrefu)-sizeof(long), 0);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Handlers de signaux ////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handlerSIGUSR1(int sig)
{
    MESSAGE m;
    while(msgrcv(idQ, &m, sizeof(MESSAGE)-sizeof(long), getpid(), IPC_NOWAIT) != -1) {
       // printf("==== MESSAGE RECU ====\n");
       //  printf("Type (PID destinataire) : %ld\n", (long)m.type);
       //  printf("Expediteur (PID)       : %d\n", m.expediteur);
       //  printf("Requete                : %d\n", m.requete);
       //  printf("Data1                  : %s\n", m.data1);
       //  printf("Data2                  : %s\n", m.data2);
       //  printf("Texte                  : %s\n", m.texte);
       //  printf("=======================\n");
      switch(m.requete)
      {
        case LOGIN :
                    {
                    if (strcmp(m.data1,"OK") == 0)
                    {
                      fprintf(stderr,"(CLIENT %d) Login OK\n",getpid());
                      w->loginOK();
                      w->dialogueMessage("Login...",m.texte);

                      timeOut = TIME_OUT;
                      w->setTimeOut(timeOut);
                      alarm(1);
                      
                    }
                    else w->dialogueErreur("Login...",m.texte);
                    break;
                    }
        case ADD_USER:
                {
                    bool dejaPresent = false;

                    for (int i = 1; i <= 5; i++)
                    {
                        if (strcmp(w->getPersonneConnectee(i), m.data1) == 0)
                        {
                            dejaPresent = true;
                            break;
                        }
                    }

                    if (!dejaPresent)
                    {
                        for (int i = 1; i <= 5; i++)
                        {
                            if (strlen(w->getPersonneConnectee(i)) == 0)
                            {
                                w->setPersonneConnectee(i, m.data1);
                                break;
                            }
                        }
                    }
                    break;
                }


        case REMOVE_USER:
                  {
                      for (int i = 1; i <= 5; i++)
                      {
                          if (strcmp(w->getPersonneConnectee(i), m.data1) == 0)
                          {
                              w->setPersonneConnectee(i, "");
                              break;
                          }
                      }
                      break;
                  }

        case SEND:
                  {
                      // m.data1 = nom de l'expéditeur
                      // m.texte = message
                      w->ajouteMessage(m.data1, m.texte);
                      break;
                  }

        case CONSULT:
                      fprintf(stderr,"testtes");
                      if (strcmp(m.data1, "OK") == 0) {
                          w->setGsm(m.data2);
                          w->setEmail(m.texte);
                      } else {
                          w->setGsm("NON TROUVE");
                          w->setEmail("NON TROUVE");
                      }
                      break;

      }
    }
}
void handlerSIGUSR2(int sig)
{
    (void)sig;

    char* pubTexte = (char*)shmat(idShm, NULL, 0);
    if (pubTexte == (char*)-1)
        return;

    w->setPublicite(pubTexte);

    shmdt(pubTexte);  
}



void handlerSIGINT(int sig)
{
    MESSAGE mlogout;
    mlogout.type = 1;
    mlogout.expediteur = getpid();
    mlogout.requete = LOGOUT;

    msgsnd(idQ, &mlogout, sizeof(mlogout) - sizeof(long), 0);

    MESSAGE m;
    m.type = 1;
    m.expediteur = getpid();
    m.requete = DECONNECT;

    msgsnd(idQ, &m, sizeof(m) - sizeof(long), 0);

    QApplication::exit();
}

void handlerSIGALRM(int sig)
{
    timeOut--;
    w->setTimeOut(timeOut);

    if (timeOut <= 0)
    {
        MESSAGE m;
        m.type = 1;
        m.expediteur = getpid();
        m.requete = LOGOUT;

        msgsnd(idQ, &m, sizeof(m)-sizeof(long), 0);

        w->logoutOK();
        alarm(0); 
        return;
    }

    alarm(1);
}


void WindowClient::resetTimeOut()
{
    alarm(0);             
    timeOut = TIME_OUT;
    setTimeOut(timeOut);
    alarm(1);
}


