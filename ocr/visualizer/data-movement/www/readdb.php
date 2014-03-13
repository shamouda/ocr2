<?php
session_start();

$table = "movement";
if (isset($_SESSION['db'])) {
	$dbname = $_SESSION['db'];
}

function varDumpToString ($var)
{
	ob_start();
	var_dump($var);
	$result = ob_get_clean();
	return $result;
}

function query_str()
{
	$query = "SELECT size,source,destination FROM " . $GLOBALS['table'];
	if (isset($_GET['edt'])) {
		$query = $query . ' WHERE edt="' . $_GET['edt'] . '"';
	}
	return $query;
}

function name($rank)
{
	switch ($rank) {
		case 0:
			return "Board (DRAM)";
		case 1:
			return "Chip ";
		case 2:
			return "Unit ";
		case 3:
			return "Block ";
		case 4:
			return "XE ";

		default:
			return "";
	}
}

// Recursive function to build a tree
function add_descendants(&$tree_node, $family)
{
	$children = array();
	foreach ($family as $child => $parent) {
		if ((string)$parent === (string)$tree_node['id']) {
			$children[] = $child;
		}
	}
	$kids = array();
	foreach ($children as $child) {
		$temp = explode('.', $child);
		$index = (int)end($temp);
		$name = name(count($temp)) . $index;
		$kids[$index] = array(
				'id' => $child,
				'name' => $name,
				'data' => array(
					'inside' => 0,
					'outside' => 0),
				'children' => array());
	}
	// This is necessary to make PHP consider it an
	// unindexed array. Otherwise breaks json_encode.
	sort($kids);
	foreach ($kids as $k) {
		$tree_node['children'][] = $k;
	}
 	foreach ($tree_node['children'] as $i => $child) {
 		add_descendants($child, $family);
 		$tree_node['children'][$i] = $child;
 	}
}
/**
 * Gets a reference to given scratchpad in a heirarchy with a given ID, using the known structure.
 * @param array $mems The heirarchy of memories
 * @param string $name The ID of the scatchpad, in the form "X.X.X" (or "b" for the root)
 * @return array
 */
function &scratchpad_by_name(&$mems, $name)
{
	$rv =& $mems;
	if ($mems['id'] == $name) {
		return $mems;
	}
	$levels = explode('.', $name);
	for ($i = 0; $i < count($levels); $i++) {
// 		print "=== LEVEL $i ===\n";
// 		var_dump(array_keys($rv['children']));
		$rv =& $rv['children'][$levels[$i]];
	}
	return $rv;
}

/**
 * Find the lowest common ancestor of two scratchpads, searching by name
 * @param unknown_type $src
 * @param unknown_type $dst
 */
function common_ancestor($src, $dst)
{
	$ancestor = rtrim(common_prefix($src, $dst), '.');
	if (strlen($ancestor) == 0) {
		return 'b';
	}
	return $ancestor;
}
/**
 * Gets the common prefix of two strings
 * @param string $str1
 * @param string $str2
 */
function common_prefix($str1, $str2)
{
	$length = 0;
	while (strncmp($str1, $str2, $length) == 0) {
		$length++;
	}
	return substr($str1, 0, $length-1);
}

function parse_line(&$mems, $parents, $line)
{
	$src = $line['source'];
	$dst = $line['destination'];
	$ancestor = common_ancestor($src, $dst);
	$size = $line['size'];
	// Add to direct ancestors of source
	$name = explode('.', $src);
	$pad =& scratchpad_by_name($mems, implode('.', $name));

	while ($pad['id'] != $ancestor) {
		$pad['data']['outside'] += $size;
		array_pop($name);
		if (count($name) == 0) {
			break;
		}
		$pad =& scratchpad_by_name($mems, implode('.', $name));
	}
	// Add to direct ancestors of destination
	$name = explode('.', $dst);
	$pad =& scratchpad_by_name($mems, implode('.', $name));

	while ($pad['id'] != $ancestor) {
		$pad['data']['outside'] += $size;
		array_pop($name);
		if (count($name) == 0) {
			break;
		}
		$pad =& scratchpad_by_name($mems, implode('.', $name));
	}
	$name = explode('.', $ancestor);
	$pad =& scratchpad_by_name($mems, $ancestor);
	while (count($name) > 0) {
		$pad['data']['inside'] += $size;
		array_pop($name);
		if (count($name) == 0) {
			break;
		}
		$pad =& scratchpad_by_name($mems, implode('.', $name));
	}
	// If the root has a unique ID (currently always the case), then a special
	// case is needed for when the common ancestor doesn't track back directly
	// to the root
	if ($pad['id'] != $mems['id']) {
		$mems['data']['inside'] += $size;
	}
}

try {
	// Open the database
	$conn = new PDO("sqlite:$dbname");
	$conn->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

	// BUILD THE HEIRARCHY HERE
	$result = $conn->query("SELECT id,parent FROM layout");
	$results = $result->fetchAll();
	$scratchpad_parents = array();
	foreach ($results as $r) {
		$id = (string)$r['id'];
		$scratchpad_parents[$id] = (string)$r['parent'];
	}

	$result = $conn->query(query_str());
	$scratchpads = array(
			'id' => array_search('.', $scratchpad_parents),
			'name' => name(0),
			'data' => array(
					'inside' => 0,
					'outside' => 0),
			'children' => array());

	// Make heirarchy
	add_descendants($scratchpads, $scratchpad_parents);

 	foreach ($result->fetchAll() as $r) {
 		parse_line($scratchpads, $scratchpad_parents, $r);
 	}
	// Calculate max movement outside a node
	$max = 0;
	foreach (array_keys($scratchpad_parents) as $key) {
		$sp = scratchpad_by_name($scratchpads, $key);
		$max = max($max, $sp['data']['outside']);
	}

	$scratchpads['max'] = $max;
 	echo json_encode($scratchpads);
} catch (PDOException $e) {
	echo 'ERROR: ' . $e->getMessage() . '</br>';
	echo $e->getTraceAsString();
	echo ("(db is $dbname)");
}

?>