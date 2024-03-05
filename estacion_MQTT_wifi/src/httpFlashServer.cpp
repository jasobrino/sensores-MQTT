
#include "httpFlashServer.h"

//webserver
WebServer server(80);

void startServer() {
  //ajustamos la hora servidor ntp
  setNTPTime();
  //crearmos el router del servidor web
  server.on("/", handleRoot);
  server.on("/download", handleDownloadFile);
  server.on("/fupload",  HTTP_POST,[](){ server.send(200);}, handleFileUpload);
  server.on("/delete", handleDelete);
  server.on("/format", handleFormat);
  server.on("/reset", handleReset);
  server.begin();
}

void serverHandleClient() {
  server.handleClient();
}


void  handleReset() {
  Serial.println("reseteando el ESP32...");
  String page = String(HTML_CAB);
  page.concat("<h3>enviado RESET</h3>");
  page.concat(HTML_FIN);
  server.send(200,"text/html", page);
  ESP.restart();
}

void handleRoot() {
  String content = String(HTML_CAB);
  if(!LittleFS.begin(false)) {
    Serial.println("fallo al montar LittleFS");
    return;
  }
  content.concat("<h1>Directorio memoria flash</h1>");
  String now = rtc.getTime("%02d/%02m/%Y %02H:%02M:%02S");
  content.concat("Fecha RTC interno: ");
  content.concat(now);
  content.concat("<br/>Tamaño total: ");
  content.concat(LittleFS.totalBytes());
  content.concat("  en uso: ");
  content.concat(LittleFS.usedBytes());  
  content.concat("  libre: ");
  content.concat(LittleFS.totalBytes() - LittleFS.usedBytes());  
  content.concat("<br/>");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  content.concat("<table id='files'><tr>");
  content.concat("<th>Archivo</th><th>tamaño</th><th>acciones</th></tr>");
  while(file) {
    content.concat("<tr><td>");
    content.concat(file.name());
    content.concat("</td><td style='text-align:right'>");
    content.concat(file.size());
    content.concat("</td><td><div class='row'><form action='/download'>");
    content.concat("<input type='hidden' name='filename' value='");
    content.concat(file.name());
    content.concat("'/>");
    content.concat("<input type='submit' class='btDescargar' value='descargar'/></form>");
    content.concat("<form action='/delete' onsubmit='return confirm(\"borrar archivo?\")'>");
    content.concat("<input type='hidden' name='filename' value='");
    content.concat(file.name());
    content.concat("'/>");
    content.concat("<input type='submit' class='btBorrar' value='borrar'/></form></div></td>");
    content.concat("</tr>");
    file = root.openNextFile();
  }
  content.concat("</table><br/>");
  content.concat("<form action='/fupload' id='frmUpload' method='post' enctype='multipart/form-data'>");
  content.concat("<span>Elija el archivo a subir:</span><br/>");
  content.concat("<input type='file' name='fupload' id='fupload'></form>");
  content.concat("<span id='msgUpload'></span>");
  content.concat("<form action='/format' onsubmit='return confirm(\"ATENCION: Si continua se perderan todos los archivos\")'>");
  content.concat("<br/><input type='submit' class='btFormat' value='formatear particion'/></form>");
  content.concat("<form action='/reset' method='post'>");
  content.concat("<input type='submit' class='btFormat' value='reset estacion'/></form>");
  content.concat(HTML_FIN);
  server.send(200, "text/html", content);
  file.close();
  root.close(); 
}

void handleDelete(){
  File f;
  if(!server.hasArg("filename")) {
    server.send(400, "text/html", "no se ha recibido filename");
    return;
  }
  String fileName = server.arg(0);
  if(!LittleFS.begin(false)) {
    Serial.println("fallo al montar LittleFS");
    return;
  }
  if(LittleFS.remove("/"+fileName)) {
    String page = String(HTML_CAB);
    page.concat("<h3>archivo borrado</h3>");
    page.concat("<a href='/'>[Volver]</a><br/><br/>");
    page.concat(HTML_FIN);
    server.send(200,"text/html", page);
  } else {
    server.send(400, "text/html", "error al borrar el archivo");
  } 
}

void handleFormat() {
  String page = String(HTML_CAB);
  //LitteFS se puede formatear sin iniciar con begin 
  Serial.println("formateando particion");
  if(LittleFS.format()) {
    Serial.println("particion formateada");
    page.concat("<h3>particion formateada con exito</h3>");
  } else {
    Serial.println("la particion no pudo formatearse");
    page.concat("<h3>ha fallado el formateo de la particion</h3>");
  }
  page.concat("<a href='/'>[Volver]</a><br/><br/>");
  page.concat(String(HTML_FIN));
  server.send(200,"text/html", page);
}

void handleDownloadFile() {
  static File fDown;
  if(!server.hasArg("filename")) {
    server.send(400, "text/html", "no se ha recibido filename");
    return;
  }
  Serial.printf("args: %d  - %s\n",server.args(), server.arg(0).c_str());
  String fileName = server.arg(0);
  if(!LittleFS.begin(false)) {
    Serial.println("fallo al montar LittleFS");
    return;
  }
  String rootFileName = "/";
  rootFileName.concat(fileName);
  if(LittleFS.exists(rootFileName)) {
    fDown = LittleFS.open(rootFileName);
    server.sendHeader("Content-Type", "text/text");
    server.sendHeader("Content-Disposition", "attachment; filename="+fileName);
    server.sendHeader("Connection", "close");
    server.streamFile(fDown, "application/octet-stream");
    fDown.close();
  } else {
    server.send(400,"text/html","archivo no encontrado");
  }
}

void handleFileUpload() {
  HTTPUpload& fileUpload = server.upload();
  static File fUpld;
  String RootFileName;

  if(fileUpload.status == UPLOAD_FILE_START) {
    RootFileName = "/";
    RootFileName.concat(fileUpload.filename);
    Serial.printf("fichero a subir: %s\n", RootFileName.c_str());
    if(!LittleFS.begin(false)) {
      Serial.println("fallo al montar LittleFS");
      return;
    }
    if(LittleFS.exists(RootFileName)) {
      Serial.println("borrando archivo antes de subirlo");
      LittleFS.remove(RootFileName);
    }
    fUpld = LittleFS.open(RootFileName, FILE_WRITE);
    Serial.print("creando archivo");
  }
  else if(fileUpload.status == UPLOAD_FILE_WRITE) {
    fUpld.write(fileUpload.buf, fileUpload.currentSize);  
    //Serial.print('.');
  }
  else if(fileUpload.status == UPLOAD_FILE_END) {
    fUpld.close();
    String linea = "\ntotal subido: en bytes: " + String(fileUpload.totalSize);
    Serial.println(linea);
    server.sendHeader("Connection", "close");
    String page = String(HTML_CAB);
    page.concat("<h3>archivo añadido</h3>");
    page.concat("<a href='/'>[Volver]</a><br/><br/>");
    page.concat(String(HTML_FIN));
    server.send(200,"text/html", page);
  }
}
