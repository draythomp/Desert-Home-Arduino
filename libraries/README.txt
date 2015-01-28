These are the libraries I use in addition to the default libraries supplied
as part of the Arduino IDE release.

Exceptions:

SoftwareSerial - I picked up the version that is capable of more functions to use with
the enhanced version of the XBee library.

XBee library - I modified this library to support the ZigBee specific frame types and 
added a parameter to setSerial() to print out the characters as they come in or go out.
To use it, just add a parameter after the stream that is being used for communication

XBee.setSerial(Serial1, true);  // will enable debugging
XBee.setSerial(Serial1, false); // will turn debugging off

The normal XBee.setSerial(Serial1); // this works just like it did before.