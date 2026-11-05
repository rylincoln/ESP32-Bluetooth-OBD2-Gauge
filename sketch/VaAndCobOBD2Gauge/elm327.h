//elm327 interpreter

String getPID(String pid) {//fucntion getpid response from elm327
    while (BTSerial.available()>0) {//clear rx buffer
      BTSerial.read();
    }
   String bt_response = "";
   BTSerial.print(pid+"\r");//get no of dtc
   delay(500);//wait for response
   while (BTSerial.available()>0) {//reading Bluetooth
    char incomingChar = BTSerial.read();
    //check CR/NL
    if (incomingChar == '>') {
      Serial.println(bt_response);
      BTSerial.flush();
    } else {
      bt_response.concat(incomingChar);//keep elm327 respond
    }
   } //while BTSerial
   return bt_response;
}
//----------------------------

