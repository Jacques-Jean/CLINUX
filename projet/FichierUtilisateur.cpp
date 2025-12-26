#include "FichierUtilisateur.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int estPresent(const char* nom)
{
  int fd;
  ssize_t n;
  UTILISATEUR e;
  fd = open(FICHIER_UTILISATEURS, O_RDONLY);
  int pos= 0;
  if (fd == -1)
    return -1;
  
  while (( n = read(fd , &e , sizeof(UTILISATEUR))) > 0){
    if (strcmp(e.nom ,nom )==0){
      close(fd);
      return pos+1;
        
    }
    else pos++;
  }
  
  close(fd);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////
int hash(const char* motDePasse)
{
  int i =0;
  int hash=0;
  while( (motDePasse[i] != '\0')){
    hash+= (i+1)*motDePasse[i];
    i++;
  }
  return hash%97;
}

////////////////////////////////////////////////////////////////////////////////////
void ajouteUtilisateur(const char* nom, const char* motDePasse)
{
  
  fprintf(stderr,">>> ajouteUtilisateur APPELEE pour [%s]\n", nom);
  int fd = open(FICHIER_UTILISATEURS, O_WRONLY | O_APPEND | O_CREAT, 0664);
  if (fd == -1) {
    return;
  }
  
  UTILISATEUR e;
  strcpy(e.nom, nom);
  e.hash = hash(motDePasse);
  int a = write(fd, &e, sizeof(UTILISATEUR));
  close(fd);
}


////////////////////////////////////////////////////////////////////////////////////
int verifieMotDePasse(int pos, const char* motDePasse)
{
  int fd = open(FICHIER_UTILISATEURS, O_RDONLY);
  if (fd==-1){
    return -1;
  }
  if (lseek(fd,(pos-1)*sizeof(UTILISATEUR),SEEK_SET)==-1){
    close(fd);
    return -1;
  }
  UTILISATEUR e;
  ssize_t n = read(fd , &e , sizeof(UTILISATEUR));
  
  close(fd);
  if(n!= sizeof(UTILISATEUR))
    return -1;
  
  if(e.hash==hash(motDePasse)){
    
    return 1;
  }
  else
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////
int listeUtilisateurs(UTILISATEUR *vecteur) // le vecteur doit etre suffisamment grand
{
  int fd;
  ssize_t n;
  UTILISATEUR e;
  fd = open(FICHIER_UTILISATEURS, O_RDONLY);
  int userNumber= 0;
  if (fd == -1)
    return -1;
  
  while (( n = read(fd , &e , sizeof(UTILISATEUR))) > 0){
      
    vecteur[userNumber]=e;
    userNumber++;
      
    
  }
  close(fd);
  return userNumber;
}



////////////////////////////////////////////////////////////////////////////////////
void updateMotDePasse(const char* nom, const char* motDePasse)
{
  int fd = open(FICHIER_UTILISATEURS, O_RDWR);
  if (fd == -1)
    return;

  UTILISATEUR u;
  int pos = 0;

  while (read(fd, &u, sizeof(UTILISATEUR)) == sizeof(UTILISATEUR))
  {
    if (strcmp(u.nom, nom) == 0)
    {
      u.hash = hash(motDePasse);
      lseek(fd, pos * sizeof(UTILISATEUR), SEEK_SET);
      write(fd, &u, sizeof(UTILISATEUR));
      break;
    }
    pos++;
  }

  close(fd);
}