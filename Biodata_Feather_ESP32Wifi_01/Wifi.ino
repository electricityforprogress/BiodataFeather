boolean displayMode = true; //used for web page demo
boolean white = false; // used for web page demo

const String HtmlHtml = "<html><head>"
                        "<meta name='viewport' content='width=device-width, initial-scale=1' /></head>";
                        
const String HtmlTitle = "<h1>Welcome to Biodata </h1><br/>\n";

  
const String HtmlHtmlClose = "</html>";

String displayControl(){
  
   String displayHtml = "<br><br><br>";
   
   if(!displayMode) displayHtml = displayHtml + "<br><br><a href='/on'><button style='class:btn;height:20px;width:120px;color:white;background-color:green'>On</button></a> ";
   else displayHtml = displayHtml + "<br><br><a href='/off'><button style='class:btn;height:20px;width:120px;color:white;background-color:red'>Off</button></a> ";

   if(!white) displayHtml = displayHtml + "&nbsp; <a href='/white'><button style='class:btn;height:20px;width:80px;color:black;background-color:white'>White</button></a> ";
   else displayHtml = displayHtml + "&nbsp; <a href='/white'><button style='class:btn;height:20px;width:80px;color:white;background-color:orange'>Colors</button></a> ";
  
   return displayHtml;
}




void handleRoot() { //on connection to IP root
 // Serial.println("Root page");
  String htmlRes = HtmlHtml + HtmlTitle + ssid + "<br>\n" + displayControl() + HtmlHtmlClose;
  server.send(200, "text/html", htmlRes);
}

void handle_on(){
  Serial.println("Display On");
  displayMode = true;
  handleRoot();
}

void handle_off(){
  Serial.println("Display Off");
  displayMode = false; //toggle displaymode
  handleRoot();
}

void handle_white(){
  white = !white; //toggle
  handleRoot();
}


void handle_notFound(){ //passing some other variables
  String path = server.uri();
  String inValue = getValue(path,'/',2);

  int value = inValue.toInt();
  Serial.println("notFound Value: " + value);

}

void wifiConfig(){

}



String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;
  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}
