// Header file  to allow EEPROM_writeAnything  and
// EEPROM_readAnything examples to compile under arduino-0022
//
// Due to the (broken) way that Arduino version 0022 collects function
// prototypes before other things in the main sketch, template
// stuff must be in a separate header
//
// davekw7x
//
#include <EEPROM.h>
#include <WProgram.h>

template <class T> int EEPROM_writeAnything(int ee, const T& value)
{
    const byte* p = (const byte*)(const void*)&value;
    int i;
    for (i = 0; i < sizeof(value); i++)
	  EEPROM.write(ee++, *p++);
    return i;
}

template <class T> int EEPROM_readAnything(int ee, T& value)
{
    byte* p = (byte*)(void*)&value;
    int i;
    for (i = 0; i < sizeof(value); i++)
	  *p++ = EEPROM.read(ee++);
    return i;
}

