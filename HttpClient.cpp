#include "Arduino.h"
#include "HttpClient.h"

HttpClient::HttpClient(){
	http_resp_status = HTTP_STATUS_IDLE;
	http_timeout = HTTP_DEFAULT_TIMEOUT;
}

/**
 * Lecture de la première ligne de la réponse, avec le code réponse
 *	@return int code réponse, ou HTTP_RESP_ERROR s'il y a eu une erreur
 *		 HTTP_ERROR s'il y a eu une erreur
 *		 HTTP_RESP_EOF si la fin de fichier a été rencontrée
 *		 HTTP_RESP_END_OF_HEARDERS s'il n'y a plus d'en-têtes
 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
 */
int HttpClient::http_resp_read_code(Client &cl){
	if(http_resp_status>HTTP_STATUS_RESP_BEGIN) return HTTP_ERROR;
	char* intro = "HTTP/1.1 ";
	int result = 0;
	http_start_millis = millis();
	
	for(int charIdx=0;;charIdx++){
		int c = httpRead(cl);
		if(c<0) return c;	//C'est une erreur
		
		if(charIdx<=8){
			//Vérification du "HTTP/1.1 "
			if(c!=intro[charIdx]){
				return HTTP_ERROR;
			}
		}
		else if(charIdx<=11){
			//Lecture du code réponse
			if(c<'0' || c >'9'){
				return HTTP_ERROR;
			}
			result += (c-'0')*10^(11-charIdx);
		}
		else{
			//On passe le reste de la ligne, et on renvoie le résultat
			if(int res=httpRespMoveToEOL(cl)<0) return res;
			http_resp_status = HTTP_STATUS_RESP_HEADER;	//La ligne suivante est une en-tête
			return result;
		}
	}
}

/**
 * Ligne d'entête HTTP : lecture de la clé uniquement
 *	@return int nb de caractères lus
 *		 HTTP_ERROR s'il y a eu une erreur
 *		 HTTP_RESP_EOF si la fin de fichier a été rencontrée
 *		 HTTP_RESP_END_OF_HEARDERS s'il n'y a plus d'en-têtes
 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
 */
int HttpClient::http_resp_read_next_header_key(Client &cl, char* key,int maxlen){
	if(http_resp_status!=HTTP_STATUS_RESP_HEADER){
		//On n'est pas en début de ligne d'en-tête
		return HTTP_ERROR;
	}
	
	for(int nbChars=0;;){
		int c = httpRead(cl);
		if(c<0) return c;	//C'est une erreur
		
		if(c=='\n' && nbChars==0){
			//C'est la fin des en-têtes
			http_resp_status = HTTP_STATUS_RESP_BODY;
			return HTTP_RESP_END_OF_HEARDERS;
		}
		if(c==':'){
			//Fin de la clé
			key[nbChars] = '\0';
			http_resp_status = HTTP_STATUS_RESP_HEADER_VALUE; //L'étape suivante est de lire la valeur
			return nbChars;
		}
		if(c>='A' && c<='Z' || c>='a' && c<='z' || c>='0' && c<='9' || c=='_' || c=='-'){
			nbChars++;
			if(nbChars>maxlen) return HTTP_ERROR;
			key[nbChars-1] = c;
		}
		else{
			//Caractère invalide
			return HTTP_ERROR;
		}
	}
}

int HttpClient::http_resp_read_next_header_value(Client &cl, char* value,int maxlen){
	
}

int HttpClient::httpRespMoveToEOL(Client &cl){
	if(http_resp_status<HTTP_STATUS_RESP_BEGIN){
		//La réponse n'a pas commencé
		return HTTP_ERROR;
	}
	for(int nbChars=0;;nbChars++){
		int c = httpRead(cl);
		if(c<0) return c;	//C'est une erreur
		if(c=='\n'){
			if(http_resp_status<=HTTP_STATUS_RESP_HEADER_VALUE) http_resp_status = HTTP_STATUS_RESP_HEADER;
			//sinon on est dans le corps : on ne change pas le status
			return 0;
		}
	}
}

/**
 * Attend des données de réponse http
 * @return 0 si tout va bien
 *		 HTTP_RESP_EOF si la fin de fichier a été rencontrée
 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
 */
int HttpClient::httpWaitForResponse(Client &cl){
	while(!cl.available()){
		//on attend tant qu'il n'y a rien à lire, qu'on n'est pas déconnecté, et que le timeout n'est pas dépassé
		if (cl.connected() == 0){
			http_resp_status = HTTP_STATUS_EOF;
			return HTTP_RESP_EOF;
		}
		if(millis()-http_start_millis > http_timeout){
			http_resp_status = HTTP_STATUS_ERROR;
			return HTTP_ERROR_TIMEOUT;	 
		}
	}
	return 0;
}

/**
 * Renvoie le caractère suivant de la réponse, en attendant si nécessaire
 * @return caractère suivant dans la réponse
 *		 HTTP_RESP_EOF si la fin de fichier a été rencontrée
 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
 */
int HttpClient::httpRead(Client &cl){
	if(int res=httpWaitForResponse(cl) != 0) return res;
	
	//On ignore le caractère '\r'
	int c;
	do{
		c = cl.read();
	} while(c=='\r');
	if(c==EOF){
		http_resp_status = HTTP_STATUS_EOF;
		return HTTP_RESP_EOF;
	}
	return c;
}

