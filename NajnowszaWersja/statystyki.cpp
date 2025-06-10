#include "htmlpages.h"
#include "globals.h"
#include "functions.h"

void sendCommonHtmlHeader(EthernetClient &client, const String &title);
void sendCommonHtmlFooter(EthernetClient &client);



void sendStatsPage(EthernetClient &client) {
    sendCommonHtmlHeader(client, "");

    client.println("<div class='container' style='max-width:1080px; margin:38px auto 28px auto; background:#181e25; padding:32px 22px 22px 22px; border-radius:17px; box-shadow:0 4px 20px #10367a50; color:#e3eaf2;'>");
    //client.println("<h2 style='text-align:center; margin-top:0; margin-bottom:27px; font-size:1.36em; color:#47b5ff; letter-spacing:1px;'>Statystyki pracy urządzenia</h2>");

    // --- Skrypt AJAX ---
    client.println("<script>");
    client.println("function resetStats() {");
    client.println("  if(confirm('Na pewno wyzerować wszystkie statystyki?')){");
    client.println("    var xhttp = new XMLHttpRequest();");
    client.println("    xhttp.onreadystatechange = function(){");
    client.println("      if(this.readyState==4 && this.status==200){ reloadStats(); }");
    client.println("    };");
    client.println("    xhttp.open('POST','/resetStats',true); xhttp.send();");
    client.println("  }");
    client.println("}");
    client.println("function resetRestarts() {");
    client.println("  if(confirm('Zerować licznik restartów?')){");
    client.println("    var xhttp = new XMLHttpRequest();");
    client.println("    xhttp.onreadystatechange = function(){");
    client.println("      if(this.readyState==4 && this.status==200){ reloadStats(); }");
    client.println("    };");
    client.println("    xhttp.open('POST','/resetRestarts',true); xhttp.send();");
    client.println("  }");
    client.println("}");
    client.println("function reloadStats() {");
    client.println("  var xhttp = new XMLHttpRequest();");
    client.println("  xhttp.onreadystatechange = function(){");
    client.println("    if(this.readyState==4 && this.status==200){");
    client.println("      try {");
    client.println("        var d=JSON.parse(this.responseText);");
    client.println("        document.getElementById('uptime').innerHTML=Math.floor(d.deviceUptime/3600)+'h '+Math.floor((d.deviceUptime%3600)/60)+'m '+(d.deviceUptime%60)+'s';");
    client.println("        document.getElementById('totalUptime').innerHTML=Math.floor(d.totalUptime/3600)+'h';");
    client.println("        document.getElementById('deviceRestarts').innerHTML=d.deviceRestarts;");
    client.println("        document.getElementById('lastRestart').innerHTML=new Date(d.lastRestartEpoch*1000).toLocaleString('pl-PL');");
    client.println("        var relTab='';for(var i=0;i<d.relays.length;i++){relTab+='<tr><td>'+(i+1)+'</td><td>'+d.relays[i].name+'</td><td>'+d.relays[i].cycles+'</td><td>'+Math.floor(d.relays[i].active/3600)+'h</td></tr>';}");
    client.println("        document.getElementById('relayRows').innerHTML=relTab;");
    client.println("        var inpTab='';for(var i=0;i<d.inputs.length;i++){inpTab+='<tr><td>'+(i+1)+'</td><td>'+d.inputs[i].name+'</td><td>'+d.inputs[i].cycles+'</td><td>'+Math.floor(d.inputs[i].active/3600)+'h</td></tr>';}");
    client.println("        document.getElementById('inputRows').innerHTML=inpTab;");
    client.println("        document.getElementById('loginAttempts').innerHTML=d.loginAttempts;");
    client.println("        document.getElementById('failedLoginAttempts').innerHTML=d.failedLoginAttempts;");
    client.println("        document.getElementById('lastLogin').innerHTML=d.lastLoginTime+' z IP: '+d.lastLoginIP;");
    client.println("        document.getElementById('fwVersion').innerHTML=d.firmwareVersion;");
    client.println("        document.getElementById('fwDesc').innerHTML=d.firmwareDescription;");
    client.println("        document.getElementById('macAddr').innerHTML=d.mac;");
    client.println("        document.getElementById('ipAddr').innerHTML=d.ip;");
    client.println("        document.getElementById('subnet').innerHTML=d.subnet;");
    client.println("        document.getElementById('gateway').innerHTML=d.gateway;");
    client.println("        document.getElementById('dns').innerHTML=d.dns;");
    client.println("      } catch(e){ document.getElementById('uptime').innerHTML='Błąd danych!'; }");
    client.println("    }");
    client.println("  };");
    client.println("  xhttp.open('GET','/api/stats',true);");
    client.println("  xhttp.send();");
    client.println("}");
    client.println("window.onload=reloadStats;");
    client.println("</script>");

    // --- Styl FLEX + panel ---
    client.println("<style>");
    client.println(".statsFlex {display:flex; flex-wrap:wrap; gap:24px;}");
    client.println(".statsCol {flex:1 1 0; min-width:320px; max-width:49%; display:flex; flex-direction:column; gap:18px;}");
    client.println(".panelSection {background:#232a35; border-radius:13px; margin-bottom:0; box-shadow:0 2px 14px #113177bb; padding:19px 14px 13px 14px;}");
    client.println(".panelSection h3{color:#49a5ff; font-size:1.09em; margin:0 0 13px 0; font-weight:600; letter-spacing:.5px;}");
    client.println(".statsTable{width:100%;border-collapse:collapse;background:#20272d;margin:0 0 12px 0;}");
    client.println(".statsTable th, .statsTable td{border:1px solid #335d91;padding:7px 9px;text-align:center;font-size:0.99em;}");
    client.println(".statsTable th{background:#203c52;color:#6bc7ff;font-weight:600;}");
    client.println(".statsTable tr:nth-child(even){background:#212733;}");
    client.println(".statsTable tr:nth-child(odd){background:#232d3a;}");
    client.println(".panelBtn{background:#2196f3;color:#fff;border:none;padding:10px 22px;border-radius:7px;font-size:1em;cursor:pointer;margin-right:12px;box-shadow:0 2px 8px #1976d244;transition:background .17s,box-shadow .17s;}");
    client.println(".panelBtn:hover{background:#156ac7;box-shadow:0 2px 14px #2196f399;}");
    client.println(".panelBtnRed{background:#b03232;}");
    client.println(".panelBtnRed:hover{background:#f02c2c;}");
    client.println("@media (max-width:900px){.statsFlex{flex-direction:column;}.statsCol{max-width:100%;}}");
    client.println("</style>");

    // FLEX UKŁAD: dwie kolumny
    client.println("<div class='statsFlex'>");

    // LEWA KOLUMNA
    client.println("<div class='statsCol'>");

    client.println("<div class='panelSection'><h3>&#128221; <span style='color:#47b5ff'>Statystyki logowania do systemu</span></h3>");
    client.println("<table style='width:100%'><tr><td><b>Liczba wszystkich prób logowania:</b></td><td><span id='loginAttempts'>...</span></td></tr>");
    client.println("<tr><td><b>Liczba nieudanych prób:</b></td><td><span id='failedLoginAttempts'>...</span></td></tr>");
    client.println("<tr><td><b>Ostatnie udane logowanie:</b></td><td><span id='lastLogin'>...</span></td></tr>");
    client.println("</table></div>");

    client.println("<div class='panelSection'><h3>&#128187; <span style='color:#47b5ff'>Informacje o systemie i sieci</span></h3>");
    client.println("<table style='width:100%'><tr><td><b>Wersja firmware:</b></td><td><span id='fwVersion'>...</span></td></tr>");
    client.println("<tr><td><b>Opis aktualizacji:</b></td><td><span id='fwDesc'>...</span></td></tr>");
    client.println("<tr><td><b>Adres MAC:</b></td><td><span id='macAddr'>...</span></td></tr>");
    client.println("<tr><td><b>Adres IP:</b></td><td><span id='ipAddr'>...</span></td></tr>");
    client.println("<tr><td><b>Maska:</b></td><td><span id='subnet'>...</span></td></tr>");
    client.println("<tr><td><b>Brama:</b></td><td><span id='gateway'>...</span></td></tr>");
    client.println("<tr><td><b>Serwer DNS:</b></td><td><span id='dns'>...</span></td></tr>");
    client.println("</table></div>");

    // --- Przyciski ---
    client.println("<div style='margin-top:19px;text-align:center;'>");
    client.println("<button class='panelBtn panelBtnRed' onclick='resetStats()' style='margin-right:11px;'>Zeruj wszystkie statystyki</button>");
    client.println("<button class='panelBtn' onclick='resetRestarts()'>Zeruj liczbę restartów</button>");
    client.println("</div>");

    client.println("</div>"); // .statsCol LEFT

    // PRAWA KOLUMNA
    client.println("<div class='statsCol'>");

    client.println("<div class='panelSection'><h3>&#9200; <span style='color:#47b5ff'>Czas pracy i restarty</span></h3>");
    client.println("<table style='width:100%'><tr><td><b>Aktualny uptime:</b></td><td><span id='uptime'>...</span></td></tr>");
    client.println("<tr><td><b>Całkowity czas pracy:</b></td><td><span id='totalUptime'>...</span></td></tr>");
    client.println("<tr><td><b>Liczba restartów:</b></td><td><span id='deviceRestarts'>...</span></td></tr>");
    client.println("<tr><td><b>Ostatni restart:</b></td><td><span id='lastRestart'>...</span></td></tr>");
    client.println("</table></div>");

    client.println("<div class='panelSection'><h3>&#128267; <span style='color:#47b5ff'>Przekaźniki</span></h3>");
    client.println("<table class='statsTable'><tr><th>#</th><th>Nazwa</th><th>Ilość cykli</th><th>Czas aktywności</th></tr>");
    client.println("<tbody id='relayRows'></tbody>");
    client.println("</table></div>");

    client.println("<div class='panelSection'><h3>&#128272; <span style='color:#47b5ff'>Wejścia</span></h3>");
    client.println("<table class='statsTable'><tr><th>#</th><th>Nazwa</th><th>Ilość cykli</th><th>Czas aktywności</th></tr>");
    client.println("<tbody id='inputRows'></tbody>");
    client.println("</table></div>");

    client.println("</div>"); // .statsCol RIGHT

    client.println("</div>"); // .statsFlex

    client.println("</div>"); // container

    sendCommonHtmlFooter(client);
}




