
#include <SPI.h>
#include <EEPROM.h>
#include <MFRC522.h>
#include <EnableInterrupt.h>

// To tidy up the code we put the function definitions here - Mainly here as I use Visual Studio for Arduino dev
void dump_byte_array(byte *buffer, byte bufferSize);
void copy_byte_array(byte *buffer, byte *current, byte bufferSize);
void register_beer(byte pumpId, byte* beerId);
void register_rating(byte pumpId, byte rating);
void register_pump(byte pumpId);
// End of function definitions

// To avoid using the "magic" of MACROS I use const byte variables - It may take up more memory so may need to be reverted to #define macros
// The first set are the pins that the 5 rating buttons are connected to
const byte BUTTON_1_PIN = 2;
const byte BUTTON_2_PIN = 3;
const byte BUTTON_3_PIN = 4;
const byte BUTTON_4_PIN = 5;
const byte BUTTON_5_PIN = 6;
// End of rating buttons

// The following set of pins are those used for reading the RFID
const byte MFRC522_RST_PIN = 9;
const byte MFRC522_SS_PIN = 10;
// End of RFID pins

// It may be worth adding an LED to signify errors etc as we will not have Serial connected when actually running
const byte LED_PIN = 0;

//  This is to allow the default base url to be defined within the code - This needs to be updated to use the correct ip address for the actual web server
const String BASE_URL = "http://localhost:8080";

// We set the pumpId to initially be 0 so that we can see if it has been set
byte pumpId = 0;

// This is simply a flag to show that the system is in error and to stop unecessary processing
bool error = false;

// A constant variable 
const byte PUMPID_EEPROM_ADDRESS = 0;

// Initialisation of the array as C/C++ does not allow unbounded arrays
byte currentBeer[] = { 0,0,0,0 };

// Object used to interact with the RFID reader
MFRC522 mfrc522(MFRC522_SS_PIN, MFRC522_RST_PIN);

// The current rating - Set to 0 if there is no rating to process. It is declared volatile as it is modified outside of the loop/setup methods via interrupts
volatile byte rating = 0;

// A simple constant int to define the debounce time applied to button presses
const int DEBOUNCE_TIME = 750;

// An int to track the last action time - used in conjunction with DEBOUNCE_TIME
int lastActionTime = 0;

// The following are the functions that are registered for each of the interrupts applied to the button pins
void button1_interrupt()
{
	if (millis() - lastActionTime > DEBOUNCE_TIME) {
		rating = 1;
		lastActionTime = millis();
	}

}

void button2_interrupt()
{
	if (millis() - lastActionTime > DEBOUNCE_TIME) {
		rating = 2;
		lastActionTime = millis();
	}
}

void button3_interrupt()
{
	if (millis() - lastActionTime > DEBOUNCE_TIME) {
		rating = 3;
		lastActionTime = millis();
	}
}

void button4_interrupt()
{
	if (millis() - lastActionTime > DEBOUNCE_TIME) {
		rating = 4;
		lastActionTime = millis();
	}
}

void button5_interrupt()
{
	if (millis() - lastActionTime > DEBOUNCE_TIME) {
		rating = 5;
		lastActionTime = millis();
	}
}
// end of Interrupt functions

/* The following two functions are here to allow easy enabling and disabling of the button interrupts -
* i.e. we disable whilst reading RFID and also when processing a rating -
*  we then enable again once that processing has finished
*/
void enableButtonInterrupts()
{
	enableInterrupt(BUTTON_1_PIN, button1_interrupt, FALLING);
	enableInterrupt(BUTTON_2_PIN, button2_interrupt, FALLING);
	enableInterrupt(BUTTON_3_PIN, button3_interrupt, FALLING);
	enableInterrupt(BUTTON_4_PIN, button4_interrupt, FALLING);
	enableInterrupt(BUTTON_5_PIN, button5_interrupt, FALLING);
}

