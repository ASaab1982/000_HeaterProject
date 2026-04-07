// #define INCLUDE_uxTaskGetStackHighWaterMark (1) found in FreeRTOSConfig.h

#include <Arduino_FreeRTOS.h>
#include <Servo.h>
#include <WiFiS3.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

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

static const int outPorts[4] = {13, 12, 11, 10};

static const uint8_t in1Pin = 8;
static const uint8_t in2Pin = 7;
static const uint8_t enable1Pin = 6;

static const uint8_t servoPin = 5;

static const uint8_t touchPin = 3;

static const uint8_t tempPin = A0;
static const uint8_t micPin  = A1;

static const int rotationSpeed = 256;

// -------------------- Globals --------------------
static volatile bool touched = false;

// Replace with your network credentials
 char* ssid     = "Sunrise_2.4GHz_3C4FAC"; //Enter the router name 
 char* password = "Mp723phn3frd"; //Enter the router password  


static DHT_Unified dht(DHTPIN, DHTTYPE);
static Servo myservo;
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

// -------------------- Touch ISR --------------------
void handleTouchInterrupt() {
  touched = true;
}

// -------------------- Low-level helpers (RTOS-safe) --------------------
static void moveOneStep(bool dir) {
  static byte out = 0x01;

  if (dir) {
    out = (out << 1);
    if (out > 0x08) out = 0x01;
  } else {
    out = (out >> 1);
    if (out == 0x00) out = 0x08;
  }

  for (int i = 0; i < 4; i++) {
    digitalWrite(outPorts[i], (out & (0x01 << i)) ? HIGH : LOW);
  }
}

static void doStepperSequence() {
  // from your loop():
  // moveSteps(false, 2*64, 20); delay(1000);
  for (int i = 0; i < (2 * 64); i++) {
    moveOneStep(false);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  vTaskDelay(pdMS_TO_TICKS(1000));

  // moveSteps(true, 2*64, 20); delay(1000);
  for (int i = 0; i < (2 * 64); i++) {
    moveOneStep(true);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
}

static void driveMotor(bool dir, int spd) {
  // same as your driveMotor
  if (dir) {
    digitalWrite(in1Pin, HIGH);
    digitalWrite(in2Pin, LOW);
  } else {
    digitalWrite(in1Pin, LOW);
    digitalWrite(in2Pin, HIGH);
  }
  analogWrite(enable1Pin, constrain(spd, 0, 255));
}

static void doDCSequence() {
  // from your loop() (rotationDir starts at 0 in original sketch)
  bool rotationDir = 0;

  driveMotor(rotationDir, map(rotationSpeed, 0, 512, 0, 255));
  rotationDir = !rotationDir;
  vTaskDelay(pdMS_TO_TICKS(1000));

  driveMotor(rotationDir, map(0, 0, 512, 0, 255));
  vTaskDelay(pdMS_TO_TICKS(500));

  driveMotor(rotationDir, map(rotationSpeed, 0, 512, 0, 255));
  rotationDir = !rotationDir;
  vTaskDelay(pdMS_TO_TICKS(1000));

  driveMotor(rotationDir, map(0, 0, 512, 0, 255));
  vTaskDelay(pdMS_TO_TICKS(1000));
}

static void doServoSequence() {
  int posservo = random(0, 181);
  int poslimit = constrain(posservo, 0, 180);
  myservo.write(poslimit);
  Serial.println(" Servo moved");
}

static void doThermistorRead() {
  // same as your readAndDisplayTemperature()
  int adcVal = analogRead(tempPin);

  float v = adcVal * 5.0 / 1024;
  float Rt = 10 * v / (5 - v);
  float tempK = 1 / (log(Rt / 10) / 3950 + 1 / (273.15 + 25));
  float tempC = tempK - 273.15;

  Serial.print("Current temperature is: ");
  Serial.print(tempK);
  Serial.print(" K, ");
  Serial.print(tempC);
  Serial.println(" C");
  Serial.print("Raw ADC: ");
  Serial.println(adcVal);

  vTaskDelay(pdMS_TO_TICKS(50)); // matches your delay(50) after temperature
}

static void doDHTRead() {
  // same as your readDHTSensor() but RTOS-safe delay
  vTaskDelay(pdMS_TO_TICKS(1000));

  sensors_event_t event;

  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println("Error reading humidity!");
  } else {
    Serial.print("Humidity: ");
    Serial.print(event.relative_humidity);
    Serial.print("%, ");
  }

  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println("Error reading temperature!");
  } else {
    Serial.print("Temperature: ");
    Serial.print(event.temperature);
    Serial.println("C");
  }

  vTaskDelay(pdMS_TO_TICKS(50)); // matches your delay(50) after DHT
}

static void doMicRead() {
  int sensorValue = analogRead(micPin);
  Serial.print("Microphone: ");
  Serial.println(sensorValue);
}

