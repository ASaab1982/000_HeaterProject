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
DHT dht(DHTPIN, DHTTYPE);

// Define all variables here (no 'static') so the 'extern' in .h files can find them
const int outPorts[4] = {13, 12, 11, 10};
const uint8_t in1Pin = 8, in2Pin = 7, enablePin = 6, tempPin = A0, micPin = A1, touchPin = 3, servoPin = 5;
const int rotationSpeed = 256;
const bool dir =1; 
const int spd =256; 
const byte HEALTH_REQUIRED = 0x7F; // Binary 00000011 (Both tasks must be OK)
const uint32_t WDT_TIMEOUT = 15000; // 3 seconds




// delcaration of variable to be exanched with the server
Servo myservo;
WiFiClient client;
volatile int g_dcMotorSpeed = 0;
volatile int g_micAdc = 0;
volatile float g_thermistorTempC = 0.0f;
volatile float g_dhtTempC = 0.0f;
volatile float g_dhtHumidity = 0.0f;
volatile float g_stepperAngleDeg = 0.0f;
volatile int g_servoPositionDeg = 0;
volatile bool touched = false;
volatile byte systemHealth = 0x00;

// Your setup() and RTOS tasks remain here...

// -------------------- Globals --------------------



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
static TaskHandle_t hWebPost   = nullptr;
static TaskHandle_t hWatchdog  = nullptr;


// -------------------- Touch ISR --------------------
void handleTouchInterrupt() {
  vTaskNotifyGiveFromISR(hTouch, NULL);
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
D_PRINTLN(" Stepper moved");
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
  TickType_t xLastWakeTime = xTaskGetTickCount();
  // We want a 1-second gap between each task notification
  const TickType_t xOneSecond = pdMS_TO_TICKS(1000); 

  for (;;) {
    // --- Second 0 ---
    xTaskNotifyGive(hStepper);
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    // --- Second 1 ---
    xTaskNotifyGive(hDC);
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    // --- Second 2 ---
    xTaskNotifyGive(hServo);
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    // --- Second 3 ---
    xTaskNotifyGive(hTherm);
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    // --- Second 4 ---
    xTaskNotifyGive(hDHT);
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    // --- Second 5-9: Buffer / Web / Health ---
    xTaskNotifyGive(hWebPost); 
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);
    
    xTaskNotifyGive(hMonitor);
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    // Add empty delays if you want to reach a full 10 seconds
    vTaskDelayUntil(&xLastWakeTime, xOneSecond); 
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);
    vTaskDelayUntil(&xLastWakeTime, xOneSecond);

    D_PRINTLN(F("--- 10s Cycle Complete ---"));
  }
}

