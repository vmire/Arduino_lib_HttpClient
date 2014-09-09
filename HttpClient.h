#ifndef HttpClient_h
#define HttpClient_h

#include "Arduino.h"
#include "Client.h"

class HttpClient{

public:
	HttpClient(Client& cl);
	
	//Les status que peut prendre la connection HTTP
	#define HTTP_STATUS_IDLE 0              //Aucune requete http en cours
	#define HTTP_STATUS_RESP_WAIT 1         //En attente de la réponse
	#define HTTP_STATUS_RESP_BEGIN 2        //Au début de la réponse (avant HTTP/1.1)
	#define HTTP_STATUS_RESP_HEADER 3       //Au début d'une ligne d'en-tête
	#define HTTP_STATUS_RESP_HEADER_VALUE 4 //Ligne d'en-tête, après le ':' (clé déjà lue)
	#define HTTP_STATUS_RESP_BODY 5         //corps de la réponse
	#define HTTP_STATUS_ERROR 6             //Une erreur est survenue
	#define HTTP_STATUS_EOF 7               //La fin de fichier a été atteinte
	
	//Les codes retours possibles (négatifs pour les différencier du nombre de caractères que peuvent renvoyer certaines fonctions)
	#define HTTP_ERROR -1					
	#define HTTP_RESP_END_OF_HEARDERS -2    //Fin des en-têtes
	#define HTTP_RESP_EOF -3                //Fin de réponse
	#define HTTP_ERROR_TIMEOUT -4;
	#define HTTP_ERROR_CONNECTION_FAILED -5
	
	#define HTTP_DEFAULT_TIMEOUT 3000;		//Timeout http par défaut
	
	int sendGet(char* server, int port, char* path);
	
	int http_resp_read_code();
	int http_resp_read_next_header_key(char* key,int maxlen);
	int http_resp_read_next_header_value(char* value,int maxlen);
	int httpRespMoveToEOL();
	int httpWaitForResponse();
	int httpRead();

protected:
	Client* client;					//Interface Client (ethernet, GSM, Wifi ...)

private:
	int http_status;                   //Status courant de la connection
	int http_start_millis;                  //timestamp de démarrage de la requete en cours (sert pour le timeout)
	int http_timeout;                       //configuration de timeout en cours
	
};

#endif