void disableButtonInterrupts()
{
	disableInterrupt(BUTTON_1_PIN);
	disableInterrupt(BUTTON_2_PIN);
	disableInterrupt(BUTTON_3_PIN);
	disableInterrupt(BUTTON_4_PIN);
	disableInterrupt(BUTTON_5_PIN);
}
// End of enable/disable interrupts

// Normal setup method
void setup()
{
	// Waiting for serial to connect
	Serial.begin(9600);
	while (!Serial) {};

	// Initialise RFID reader
	SPI.begin();
	mfrc522.PCD_Init();
	// End initialise RFID reader

	// Enable LED output
	pinMode(LED_PIN, OUTPUT);
	// Turn the led off initialy
	digitalWrite(LED_PIN, LOW);

	// Initialise the buttons to be inputs
	pinMode(BUTTON_1_PIN, INPUT_PULLUP);
	pinMode(BUTTON_2_PIN, INPUT_PULLUP);
	pinMode(BUTTON_3_PIN, INPUT_PULLUP);
	pinMode(BUTTON_4_PIN, INPUT_PULLUP);
	pinMode(BUTTON_5_PIN, INPUT_PULLUP);

	// End of button initialisation

	// Lets have a snooze for a second
	delay(1000);

	// We can now use the buttons to set the pumpId
	// This uses the buttons to signify the bits of the pumpId
	// button 1 - bit 0 (least significant bit)
	// button 5 - bit 4
	// This allows the pumpId to be overriden to a value between 0 to 31
	auto button1State = digitalRead(BUTTON_1_PIN);
	Serial.print("Button 1 state: ");
	Serial.println(button1State);
	digitalWrite(13, button1State);
	if (button1State == LOW)
	{

		bitSet(pumpId, 0);
	}

	auto button2State = digitalRead(BUTTON_2_PIN);
	if (button2State == LOW)
	{
		bitSet(pumpId, 1);
	}

	auto button3State = digitalRead(BUTTON_3_PIN);
	if (button3State == LOW)
	{
		bitSet(pumpId, 2);
	}

	auto button4State = digitalRead(BUTTON_4_PIN);
	if (button4State == LOW)
	{
		bitSet(pumpId, 3);
	}

	auto button5State = digitalRead(BUTTON_5_PIN);
	if (button5State == LOW)
	{
		bitSet(pumpId, 4);
	}
	// End of reading pumpId from buttons

	// We now read the saved pumpId from the EEPROM<
	auto savedPumpId = EEPROM.read(PUMPID_EEPROM_ADDRESS);
	// Log the message
	Serial.print("Saved Pump ID: ");
	Serial.println(savedPumpId);


	if (pumpId == 0) {

		// Just a check to see if it is a realistic value (this also convieniently allows us to set the value to 31 by pressing all buttons so that when it restarts it is ignored to demo error condition)
		if (savedPumpId < 30)
		{
			Serial.println("Using saved pump ID");
			// We set the pumpId to be the one retrieved from the EEPROM
			pumpId = savedPumpId;
		}
	}
	else
	{
		// We got here if the buttons were used to set the pumpId - If it is different from the saved one then we save it to EEPROM
		if (savedPumpId != pumpId)
		{
			Serial.println("Saving PumpID");
			EEPROM.write(PUMPID_EEPROM_ADDRESS, pumpId);
		}

	}

	// Just print to ensure the pump id is as we expect
	Serial.print("Pump ID: ");
	Serial.println(pumpId);

	// We then check again to see if there is a valid pumpId (i.e. its greater than 0);
	if (pumpId == 0)
	{
		// We set error to be true so that in the loop we don't do unecessary processing
		error = true;
		Serial.println("ERROR: Pump ID is 0");

	}
	else
	{
		// If we get here then we think its a valid pumpId so we can then register it with the webserver
		register_pump(pumpId);
	}

	// Now that we have finished using the buttons for identifying the pumpId we can now enable the interrupts
	enableButtonInterrupts();
	Serial.println("**** End of Setup ****");
} // End of setup