static void printRtosStats() {
    D_PRINTLN("---- Task Stack HWM (words) ----");
  if (hStepper) { D_PRINT(F("Stepper   : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hStepper)); }
  if (hDC)      { D_PRINT(F("DC        : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hDC)); }
  if (hServo)   { D_PRINT(F("Servo     : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hServo)); }
  if (hTherm)   { D_PRINT(F("Thermistor: ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hTherm)); }
  if (hDHT)     { D_PRINT(F("DHT       : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hDHT)); }
  if (hMic)     { D_PRINT(F("Mic       : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hMic)); }
  if (hTouch)   { D_PRINT(F("Touch     : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hTouch)); }
  if (hMonitor) { D_PRINT(F("Monitor   : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hMonitor)); }
  if (hMaster)  { D_PRINT(F("Master   : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hMaster)); }
  if (hWatchdog)  { D_PRINT(F("WatchDog   : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hWatchdog)); }
  if (hWebPost)  { D_PRINT(F("WebPost   : ")); D_PRINTLN(uxTaskGetStackHighWaterMark(hWebPost)); }
  

  D_PRINTLN("---- RTOS Stats ----");
  D_PRINT(F("FreeHeap=")); D_PRINTLN(xPortGetFreeHeapSize());
  D_PRINT(F("MinEver =" )); D_PRINTLN(xPortGetMinimumEverFreeHeapSize());
}

static void watchdogMonitorTask(void *pv) {
    for (;;) {
        // Logic: Only refresh if the mask matches our requirements
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        D_PRINT(F("System Health   : ")); 
        D_PRINTLN(systemHealth);
        if (systemHealth == HEALTH_REQUIRED) {
            WDT.refresh();            // Physical Hardware Kick
            systemHealth = 0x00;      // Reset for next cycle
            
            #if DEBUG
                Serial.println(F("WDT: All tasks reported. System Healthy."));
            #endif
        } else {
            #if DEBUG
                Serial.print(F("WDT Warning: Missing task report. Health Mask: "));
                Serial.println(systemHealth, BIN);
            #endif
        }
    }
}

static void TaskStepper(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doStepperSequence();
    D_PRINT(millis()); // Show exactly when this happened
    D_PRINTLN(F("Stepper moved"));
  }
}

static void TaskDC(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    driveDCMotor(dir, spd);
    D_PRINT(millis()); // Show exactly when this happened
    D_PRINTLN(F("DC moved"));
  }
}

static void TaskServo(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doServoSequence();
    D_PRINT(millis()); // Show exactly when this happened
    D_PRINTLN(F("Servo moved"));
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

static void TaskWebPost(void *pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doTaskWebPost();
  }
}

static void TaskMonitor(void *pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    printRtosStats();
  }
}


// -------------------- Arduino setup/loop --------------------
void setup() {
    Serial.begin(115200);
  /*
    if (!WDT.begin(5592)) {
    D_PRINTLN(F("WDT.begin a échoué. Tentative de refresh forcé..."));
    WDT.refresh(); 
    D_PRINTLN(F("Si vous lisez ceci, le code continue malgré l'erreur."));
      }
    else {
    // SCÉNARIO B : SUCCÈS
    D_PRINTLN(F("[OK] Watchdog Hardware initialized (15s timeout)."));
  }
  */
  

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
  D_PRINT(F("Connecting to "));
  D_PRINTLN(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    D_PRINT(F("."));
  }

  D_PRINTLN(F("\nWiFi Connected! Waiting for DHCP address..."));
  while (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    delay(10);
    D_PRINT(F("."));
  }

  D_PRINTLN(F(""));
  D_PRINTLN(F("WiFi connected."));
  D_PRINTLN(F("IP address: "));
  D_PRINTLN(WiFi.localIP());
  // server.begin();

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
  ok = xTaskCreate(TaskStepper,   "TaskStepper",   90, nullptr, 1, &hStepper);
  if (ok != pdPASS) { D_PRINTLN(F("TaskStepper create failed")); for(;;){} }

  ok = xTaskCreate(TaskDC,        "TaskDC",        90, nullptr, 1, &hDC);
  if (ok != pdPASS) { D_PRINTLN(F("TaskDC create failed")); for(;;){} }


  ok =  xTaskCreate(TaskServo,     "TaskServo",     90, nullptr, 1, &hServo);
  if (ok != pdPASS) { D_PRINTLN(F("TaskServo create failed")); for(;;){} }


  ok = xTaskCreate(TaskThermistor,"TaskTherm",    90, nullptr, 1, &hTherm);
  if (ok != pdPASS) { D_PRINTLN(F("TaskTherm create failed")); for(;;){} }


  ok = xTaskCreate(TaskDHT,       "TaskDHT",       180, nullptr, 1, &hDHT);
  if (ok != pdPASS) { D_PRINTLN(F("TaskDHT create failed")); for(;;){} }


  ok = xTaskCreate(TaskMic,       "TaskMic",       70, nullptr, 1, &hMic);
  if (ok != pdPASS) { D_PRINTLN(F("TaskMic create failed")); for(;;){} }


  ok = xTaskCreate(TaskTouch,     "TaskTouch",    50, nullptr, 1, &hTouch);
  if (ok != pdPASS) { D_PRINTLN(F("TaskTouch create failed")); for(;;){} }

  ok =   xTaskCreate(TaskMonitor, "Monitor", 70, nullptr, 1, &hMonitor);
  if (ok != pdPASS) { D_PRINTLN(F("Monitor create failed")); for(;;){} }

  ok = xTaskCreate(TaskMasterTimer, "Master", 60, nullptr, 3, &hMaster);
  if (ok != pdPASS) { D_PRINTLN(F("Task Master Time create failed")); for(;;){} }

   ok =xTaskCreate(TaskWebPost, "TaskWeb", 300, nullptr, 1, &hWebPost);
  if (ok != pdPASS) { D_PRINTLN(F("Task web post create failed")); for(;;){} }

  ok = xTaskCreate(watchdogMonitorTask, "WDT_Mon", 50, nullptr, 1, &hWatchdog);
  if (ok != pdPASS) { D_PRINTLN(F("Watchdog task create failed"));  for(;;){} }

  D_PRINT(F("Free heap bytes: "));
  D_PRINTLN(xPortGetFreeHeapSize());

  vTaskStartScheduler();
}

void loop() {
  // Not used once RTOS scheduler is running
}