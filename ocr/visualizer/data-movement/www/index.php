<!DOCTYPE html>
<html>
<?php session_start(); ?>
<head>
	<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
	<title>Data Movement Sample</title>
	<link type="text/css" href="data-movement.css" rel="stylesheet">
	<script type="text/javascript" src="jit.js"></script>
	<script type="text/javascript" src="//ajax.googleapis.com/ajax/libs/jquery/1.10.2/jquery.min.js"></script>
	<script type="text/javascript" src="data-movement.js"></Script>
	<script type="text/javascript">
	function checkKeyPress(e) {
		if (typeof e == 'undefined' && window.event) { e = window.event; }
		if (e.which == 13)
		{
			document.getElementById('db_submit').click();
		}
	}
	</script>
</head>
<body onload="init()">
<div id="controls" style="height: 26px">
<div style="float: left">
<form name="edt_select">
<select name="edt_list" onchange="updateEdt()">
<option value="">EDT list</option>
</select>
</form>
</div>
<div style="float: right">
Select database:
<input type="text" id="db_path" onKeyPress="checkKeyPress(event);">
<input type="button" id="db_submit" onclick="loadDb()" value="Load">
</div>
</div>
<div id="chart-container"></div>
</body>
</html>
