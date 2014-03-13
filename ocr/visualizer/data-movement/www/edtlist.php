<?php
session_start();
$table = "movement";

$edts = array();

if (isset($_SESSION['db'])) {
	$dbname = $_SESSION['db'];
} else {
}

try {
	// Open the database
	$conn = new PDO("sqlite:$dbname");
	$conn->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

	$results = $conn->query("SELECT DISTINCT edt FROM movement")->fetchAll();

	foreach ($results as $r) {
		$edts[] = $r['edt'];
	}
	echo json_encode($edts);
} catch (PDOException $e) {
	echo 'ERROR: ' . $e->getMessage() . '</br>';
	echo $e->getTraceAsString();
}
?>