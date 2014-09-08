#ifndef HttpClient_h
#define HttpClient_h

#include "Arduino.h"
#include "Client.h"

class HttpClient {

public:
	HttpClient();

	//Les status que peut prendre la connection HTTP
	#define HTTP_STATUS_IDLE 0							//Aucune requete http en cours
	#define HTTP_STATUS_RESP_WAIT 1				 //En attente de la réponse
	#define HTTP_STATUS_RESP_BEGIN 2				//Au début de la réponse (avant HTTP/1.1)
	#define HTTP_STATUS_RESP_HEADER 3			 //Au début d'une ligne d'en-tête
	#define HTTP_STATUS_RESP_HEADER_VALUE 4 //Ligne d'en-tête, après le ':' (clé déjà lue)
	#define HTTP_STATUS_RESP_BODY 5				 //corps de la réponse
	#define HTTP_STATUS_ERROR 6						 //Une erreur est survenue
	#define HTTP_STATUS_EOF 7							 //La fin de fichier a été atteinte
	
	//Les codes retours possibles (négatifs pour les différencier du nombre de caractères que peuvent renvoyer certaines fonctions)
	#define HTTP_ERROR -1
	#define HTTP_RESP_END_OF_HEARDERS -2	//Fin des en-têtes
	#define HTTP_RESP_EOF -3							//Fin de réponse
	#define HTTP_ERROR_TIMEOUT -4;
	
	int http_resp_status;
	int http_start_millis;
	int http_timeout;
	
	#define HTTP_DEFAULT_TIMEOUT 3000;
	
	/**
	 * Lecture de la première ligne de la réponse, avec le code réponse
	 *	@return int code réponse, ou HTTP_RESP_ERROR s'il y a eu une erreur
	 *		 HTTP_ERROR s'il y a eu une erreur
	 *		 HTTP_RESP_EOF si la fin de fichier a été rencontrée
	 *		 HTTP_RESP_END_OF_HEARDERS s'il n'y a plus d'en-têtes
	 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
	 */
	int http_resp_read_code(Client &cl);
	
	/**
	 * Ligne d'entête HTTP : lecture de la clé uniquement
	 *	@return int nb de caractères lus
	 *		 HTTP_ERROR s'il y a eu une erreur
	 *		 HTTP_RESP_EOF si la fin de fichier a été rencontrée
	 *		 HTTP_RESP_END_OF_HEARDERS s'il n'y a plus d'en-têtes
	 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
	 */
	int http_resp_read_next_header_key(Client &cl, char* key,int maxlen);
	
	/**
	 * 
	 */
	int http_resp_read_next_header_value(Client &cl, char* value,int maxlen);
	
	/**
	 * Déplacer le curseur à la fin de la ligne en cours
	 */
	int httpRespMoveToEOL(Client &cl);
	
	/**
	 * Attend des données de réponse http
	 * @return 0 si tout va bien
	 *		 HTTP_RESP_EOF si la fin de fichier a été rencontrée
	 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
	 */
	int httpWaitForResponse(Client &cl);
	
	/**
	 * Renvoie le caractère suivant de la réponse, en attendant si nécessaire
	 * @return caractère suivant dans la réponse
	 *		 HTTP_RESP_EOF si la fin de fichier a été rencontrée
	 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
	 */
	int httpRead(Client &cl);

};

#endif
