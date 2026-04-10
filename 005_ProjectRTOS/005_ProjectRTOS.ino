// #define INCLUDE_uxTaskGetStackHighWaterMark (1) found in FreeRTOSConfig.h

#include "ProjectHeater.h"


// -------------------- Pins / constants (same as your sketch) --------------------
/*
**********************************
const (Le Garde-fou)
C'est l'abréviation de "constant".
Son rôle : Il indique au compilateur que la valeur de in1Pin ne changera jamais pendant l'exécution du programme.
Pourquoi l'utiliser ? Si par erreur tu essaies d'écrire in1Pin = 10; plus loin dans ton code,
l'ordinateur affichera une erreur au lieu de laisser le bug se produire. C'est une sécurité.
**********************************
 static (Le Résident Permanent)
Le sens de static varie selon l'endroit où il est placé, mais dans le contexte d'une classe ou d'un fichier :
Son rôle : Il signifie que cette variable appartient à la "classe" ou au fichier lui-même, et non à une "instance" (un objet) spécifique.
En pratique : La variable est créée une seule fois en mémoire au démarrage et y reste jusqu'à l'extinction. 
Cela évite de recréer la variable à chaque fois qu'une fonction est appelée, ce qui économise de la précieuse mémoire vive (RAM) sur ton Arduino.
**********************************
*/
#define DHTPIN 2
#define DHTTYPE DHT11

// Define all variables here (no 'static') so the 'extern' in .h files can find them
const int outPorts[4] = {13, 12, 11, 10};
const uint8_t in1Pin = 8, in2Pin = 7, enablePin = 6;
const uint8_t tempPin = A0;
const int rotationSpeed = 256;
const uint8_t micPin = A1;
const uint8_t touchPin = 3;
const bool dir =1; 
const int spd =256; 
DHT dht(2, DHT11);

// Remove 'static' so ServoAction.cpp can see these!
const uint8_t servoPin = 5;
Servo myservo;


volatile int g_dcMotorSpeed = 0;
volatile int g_micAdc = 0;
volatile float g_thermistorTempC = 0.0f;
volatile float g_dhtTempC = 0.0f;
volatile float g_dhtHumidity = 0.0f;
volatile float g_stepperAngleDeg = 0.0f;
volatile int g_servoPositionDeg = 0;
volatile bool touched = false;

// Your setup() and RTOS tasks remain here...

// -------------------- Globals --------------------


static WiFiServer server(80);
/*
**********************************
1. TaskHandle_t (Le Type)
C'est un identifiant unique (un pointeur) vers une tâche spécifique. 
Imagine que ton Arduino exécute plusieurs programmes en même temps (un pour le moteur Stepper, un pour le micro, un pour le tactile). 
Le TaskHandle_t est l'adresse qui permet de dire au système : "Hé, je parle de la tâche qui gère le moteur !".
**********************************

2. static (La Portée)
Comme nous l'avons vu précédemment, static ici garantit que ces variables :
Gardent leur valeur pendant toute la durée de vie du programme.
Sont confinées au fichier actuel (elles ne sont pas visibles depuis d'autres fichiers .cpp de ton projet, ce qui évite les conflits de noms).
**********************************

3. nullptr (L'état initial)
Cela signifie "pointe vers rien".
Au démarrage, les tâches n'ont pas encore été créées. On initialise les "télécommandes" à vide pour éviter 
que le programme ne tente de manipuler une tâche fantôme, ce qui ferait planter l'Arduino.
*/

// Task handles for chaining
static TaskHandle_t hStepper   = nullptr;
static TaskHandle_t hDC        = nullptr;
static TaskHandle_t hServo     = nullptr;
static TaskHandle_t hTherm     = nullptr;
static TaskHandle_t hDHT       = nullptr;
static TaskHandle_t hMic       = nullptr;
static TaskHandle_t hTouch     = nullptr;
static TaskHandle_t hMonitor   = nullptr;
static TaskHandle_t hMaster    = nullptr;


// -------------------- Touch ISR --------------------
void handleTouchInterrupt() {
  vTaskNotifyGiveFromISR(hTouch, NULL);
}


static void printRtosStats() {
    Serial.println("---- Task Stack HWM (words) ----");
  if (hStepper) { Serial.print(F("Stepper   : ")); Serial.println(uxTaskGetStackHighWaterMark(hStepper)); }
  if (hDC)      { Serial.print(F("DC        : ")); Serial.println(uxTaskGetStackHighWaterMark(hDC)); }
  if (hServo)   { Serial.print(F("Servo     : ")); Serial.println(uxTaskGetStackHighWaterMark(hServo)); }
  if (hTherm)   { Serial.print(F("Thermistor: ")); Serial.println(uxTaskGetStackHighWaterMark(hTherm)); }
  if (hDHT)     { Serial.print(F("DHT       : ")); Serial.println(uxTaskGetStackHighWaterMark(hDHT)); }
  if (hMic)     { Serial.print(F("Mic       : ")); Serial.println(uxTaskGetStackHighWaterMark(hMic)); }
  if (hTouch)   { Serial.print(F("Touch     : ")); Serial.println(uxTaskGetStackHighWaterMark(hTouch)); }
  if (hMonitor) { Serial.print(F("Monitor   : ")); Serial.println(uxTaskGetStackHighWaterMark(hMonitor)); }
  if (hMaster) { Serial.print(F("Master   : ")); Serial.println(uxTaskGetStackHighWaterMark(hMaster)); }

  Serial.println("---- RTOS Stats ----");
  Serial.print(F("FreeHeap=")); Serial.println(xPortGetFreeHeapSize());
  Serial.print(F("MinEver =" )); Serial.println(xPortGetMinimumEverFreeHeapSize());
}


