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
	close();
}

void HttpClient::close(){
	if(debug) Serial.println(F("HttpClient.close()"));
	//if(client->connected()){
		client->stop();
	//}
	status = HTTP_STATUS_IDLE;
}

void HttpClient::setDebug(boolean d){
	debug = d;
	Serial.print("debug=");Serial.println(debug);
}

/**
 * Envoi d'une requete GET
 * @return 0 si la connexion a réussi
 *	HTTP_ERROR_CONNECTION_FAILED si la conexion a échoué
 */
int HttpClient::sendGet(char* server, int port, char* path, char* params){
	if(debug) Serial.print(F("HttpClient.sendGet "));
	
	if( !connect(server, port) ){
		return error(HTTP_ERROR_CONNECTION_FAILED);
	}
	print("GET ");
	print(path);
	if(debug) Serial.print(path);
	if(params[0]!='\0'){
		print("?");
		print(params);
		if(debug){
			Serial.print("?");
			Serial.print(params);
		}
	}
	print(" HTTP/1.1\nHost: ");
	println(server);
	println("Connection: close\n");
	if(debug) Serial.print("\n");
	status = HTTP_STATUS_RESP_WAIT;
	return 0;
}

int HttpClient::print(char* str){
	//if(debug) Serial.print(str);
	client->print(str);
}
int HttpClient::println(char* str){
	//if(debug) Serial.println(str);
	client->println(str);
}

/**
 * Connection au serveur
 */
boolean HttpClient::connect(char* serv, int port){
	if(debug){
		Serial.print(F("HttpClient.connect to "));
		Serial.print(serv);
		Serial.print("...");
	}
	if(status != HTTP_STATUS_IDLE){
		Serial.println(F("Connection deja en cours..."));
		return false;
	}

	if( !client->connect(serv, port) ){
		Serial.print(F("FAILED..."));
		//KO
		client->stop();
		delay(500); //Don't know if necessary
		if( !client->connect(serv, port) ){
			Serial.println(F("FAILED"));
			return false;
		}
	}
	if(debug) Serial.println("OK");
	lastConnected = true;
	return true;
}

/**
 * Attend le début de la réponse, lit la première ligne de la réponse, avec le code réponse
 * Fait suite à l'envoi d'une requête.
 *	@return int code réponse
 *		 HTTP_ERROR s'il y a eu une erreur
 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
 */
int HttpClient::readResponseCode(){
	if(debug) Serial.print(F("HttpClient.readResponseCode..."));
	if(status!=HTTP_STATUS_RESP_WAIT){
		if(debug) Serial.println(F("Excepted status:WAIT"));
		return error(HTTP_ERROR_STATUS);
	}
	char* intro = "HTTP/1.1 ";
	int result = 0;
	startMillis = millis();
	
	for(int charIdx=0;;charIdx++){
		char c = httpRead();

		if(c<0) return c;	//C'est une erreur
		
		if(charIdx<=8){
			//Vérification du "HTTP/1.1 "
			if(c!=intro[charIdx]){
				return error(HTTP_ERROR);
			}
		}
		else if(charIdx<=11){
			//Lecture du code réponse
			if(c<'0' || c >'9'){
				return error(HTTP_ERROR);
			}
			int tmp = 1;
			for(uint8_t i=0;i<11-charIdx;i++) tmp = tmp*10;
			tmp = (c-48)*tmp;
			result += tmp;
			//Serial.print("c=");Serial.print(c-48,DEC);Serial.print(" ");Serial.print(tmp);Serial.print("   idx=");Serial.print(charIdx,DEC);Serial.print("  ->");Serial.println(result,DEC);
			//if(debug) Serial.println(result);
		}
		else{
			//On passe le reste de la ligne, et on renvoie le résultat
			if(int res=moveToEOL()<0) return error(res);
			status = HTTP_STATUS_RESP_HEADER;	//La ligne suivante est une en-tête
			if(debug) Serial.println(result);
			return result;
		}
	}
}

/**
 * Lecture de la réponse jusqu'à un certain caractère (exclu) ou la fin de ligne
 * Le caractère '\r' est ignoré
 *	@return int nb de caractères lus (0 signifie que le premier caractère était ce caractère ou une fin de ligne)
 *		 HTTP_ERROR s'il y a eu une erreur
 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
 */
