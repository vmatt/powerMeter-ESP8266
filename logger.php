<?php
error_reporting(-1);
if(isset($_GET['amper'])) {
    $get_amper=htmlspecialchars($_GET["amper"]);
}
<?php

function squery ($queryp) {
	$servername = "...";  //postgreSQL server address
	$username = "...";
	$password = "...";
	try {
	  $conn = new PDO("pgsql:host=$servername;port=5432;dbname=YOUR_DB_NAME", $username, $password);
	  // set the PDO error mode to exception
	  $conn->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
	  
	  $sql = $queryp;
	  $result= $conn->query($sql)->fetchAll();
	  $conn=null;
	  return $result;
	  
	  
	} catch(PDOException $e) {
	  echo "DB connection error: " . $e->getMessage();
	}
}
?>
$result = squery("INSERT INTO logger(timestamp,amp) VALUES (now(), ".htmlspecialchars($get_amper).");");
echo $result;
?>

