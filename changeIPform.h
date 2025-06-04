#ifndef CHANGE_NET_FORM_H
#define CHANGE_NET_FORM_H

const char CHANGE_NET_FORM_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Zmiana adresu IP</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background: #222;
      color: #ccc;
      padding: 20px;
      text-align: center;
    }
    .container {
      max-width: 400px;
      margin: auto;
      background: #333;
      padding: 20px;
      border-radius: 8px;
    }
    input[type="text"] {
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
    <h1>Zmiana adresu IP</h1>
    <form method="POST" action="/changeNet">
      <label>Nowy adres IP:<br>
        <input type="text" name="ip" placeholder="192.168.1.100">
      </label><br>
      <label>Bramka (Gateway):<br>
        <input type="text" name="gw" placeholder="192.168.1.1">
      </label><br>
      <label>Maska podsieci:<br>
        <input type="text" name="subnet" placeholder="255.255.255.0">
      </label><br>
      <label>DNS:<br>
        <input type="text" name="dns" placeholder="8.8.8.8">
      </label><br><br>
      <input type="submit" value="Zapisz zmiany">
    </form>
    <p><a href="/">Powrót</a></p>
  </div>
</body>
</html>
)rawliteral";

#endif // CHANGE_NET_FORM_H
