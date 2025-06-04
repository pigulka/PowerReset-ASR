#ifndef PASSWORD_FORM_H
#define PASSWORD_FORM_H

#include <Ethernet.h>

// Formularz zmiany hasła – wymagane: stare hasło, nowe hasło, potwierdzenie nowego hasła
const char PASSWORD_FORM_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Zmiana Hasła</title>
  <style>
    body { 
      font-family: Arial, sans-serif; 
      background: #222; 
      color: #ccc; 
      margin: 0; 
      padding: 20px; 
      text-align: center; 
    }
    .container { 
      max-width: 400px; 
      margin: 0 auto; 
      background: #333; 
      padding: 20px; 
      border-radius: 8px; 
    }
    input[type="password"] { 
      width: 100%; 
      padding: 10px; 
      margin: 10px 0; 
      border: 1px solid #555; 
      border-radius: 4px; 
      background: #444; 
      color: #ccc; 
    }
    input[type="submit"] { 
      padding: 10px 20px; 
      background: #007bff; 
      color: #fff; 
      border: none; 
      border-radius: 4px; 
      cursor: pointer; 
    }
    input[type="submit"]:hover { 
      background: #0056b3; 
    }
    a { 
      color: #0af; 
      text-decoration: none; 
    }
    a:hover { 
      text-decoration: underline; 
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Zmiana Hasła</h1>
    <form method="POST" action="/changePassword">
      <label>Stare hasło:<br>
        <input type="password" name="oldPass">
      </label><br>
      <label>Nowe hasło:<br>
        <input type="password" name="newPass">
      </label><br>
      <label>Powtórz nowe hasło:<br>
        <input type="password" name="confirmPass">
      </label><br><br>
      <input type="submit" value="Zmień Hasło">
    </form>
    <p><a href="/">Powrót</a></p>
  </div>
</body>
</html>
)rawliteral";

#endif // PASSWORD_FORM_H
