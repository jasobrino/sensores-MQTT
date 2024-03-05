const char HTML_CAB[] = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
        <title>control estacion</title>
</head>
<body onload="docReady()">
<script>
    function docReady() {
        document.getElementById("fupload").onchange = function() {
            document.getElementById("msgUpload").innerHTML = "<h2>Espere a que termine el upload</h2>";
            document.getElementById("frmUpload").submit();
        }
    }
</script>
    <style>
        table {  border-collapse: collapse; border: 2px solid;}
        input {
            cursor: pointer;
            font-size: 14px;
            font-weight: 600;
            border: 1px solid transparent;
            border-radius: .25rem;
            position: relative;
            margin: 2px;
        }
        .btFormat {
            padding: 12px 8px;
            background-color: #ef485b;
            color: #fff;
        }
        .btDescargar {
            width: 6em;
            padding: 4px;
            background-color:aqua;
        }
        .btBorrar {
            width: 6em;
            padding: 4px;
            background-color:rgb(249, 126, 18);
        }
        .row{
            text-align:center;
            display:flex;
        }      

        #files td, #files th {
            border: 1px solid #ddd;
            padding: 8px;
        }
        #files tr:nth-child(even){background-color: #f2f2f2;}
        #files tr:hover {background-color: #ddd;}
    </style>
)=====";

const char HTML_FIN[] = R"=====(
</body>
</html>
)=====";