void loop()
{

	if (!error)
	{
		// If there is not an error from setup
		// We check to see if there is a new RFID present

		if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())
		{
			// We disable the button interrupts whilst we handle the RFID reading
			disableButtonInterrupts();

			// Signify that we are reading the RFID
			//digitalWrite(LED_PIN, HIGH);

			// Log the new id to serial
			Serial.print("New RFID present: UID: ");
			dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);

			// Copy the new id to the currentBeer field
			copy_byte_array(mfrc522.uid.uidByte, currentBeer, mfrc522.uid.size);
			Serial.println("");
			// Sleep for a couple of seconds to allow user to remove card
			delay(2000);

			// Call out to the wifi client to register the beer with the webserver
			register_beer(pumpId, currentBeer);
			// Perform multiple flashes of the LED
			//digitalWrite(LED_PIN, LOW);
			//// Flash the led a few times if connected
			//for(auto i = 0; i<5; i++)
			//{
			//	delay(500);
			//	digitalWrite(LED_PIN, HIGH);
			//	delay(500);
			//	digitalWrite(LED_PIN, LOW);
			//}

			// Re-enable the interrupts
			enableButtonInterrupts();
		}

		// If an interrupt fires then we will have a rating that is greater than 0
		if (rating > 0)
		{
			// Disable the interrupts whilst we process
			disableButtonInterrupts();
			Serial.print("Registering rating of ");
			Serial.println(rating);
			// Call out to the wifi client to register the rating with the pump
			register_rating(pumpId, rating);
			// Reset rating to 0 so that future interrupts are heard (and this one is not re-processed)
			rating = 0;
			// Re-enable the interrupts
			enableButtonInterrupts();
		}
	}
	else
	{
		// If there is an error steady blink the LED
		/*digitalWrite(LED_PIN, HIGH);
		delay(500);
		digitalWrite(LED_PIN, LOW);
		delay(500);*/
	}

}

// Function to dump the byte array (of the RFID) to serial
void dump_byte_array(byte *buffer, byte bufferSize)
{
	for (byte i = 0; i < bufferSize; i++) {
		Serial.print(buffer[i] < 0x10 ? " 0" : " ");
		Serial.print(buffer[i], HEX);
	}
}

// Copy byte array from one variable to another (allows us to set the currentBeer varialbe);
void copy_byte_array(byte *buffer, byte *current, byte bufferSize)
{
	for (byte i = 0; i < bufferSize; i++)
	{
		current[i] = buffer[i];
	}
}

// Function that will call out to the wifi client (just prints to serial at the moment)
void register_beer(byte pumpId, byte* beerId)
{

	String pumpStr = pumpId < 10 ? String("0") + pumpId : String(pumpId);


	String url = BASE_URL + "/v2/api/pump/pump%20" + pumpStr
		+ String("/beer/")
		+ String(beerId[0], HEX)
		+ String(beerId[1], HEX)
		+ String(beerId[2], HEX)
		+ String(beerId[3], HEX);

	Serial.print("Calling url: ");
	Serial.println(url);

}

// Function that will call out to the wifi client (just prints to serial at the moment)
void register_rating(byte pumpId, byte rating)
{
	String pumpStr = pumpId < 10 ? String("0") + pumpId : String(pumpId);

	String url = BASE_URL + "/v2/api/pump/pump%20" + pumpStr
		+ String("/rating/")
		+ String(rating);

	Serial.print("Calling url: ");
	Serial.println(url);
}

// Function that will call out to the wifi client (just prints to serial at the moment)
void register_pump(byte pumpId)
{
	String pumpStr = pumpId < 10 ? String("0") + pumpId : String(pumpId);

	String url = BASE_URL + "/v2/api/pump/pump%20" + pumpStr;

	Serial.print("Calling url: ");
	Serial.println(url);
}
