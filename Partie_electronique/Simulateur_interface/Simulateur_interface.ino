/*Réaliser par HAMMAMI Mounir 23/02/2026 */

/*
  pin TX ---> 
  pin RX --->
*/
#define CMD_SBIB       0xC0 // commande pour demander les trames de la sortie SBI vers balise
#define CMD_SBIM1      0xD0 // commande pour demander les trames de la sortie SBI maintenance 1
#define CMD_SBIM2      0xD4 // commande pour demander les trames de la sortie SBI maintenance 2
#define CMD_SBIM3      0xD8 // commande pour demander les trames de la sortie SBI maintenance 3
#define CMD_SBIM4      0xDC // commande pour demander les trames de la sortie SBI maintenance 4
#define CMD_UCS        0xE0 // commande pour demander les trames de la sortie UCS de maintenance
#define CMD_test_train 0xF0 // commande pour simuler le passage du train

#define MAX_PACKET_SIZE 24
#define TIMEOUT_MS 10   // temps d'attente entre 2 octets

void envoie_commande_vers_stm(uint8_t commande);
void afficher_menu();

uint8_t buffer_rx[MAX_PACKET_SIZE];
uint8_t index_rx = 0;
unsigned long lastByteTime = 0;
bool receptionEnCours = false;

void setup() 
{
  Serial.begin(115200);     
  Serial1.begin(115200); 
  Serial.println("Systeme pret");
  afficher_menu();
}

void loop()
{
  static bool attente_commande = true;
  uint8_t commande = 0;

  // ----- Lecture commande utilisateur -----
  if (attente_commande && Serial.available() > 0)
  {
    String input = Serial.readStringUntil('\n');  // lit toute la ligne
    input.trim(); // enlève \r et espaces

    if (input.length() > 0)
      {
      commande = input.toInt();

      Serial.print("Commande choisie : ");
      Serial.println(commande);

      envoie_commande_vers_stm(commande);

      attente_commande = false;   // IMPORTANT
    }
  }
  
  // ----- Reception non bloquante STM -----
  while (Serial1.available() > 0)
  {
    if (index_rx < MAX_PACKET_SIZE)
    {
      buffer_rx[index_rx++] = Serial1.read();
      lastByteTime = millis();
      receptionEnCours = true;
    }
    else
    {
      Serial1.read(); // évite overflow
    }
  }

  // ----- Détection fin de paquet par timeout -----
  if (receptionEnCours && (millis() - lastByteTime > TIMEOUT_MS))
  {
    Serial.print("Paquet recu (");
    Serial.print(index_rx);
    Serial.println(" octets) :");

    for (uint8_t i = 0; i < index_rx; i++)
    {
      Serial.print("0x");
      Serial.print(buffer_rx[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    // Reset
    index_rx = 0;
    receptionEnCours = false;
    attente_commande = true;

    afficher_menu();
  }
}

void envoie_commande_vers_stm(uint8_t commande)
{
  switch(commande)
  {
    case 0 : Serial1.write(CMD_SBIB); break;
    case 1 : Serial1.write(CMD_SBIM1); break;
    case 2 : Serial1.write(CMD_SBIM2); break;
    case 3 : Serial1.write(CMD_SBIM3); break;
    case 4 : Serial1.write(CMD_SBIM4); break;  
    case 5 : Serial1.write(CMD_UCS); break;
    case 6 : Serial1.write(CMD_test_train); break;
    default:
      Serial.println("Commande invalide !");
      break;
  }
}

void afficher_menu()
{
  Serial.println("\n===== MENU COMMANDES =====");
  Serial.println("0 : SBIB");
  Serial.println("1 : SBIM1");
  Serial.println("2 : SBIM2");
  Serial.println("3 : SBIM3");
  Serial.println("4 : SBIM4");
  Serial.println("5 : UCS");
  Serial.println("6 : TEST TRAIN");
  Serial.println("==========================");
  Serial.println("Choisir un numero : ");
}