// -------------------- Tasks (one per function) --------------------
/*
1. La signature de la fonction
static void TaskStepper(void* pv)
static : Limite la visibilité de cette fonction à ce fichier unique (bonne pratique pour éviter les conflits).
void* pv : C'est le paramètre standard imposé par FreeRTOS pour toutes les tâches. Même si tu ne l'utilises pas, il doit être présent.

2. Ignorer le paramètre inutilisé
(void)pv;
Cette ligne ne fait techniquement "rien" à l'exécution. Elle sert juste à dire au compilateur : 
"Je sais que j'ai un paramètre pv et que je ne l'utilise pas, ne m'envoie pas d'avertissement (warning)."

3. La boucle infinie
for (;;)
Une tâche FreeRTOS ne doit jamais s'arrêter ou arriver à la fin de sa fonction (sinon l'Arduino crash). 
On crée donc une boucle qui tournera pour toujours.

4. L'attente du signal (Le verrou)
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
C'est la ligne la plus importante. La tâche s'arrête ici et "s'endort". Elle ne consomme plus aucune ressource processeur.
pdTRUE : Une fois réveillée, elle remet son compteur de notifications à zéro.
portMAX_DELAY : Elle attendra indéfiniment qu'une autre tâche lui envoie un signal (un "Top départ").

5. L'action
doStepperSequence();
Une fois que la tâche a reçu sa notification (le réveil), elle exécute enfin le mouvement du moteur.

6. Le feedback
Serial.println(" Stepper moved");
Affiche un message dans le moniteur série pour te confirmer que la séquence est terminée.

7. Le passage de relais (Le chaînage)
xTaskNotifyGive(hDC);
La tâche Stepper a fini son travail, elle envoie maintenant une notification à la tâche suivante : hDC (le moteur à courant continu).
Cela va "réveiller" la tâche TaskDC qui était probablement endormie sur son propre ulTaskNotifyTake.

Résumé du fonctionnement (Le "Chaining")
Ce code fait partie d'une chaîne de tâches. Le Stepper attend un signal, fait son travail, 
puis donne le signal au moteur DC. C'est une manière très propre de créer une séquence d'événements automatisée sans utiliser de delay() 
qui bloquerait tout ton programme.

*/

static void TaskMasterTimer(void* pv) {
  (void)pv;
  int currentSecond = 0;

  for (;;) {
    // 1. Check which task to trigger based on the second
    switch (currentSecond) {
      case 0: xTaskNotifyGive(hStepper); break;
      case 1: xTaskNotifyGive(hDC);      break;
      case 2: xTaskNotifyGive(hServo);   break;
      case 3: xTaskNotifyGive(hTherm);   break;
      case 4: xTaskNotifyGive(hDHT);     break;
      case 5: xTaskNotifyGive(hMic);     break;
    }

    // 2. Increment time
    currentSecond++;
    if (currentSecond >= 10) {
      currentSecond = 0; // Reset every 10 seconds
      Serial.println(F("--- New 10s Cycle Started ---"));
    }

    // 3. Wait exactly 1 second
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}


static void TaskStepper(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doStepperSequence();
    Serial.println(F("Stepper moved"));
  }
}

static void TaskDC(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    driveDCMotor(dir, spd);
    Serial.println(F("DC moved"));
  }
}

static void TaskServo(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doServoSequence();
  }
}

static void TaskThermistor(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doThermistorRead();  }
}

static void TaskDHT(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doDHTRead();
  }
}

static void TaskMic(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doMicRead();
  }
}

static void TaskTouch(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doTouchDisplay();
  }
} 

static void TaskMonitor(void *pv) {
  (void)pv;
  for (;;) {
    printRtosStats();
    vTaskDelay(pdMS_TO_TICKS(5000)); // every 5s
  }
}



