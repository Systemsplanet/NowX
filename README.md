prompt used to create this project

As a senior software developer 


Who specializes in ESP32 using Arduino IDE,

Build a library that is a wrapper around ESP-NOW that supports sending any length of message. Sending large files should be possible and efficient. 

create software with concise variable and function names

Each message sent should include a json header with:

-magic string REQ
-destination host name 
‐client mac address
-sequential message number
-blocks total number
-block number

Followed by the raw data from the sender.


For example:

{REQ,test.com,ad:fe:de:ee:99:de,2,1,1}data


Include buffering if message is too long to fit in a ESP-NOW message.


The result message object should include methods for extracting the header values and the data


Reply to each message with an ACK json that includes:

-magic string ACK
-server mac address 
-sequential message number
-server timestamp unsigned long 

Eg 
{ACK,ad:fe:de:ee:99:de,1,177993006}



Clients automatically sweep standard 2.4GHz Wi-Fi channels (1-11) until they receive an instant ACK (acknowledgment) from the server, preventing packet loss if the router assigns the server a new channel.



Send returns true if it was sent and ACK'd


Handle any failed messages gracefully 


Make a very easy to use api 

Client:
send("my message");

Server:
Message msg = receive();
String reply = msg.data;






Sync the client ESP32 RTC using the timestamp from server ACK.


Add clean Serial logging to help with debugging. 
Create a common wrapper around Serial.printf and Serial.println
logf()
logln()

All log lines start with prefix: hhmmss 
Eq
010301 log_message



   output a zip file with all the files.



Include unit tests to test all the functionality