int HttpClient::readLineToChar(char sep, char* key,int maxlen){
	if(debug) Serial.println(F("HttpClient.readLineToChar"));
	for(int nbChars=0;;){
		char c = httpRead();

		if(c==sep || c==0) return nbChars;	//Séparateur ou fin de fichier
		if(c<0) return c;	//httpRead a renvoyé une erreur
		if(c==10) continue;	//on ignore le '\r'
		if(c==13 && nbChars==0 && status!=HTTP_STATUS_RESP_BODY){
			//C'est la fin des en-têtes
			status = HTTP_STATUS_RESP_BODY;
			return 0;
		}
		if(status==HTTP_STATUS_RESP_HEADER && !(c>='A' && c<='Z' || c>='a' && c<='z' || c>='0' && c<='9' || c=='_' || c=='-')){
			//Caractère invalide
			if(debug){
				Serial.print(F("Caractere invalide:")); Serial.print(c); Serial.println(c,DEC);
			}
			return error(HTTP_ERROR);
		}

		nbChars++;
		if(nbChars>maxlen) return error(HTTP_ERROR_BUFFER_OVERFLOW);
		key[nbChars-1] = c;
		key[nbChars] = '\0';
	}
}

/**
 * Passe la suites des en-têtes HTTP
 *	@return 0 si tout s'est bien passé
 *		 HTTP_ERROR s'il y a eu une erreur
 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
 */
int HttpClient::skipHeaders(){
	if(debug) Serial.println(F("HttpClient.skipHeaders"));
	if(status!=HTTP_STATUS_RESP_HEADER){
		//On n'est pas dans les en-têtes
		if(debug) Serial.println("Excepted status:RESP_HEADER");
		return error(HTTP_ERROR_STATUS);
	}
	
	char prevChar;
	char c = 0;
	while(c!=13 || prevChar!=13) //tant qu'il n'y a pas deux caractères '\n' consécutifs
	{
		prevChar = c;
		c = httpRead();
		if(c<0) return error(c);	//httpRead a renvoyé une erreur
		if(c==10){
			c = prevChar;	
			prevChar = 0;
			continue;
		}
	}
	status = HTTP_STATUS_RESP_BODY;
	return 0;
}

int HttpClient::moveToEOL(){
	if(debug) Serial.println(F("HttpClient.moveToEOL"));
	if(status==HTTP_STATUS_IDLE){
		//La réponse n'a pas commencé
		if(debug) Serial.println("Error: Status not IDLE");
		return error(HTTP_ERROR_STATUS);
	}
	for(int nbChars=0;;nbChars++){
		int c = httpRead();
		if(c<0) return c;	//httpRead a renvoyé une erreur
		if(c=='\n'){
			return 0;
		}
	}
}

/**
 * Indique si des données de réponse sont arrivées
 */
int HttpClient::available(){
	return status!= HTTP_STATUS_ERROR && client->available();
}

/**
 * Renvoie le caractère suivant de la réponse, en attendant si nécessaire
 * @return caractère suivant dans la réponse
 *		 O si la fin de fichier a été rencontrée
 *		 HTTP_ERROR_TIMEOUT si le timeout a été dépassé
 */
int HttpClient::httpRead(){
	//on attend tant qu'il n'y a rien à lire, qu'on n'est pas déconnecté, et que le timeout n'est pas dépassé
	while(!client->available()){
		if ( !client->connected() ){
			status = HTTP_STATUS_EOF;
			return 0;
		}
		if(millis()-startMillis > timeout){
			status = HTTP_STATUS_ERROR;
			return error(HTTP_ERROR_TIMEOUT);
		}
	}
	
	char c = client->read();
	if(c==EOF){
		status = HTTP_STATUS_EOF;
		return 0;
	}
	return c;
}

/**
 * Ecrit un message d'erreur et renvoi le code en retour
 */
int HttpClient::error(int err){
	Serial.print(F("HttpClient ERROR: "));
	switch(err){
		case HTTP_ERROR_BUFFER_OVERFLOW:
			Serial.print(F("Buffer overflow"));
			break;
		case HTTP_ERROR_TIMEOUT:
			Serial.print(F("timeout"));
			break;
		case HTTP_ERROR_CONNECTION_FAILED:
			Serial.print(F("Connection failed"));
			break;
		case HTTP_ERROR_STATUS:
			Serial.print(F("Invalid status: "));
			Serial.print(status);
			switch(status){
				case HTTP_STATUS_IDLE:
					Serial.print(F("IDLE"));
					break;
				case HTTP_STATUS_RESP_WAIT:
					Serial.print(F("RESP_WAIT"));
					break;
				case HTTP_STATUS_RESP_BEGIN:
					Serial.print(F("RESP_BEGIN"));
					break;
				case HTTP_STATUS_RESP_HEADER:
					Serial.print(F("RESP_HEADER"));
					break;
				case HTTP_STATUS_RESP_BODY:
					Serial.print(F("RESP_BODY"));
					break;
				case HTTP_STATUS_ERROR:
					Serial.print(F("ERROR"));
					break;
				case HTTP_STATUS_EOF:
					Serial.print(F("EOF"));
					break;
				default:
					Serial.print(status);
			}
			break;
	}
	Serial.print('\n');
	return err;
}