// -------------------- Arduino setup/loop --------------------
void setup() {
  Serial.begin(115200);

  dht.begin();

  for (int i = 0; i < 4; i++) {
    pinMode(outPorts[i], OUTPUT);
  }

  pinMode(in1Pin, OUTPUT);
  pinMode(in2Pin, OUTPUT);
  pinMode(enablePin, OUTPUT);

  pinMode(tempPin, INPUT);
  pinMode(micPin, INPUT);

  pinMode(touchPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(touchPin), handleTouchInterrupt, RISING);

  myservo.attach(servoPin);

  // WiFi (kept from your setup)
  Serial.print(F("Connecting to "));
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }

  Serial.println(F("\nWiFi Connected! Waiting for DHCP address..."));
  while (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    delay(10);
    Serial.print(F("."));
  }

  Serial.println(F(""));
  Serial.println(F("WiFi connected."));
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());
  server.begin();

  randomSeed(millis());
  /*
  Chaque ligne suit exactement la même structure. Prenons l'exemple du micro :
  xTaskCreate(TaskMic, "TaskMic", 256, nullptr, 1, &hMic);

  Voici le décryptage des 6 arguments (les paramètres entre parenthèses) :

  1. La Fonction (TaskMic)
  C'est le nom de la fonction C++ que tu as écrite ailleurs dans ton code. 
  C'est le "cerveau" de la tâche. Elle contient généralement une boucle while(1) ou for(;;) pour ne jamais s'arrêter.

  2. Le Nom ("TaskMic")
  C'est une simple chaîne de caractères. Elle ne sert qu'au débogage pour que tu puisses identifier la tâche si tu utilises un moniteur système.
  L'Arduino ne l'utilise pas pour exécuter le code.

  3. La Taille de la Pile / Stack (256)
  C'est la quantité de mémoire vive (RAM) allouée exclusivement à cette tâche.

  Attention : Si tu mets trop peu (ex: 64), la tâche va "déborder" (Stack Overflow) et l'Arduino va crasher.

  Note : Dans ton code, TaskDHT a 768, car la bibliothèque DHT est souvent gourmande en mémoire pour traiter les signaux.

  4. Le Paramètre (nullptr)
  Ici, on passe nullptr (rien). Cela permettrait de passer des données à la tâche au moment de sa création
  (par exemple, un numéro de pin spécifique). Si tu n'en as pas besoin, on laisse à vide.

  5. La Priorité (1 ou 2)
  C'est le niveau d'importance de la tâche. Plus le chiffre est élevé, plus la tâche est prioritaire.

  Priorité 2 (Stepper, DC, Touch) : Ces tâches sont jugées critiques. Le moteur doit bouger précisément et le tactile doit être instantané.

  Priorité 1 (Mic, DHT) : Ces tâches sont moins urgentes. Si le processeur est très occupé par le moteur, 
  il peut attendre quelques millisecondes avant de lire la température.

  6. Le Handle (&hMic)
  C'est l'adresse de la "télécommande" (TaskHandle_t) dont nous parlions tout à l'heure. 
  xTaskCreate va remplir ta variable hMic avec l'identifiant de la tâche une fois qu'elle est créée. 
  C'est grâce à cela que tu pourras la contrôler plus tard.
*/
BaseType_t ok;

  // Create tasks
  ok = xTaskCreate(TaskStepper,   "TaskStepper",   60, nullptr, 2, &hStepper);
  if (ok != pdPASS) { Serial.println(F("TaskStepper create failed")); for(;;){} }

  ok = xTaskCreate(TaskDC,        "TaskDC",        80, nullptr, 2, &hDC);
  if (ok != pdPASS) { Serial.println(F("TaskDC create failed")); for(;;){} }


  ok =  xTaskCreate(TaskServo,     "TaskServo",     80, nullptr, 2, &hServo);
  if (ok != pdPASS) { Serial.println(F("TaskServo create failed")); for(;;){} }


  ok = xTaskCreate(TaskThermistor,"TaskTherm",    100, nullptr, 2, &hTherm);
  if (ok != pdPASS) { Serial.println(F("TaskTherm create failed")); for(;;){} }


  ok = xTaskCreate(TaskDHT,       "TaskDHT",       200, nullptr, 1, &hDHT);
  if (ok != pdPASS) { Serial.println(F("TaskDHT create failed")); for(;;){} }


  ok = xTaskCreate(TaskMic,       "TaskMic",       80, nullptr, 1, &hMic);
  if (ok != pdPASS) { Serial.println(F("TaskMic create failed")); for(;;){} }


  ok = xTaskCreate(TaskTouch,     "TaskTouch",    60, nullptr, 2, &hTouch);
  if (ok != pdPASS) { Serial.println(F("TaskTouch create failed")); for(;;){} }

  ok =   xTaskCreate(TaskMonitor, "Monitor", 60, nullptr, 1, &hMonitor);
  if (ok != pdPASS) { Serial.println(F("Monitor create failed")); for(;;){} }

  ok = xTaskCreate(TaskMasterTimer, "Master", 100, nullptr, 3, &hMaster);
  if (ok != pdPASS) { Serial.println(F("Task Master Time create failed")); for(;;){} }

  Serial.print("Free heap bytes: ");
  Serial.println(xPortGetFreeHeapSize());

  vTaskStartScheduler();
}

void loop() {
  // Not used once RTOS scheduler is running
}