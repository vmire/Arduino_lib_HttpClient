#ifndef HttpClient_h
#define HttpClient_h

#include "Arduino.h"
#include "Client.h"

class HttpClient{

public:
	HttpClient(Client& cl);
	
	//Les status que peut prendre la connection HTTP
	typedef enum {
		HTTP_STATUS_IDLE,			//Aucune requete http en cours
		HTTP_STATUS_RESP_WAIT,			//En attente de la réponse
		HTTP_STATUS_RESP_BEGIN,			//Au début de la réponse (avant HTTP/1.1)
		HTTP_STATUS_RESP_HEADER,		//Au début d'une ligne d'en-tête
		HTTP_STATUS_RESP_BODY,			//Corps de la réponse
		HTTP_STATUS_ERROR,			//Une erreur est survenue
		HTTP_STATUS_EOF				//La fin de fichier a été atteinte
	} HttpState;
	
	//Les codes retours possibles (négatifs pour les différencier du nombre de caractères que peuvent renvoyer certaines fonctions)
	#define HTTP_ERROR -2					
	#define HTTP_ERROR_BUFFER_OVERFLOW -3
	#define HTTP_ERROR_TIMEOUT -4
	#define HTTP_ERROR_CONNECTION_FAILED -5
	#define HTTP_ERROR_STATUS -6
	
	#define HTTP_DEFAULT_TIMEOUT 3000;		//Timeout http par défaut
	
	HttpState status;				//Status courant de la connection
	
	void reset();
	void close();
	void setTimeout(int millis);
	void setDebug(boolean debug);
	
	//int sendGet(char* server, int port, char* path);
	int sendGet(char* server, int port, char* path, char* params);
	
	int readResponseCode();
	int readLineToChar(char sep, char* key,int maxlen);
	int skipHeaders();
	int moveToEOL();
	int available();
	int waitForResponse();
	int httpRead();

protected:
	Client* client;					//Interface Client (ethernet, GSM, Wifi ...)

	int print(char* str);
	int println(char* str);
	boolean connect(char* serv, int port);
	int error(int err);
	
private:
	int startMillis;				//timestamp de démarrage de la requete en cours (sert pour le timeout)
	int timeout;					//configuration de timeout en cours
	boolean debug;
	boolean lastConnected;
	
};

#endif
