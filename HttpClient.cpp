#include "Arduino.h"
#include "HttpClient.h"

HttpClient::HttpClient(Client& cl) : client(&cl){
	status = HTTP_STATUS_IDLE;
	timeout = HTTP_DEFAULT_TIMEOUT;
	debug = false;
}

void HttpClient::setTimeout(int millis){
	if(millis>0) timeout = millis;
}

void HttpClient::reset(){
	if(client->connected()){
		client->stop();
	}
	status = HTTP_STATUS_IDLE;
}

void HttpClient::setDebug(boolean d){
	debug = d;
}

/**
 * Envoi d'une requete GET
 * @return 0 si la connexion a réussi
 *    HTTP_ERROR_CONNECTION_FAILED si la conexion a échoué
 */
int HttpClient::sendGet(char* server, int port, char* path){
	if(status!=HTTP_STATUS_IDLE) return HTTP_ERROR;
	
	if( connect(server, port) ) return HTTP_ERROR_CONNECTION_FAILED;
	
	print("GET ");
	print(path);
    print(" HTTP/1.1\nHost: ");
	println(server);
	println("Connection: close\n");
}

int HttpClient::print(char* str){
	client->print(str);
	if(debug) Serial.print(str);
}
int HttpClient::println(char* str){
	client->println(str);
	if(debug) Serial.println(str);
}

/**
 * Connection au serveur
 */
boolean HttpClient::connect(char* serv, int port){
	if( !client->connect(serv, port) ){
		//KO
		client->stop();
		if( !client->connect(serv, port) ){
			Serial.println(F("HTTP connection failed. try to force client.stop()"));

			//On essaye simplement de forcer la fermeture de la socket
			client->stop();
			delay(500); //Don't know if necessary
			if( !client->connect(serv, port) ) {
			//Ca n'a pas marché
			return false;
			}
		}
	}
	return true;
}

/**
 * Attend le début de la réponse, lit la première ligne de la réponse, avec le code réponse
 * Fait suite à l'envoi d'une requête.
 *	@return int code réponse
 *		 HTTP_ERROR s'il y a eu une erreur
 *		 HTTP_EOF si la fin de fichier a été rencontrée
 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
 */
int HttpClient::readResponseCode(){
	if(status!=HTTP_STATUS_RESP_WAIT) return HTTP_ERROR;
	
	char* intro = "HTTP/1.1 ";
	int result = 0;
	startMillis = millis();
	
	for(int charIdx=0;;charIdx++){
		int c = httpRead();
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
			if(int res=moveToEOL()<0) return res;
			status = HTTP_STATUS_RESP_HEADER;	//La ligne suivante est une en-tête
			return result;
		}
	}
}

/**
 * Ligne d'entête HTTP : lecture de la clé uniquement
 *	@return int nb de caractères lus
 *       0 signifie qu'il n'y a plus d'en-tête : on passe au body
 *		 HTTP_ERROR s'il y a eu une erreur
 *		 HTTP_EOF si la fin de fichier a été rencontrée
 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
 */
int HttpClient::readNextHeaderKey(char* key,int maxlen){
	if(status!=HTTP_STATUS_RESP_HEADER){
		//On n'est pas en début de ligne d'en-tête
		return HTTP_ERROR;
	}
	
	for(int nbChars=0;;){
		int c = httpRead();
		if(c<0) return c;	//httpRead a renvoyé une erreur
		
		if(c=='\r') continue;	//on ignore le '\r'
		if(c=='\n' && nbChars==0){
			//C'est la fin des en-têtes
			status = HTTP_STATUS_RESP_BODY;
			return 0;
		}
		if(c==':'){
			//Fin de la clé
			status = HTTP_STATUS_RESP_HEADER_VALUE; //L'étape suivante est de lire la valeur
			return nbChars;
		}
		if(c>='A' && c<='Z' || c>='a' && c<='z' || c>='0' && c<='9' || c=='_' || c=='-'){
			nbChars++;
			if(nbChars>maxlen) return HTTP_ERROR_BUFFER_OVERFLOW;
			key[nbChars-1] = c;
			key[nbChars] = '\0';
		}
		else{
			//Caractère invalide
			return HTTP_ERROR;
		}
	}
}

/**
 * Ligne d'entête HTTP : lecture de la valeur
 * Doit faire suite à readNextHeaderKey
 *	@return int nb de caractères lus
 *		 HTTP_ERROR s'il y a eu une erreur
 *		 HTTP_EOF si la fin de fichier a été rencontrée
 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
 */
int HttpClient::readNextHeaderValue(char* value,int maxlen){
	if(status!=HTTP_STATUS_RESP_HEADER_VALUE){
		//Le curseur n'est pas au début d'une valeur d'en-tête
		return HTTP_ERROR;
	}
	value[0] = '\0';
	for(int nbChars=0;;){
		int c = httpRead();
		if(c<0) return c;	//httpRead a renvoyé une erreur
		
		if(c=='\r') continue;	//on ignore le '\r'
		if(c=='\n'){
			//C'est la fin de la valeur
			status = HTTP_STATUS_RESP_HEADER;
			return nbChars;
		}
		if(nbChars>=maxlen){
			//Longueur de chaine dépassée
			return HTTP_ERROR;
		}
		
		value[nbChars++] = c;
		value[nbChars+1] = '\0';
	}
}

/**
 * Passe la suites des en-têtes HTTP
 *	@return 0 si tout s'est bien passé
 *		 HTTP_ERROR s'il y a eu une erreur
 *		 HTTP_EOF si la fin de fichier a été rencontrée
 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
 */
int HttpClient::skipHeaders(){
	if(status!=HTTP_STATUS_RESP_HEADER && status!=HTTP_STATUS_RESP_HEADER_VALUE){
		//On n'est pas dans les en-têtes
		return HTTP_ERROR;
	}
	
	int prevChar;
	int c = 0;
	do{
		prevChar = c;
		int c = httpRead();
		if(c<0) return c;	//httpRead a renvoyé une erreur
		if(c=='\r') continue;
	}
	while(c!='\n' || c!=prevChar); //tant qu'il n'y a pas deux caractères '\n' consécutifs
	return 0;
}

int HttpClient::moveToEOL(){
	if(status==HTTP_STATUS_IDLE){
		//La réponse n'a pas commencé
		return HTTP_ERROR;
	}
	for(int nbChars=0;;nbChars++){
		int c = httpRead();
		if(c<0) return c;	//httpRead a renvoyé une erreur
		if(c=='\n'){
			if(status<=HTTP_STATUS_RESP_HEADER_VALUE) status = HTTP_STATUS_RESP_HEADER;
			//sinon on est dans le corps : on ne change pas le status
			return 0;
		}
	}
}

/**
 * Indique si des données de réponse sont arrivées
 */
int HttpClient::available(){
	return client->available();
}

/**
 * Renvoie le caractère suivant de la réponse, en attendant si nécessaire
 * @return caractère suivant dans la réponse
 *		 HTTP_EOF si la fin de fichier a été rencontrée
 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
 */
int HttpClient::httpRead(){
	//on attend tant qu'il n'y a rien à lire, qu'on n'est pas déconnecté, et que le timeout n'est pas dépassé
	while(!client->available()){
		if ( !client->connected() ){
			status = HTTP_STATUS_EOF;
			return HTTP_EOF;
		}
		if(millis()-startMillis > timeout){
			status = HTTP_STATUS_ERROR;
			return HTTP_ERROR_TIMEOUT;	 
		}
	}
	
	int c = client->read();
	if(c==EOF){
		status = HTTP_STATUS_EOF;
		return HTTP_EOF;
	}
	return c;
}

