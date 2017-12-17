#include <ELClient.h>
#include <ELClientRest.h>
#include <ELClientWebServer.h>


//Nawiązuje połączenie z esp-link za pośrednictwem portu szeregowego dla protokołu SLIP 
//i danych debugowaniw
ELClient esp(&Serial, &Serial);

// Uruchamianie klienta REST dla połączenia uC<->esp
ELClientRest rest(&esp);

//Uruchamia webserwer
ELClientWebServer webServer(&esp);

boolean wifiConnected = false;

// Callback od esp-linka, który pilnuje zmian stanu wifi 
// Wypisuje trochę debugu i ustawia globalną flagę
void wifiCb(void *response) {
  ELClientResponse *res = (ELClientResponse*)response;
  if (res->argc() == 1) {
    uint8_t status;
    res->popArg(&status, 1);

    if(status == STATION_GOT_IP) {
      Serial.println("WIFI CONNECTED");
      wifiConnected = true;
    } else {
      Serial.print("WIFI NOT READY: ");
      Serial.println(status);
      wifiConnected = false;
    }
  }
}


//Definiuje zachowanie uC po otrzymaniu informacji, że wciśnięto 
//przycisk
void ledButtonPressCb(char * btnId) 
{
  String id = btnId;
  if( id == F("btn_on") )
    digitalWrite(A2, LOW);
    Serial.println("Otwieram furtkę");
    delay(4000);
    digitalWrite(A2, HIGH);
}

void resetCb(void) {
  Serial.println("EL-Client (re-)starting!");
  bool ok = false;
  do {
    ok = esp.Sync();      // synchronizuje się z esp-linkiem, blokuje na ponad 2 sekundy
    if (!ok) Serial.println("EL-Client sync failed!");
  } while(!ok);
  Serial.println("EL-Client synced!");
  
  webServer.setup();
}


void setup() {
  Serial.begin(9600);   
  Serial.println("EL-Client starting!");

pinMode(A0, INPUT_PULLUP); //kontaktron otwarcia furtki
pinMode(A2, OUTPUT); //przekaznik zamykania/otwierania
digitalWrite(A2, HIGH);

  //Synchronizuje z esp-link. Jest wymagana na początku każdego skeczu. Inicjalizuje 
  //callback'i do callbacku o zmianie statusu wifi. Callback jest wywoływany ze stanem początkowym
  //tuz po tym jak Sync() zakończy działanie. 
  
  esp.wifiCb.attach(wifiCb); // callback zmian stanu wifi, opcjonalne (wywalić, jeśli niepotrzebne)
  bool ok;
  do {
    ok = esp.Sync();      // synchronizuje esp-link, blokuje system na 2 sek.
    if (!ok) Serial.println("EL-Client sync failed!");
  } while(!ok);
  Serial.println("EL-Client synced!");

URLHandler *ledHandler = webServer.createURLHandler(F("/Sterowanie.html.json"));
ledHandler->buttonCb.attach(&ledButtonPressCb);

esp.resetCb = resetCb;
resetCb();

  // Ustawia klienta REST'a, żeby gadał z 192.168.1.61 na porcie 8080. 
  //Nie łączy się z nim jeszcze, ale zapamiętuje dane po stronie esp-linka
  
  int err = rest.begin("192.168.1.55", 8080);
  if (err != 0) {
    Serial.print("REST begin failed: ");
    Serial.println(err);
    while(1) ;
  }
  Serial.println("EL-REST ready");
}

#define BUFLEN 266
boolean zamknieta = true;

void loop() {

// przetwarza wszystkie callbacki od esp-linka
esp.Process();


//skrypt dla kontaktronu, który z przystawionym magnesem jest rozwarty. 
//sprawdzam kontaktron i zmieniam stan przelacznika w Domoticzu
if (digitalRead(A0) == LOW && zamknieta == true){ //kontaktron zwarty
  if(wifiConnected) {
    // Wysyłanie żądania do domoticza
    rest.get("/json.htm?type=command&param=switchlight&idx=30&switchcmd=On");
    Serial.print("Wysłałem zapytanie json");
    char response[BUFLEN];
    zamknieta = false;
    memset(response, 0, BUFLEN);
    uint16_t code = rest.waitResponse(response, BUFLEN);
    if(code == HTTP_STATUS_OK){
      Serial.println("Odpowiedz na zapytanie json do Domoticza: ");
      Serial.println(response);
    } else {
      Serial.print("Nie wykonano zapytania GET: ");
      Serial.println(code);
    }
  }
}

//Jesli jest zamknieta
if (digitalRead(A0) == HIGH && zamknieta == false){
  if(wifiConnected) {
    // pobiera metodą GET odpowiedź na zapytanie json z wcześniej ustawionego serwera 
    rest.get("/json.htm?type=command&param=switchlight&idx=30&switchcmd=Off");
    Serial.print("Wysłałem zapytanie json");
    zamknieta = true;
    char response[BUFLEN];
    memset(response, 0, BUFLEN);
    uint16_t code = rest.waitResponse(response, BUFLEN);
    if(code == HTTP_STATUS_OK){
      Serial.println("Odpowiedz na zapytanie json do Domoticza: ");
      Serial.println(response);
    } else {
      Serial.print("Nie wykonano zapytania GET: ");
      Serial.println(code);
    }
  }
}
}