static void doTouchDisplay() {
  if (touched) {
    Serial.println("Touch Detected!");
    touched = false;
  }
}



static void printRtosStats() {
    Serial.println("---- Task Stack HWM (words) ----");
  if (hStepper) { Serial.print("Stepper   : "); Serial.println(uxTaskGetStackHighWaterMark(hStepper)); }
  if (hDC)      { Serial.print("DC        : "); Serial.println(uxTaskGetStackHighWaterMark(hDC)); }
  if (hServo)   { Serial.print("Servo     : "); Serial.println(uxTaskGetStackHighWaterMark(hServo)); }
  if (hTherm)   { Serial.print("Thermistor: "); Serial.println(uxTaskGetStackHighWaterMark(hTherm)); }
  if (hDHT)     { Serial.print("DHT       : "); Serial.println(uxTaskGetStackHighWaterMark(hDHT)); }
  if (hMic)     { Serial.print("Mic       : "); Serial.println(uxTaskGetStackHighWaterMark(hMic)); }
  if (hTouch)   { Serial.print("Touch     : "); Serial.println(uxTaskGetStackHighWaterMark(hTouch)); }
  if (hMonitor) { Serial.print("Monitor   : "); Serial.println(uxTaskGetStackHighWaterMark(hMonitor)); }

  Serial.println("---- RTOS Stats ----");
  Serial.print("FreeHeap="); Serial.println(xPortGetFreeHeapSize());
  Serial.print("MinEver =" ); Serial.println(xPortGetMinimumEverFreeHeapSize());
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
static void TaskStepper(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    doStepperSequence();
    Serial.println(" Stepper moved");
    xTaskNotifyGive(hDC);
  }
}

static void TaskDC(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    doDCSequence();
    Serial.println(" DC moved");
    xTaskNotifyGive(hServo);
  }
}

static void TaskServo(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    doServoSequence();
    xTaskNotifyGive(hTherm);
  }
}

static void TaskThermistor(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    doThermistorRead();
    xTaskNotifyGive(hDHT);
  }
}

static void TaskDHT(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    doDHTRead();
    xTaskNotifyGive(hMic);
  }
}

static void TaskMic(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    doMicRead();
    xTaskNotifyGive(hTouch);
  }
}

static void TaskTouch(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    doTouchDisplay();
    xTaskNotifyGive(hStepper); // start next cycle
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
  pinMode(enable1Pin, OUTPUT);

  pinMode(tempPin, INPUT);
  pinMode(micPin, INPUT);

  pinMode(touchPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(touchPin), handleTouchInterrupt, RISING);

  myservo.attach(servoPin);

  // WiFi (kept from your setup)
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected! Waiting for DHCP address...");
  while (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    delay(10);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
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
  ok = xTaskCreate(TaskStepper,   "TaskStepper",   80, nullptr, 2, &hStepper);
  if (ok != pdPASS) { Serial.println("TaskStepper create failed"); for(;;){} }

  ok = xTaskCreate(TaskDC,        "TaskDC",        80, nullptr, 2, &hDC);
  if (ok != pdPASS) { Serial.println("TaskDC create failed"); for(;;){} }


  ok =  xTaskCreate(TaskServo,     "TaskServo",     80, nullptr, 2, &hServo);
  if (ok != pdPASS) { Serial.println("TaskServo create failed"); for(;;){} }


  ok = xTaskCreate(TaskThermistor,"TaskTherm",    100, nullptr, 2, &hTherm);
  if (ok != pdPASS) { Serial.println("TaskTherm create failed"); for(;;){} }


  ok = xTaskCreate(TaskDHT,       "TaskDHT",       200, nullptr, 1, &hDHT);
  if (ok != pdPASS) { Serial.println("TaskDHT create failed"); for(;;){} }


  ok = xTaskCreate(TaskMic,       "TaskMic",       80, nullptr, 1, &hMic);
  if (ok != pdPASS) { Serial.println("TaskMic create failed"); for(;;){} }


  ok = xTaskCreate(TaskTouch,     "TaskTouch",    60, nullptr, 2, &hTouch);
  if (ok != pdPASS) { Serial.println("TaskTouch create failed"); for(;;){} }

  ok =   xTaskCreate(TaskMonitor, "Monitor", 100, nullptr, 1, &hMonitor);
  if (ok != pdPASS) { Serial.println("Monitor create failed"); for(;;){} }


  Serial.print("Free heap bytes: ");
Serial.println(xPortGetFreeHeapSize());

  if (hStepper == nullptr) { Serial.println("hStepper null"); for(;;){} }
  xTaskNotifyGive(hStepper);

  // Kick off the chain
  xTaskNotifyGive(hStepper);

  vTaskStartScheduler();
}

void loop() {
  // Not used once RTOS scheduler is running
}