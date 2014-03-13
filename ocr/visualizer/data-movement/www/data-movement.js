// BEGIN intro taken from JIT examples
var labelType, useGradients, nativeTextSupport, animate;
(function() {
	var ua = navigator.userAgent, iStuff = ua.match(/iPhone/i)
			|| ua.match(/iPad/i), typeOfCanvas = typeof HTMLCanvasElement, nativeCanvasSupport = (typeOfCanvas == 'object' || typeOfCanvas == 'function'), textSupport = nativeCanvasSupport
			&& (typeof document.createElement('canvas').getContext('2d').fillText == 'function');
	// I'm setting this based on the fact that ExCanvas provides text support
	// for IE
	// and that as of today iPhone/iPad current text support is lame
	labelType = (!nativeCanvasSupport || (textSupport && !iStuff)) ? 'Native'
			: 'HTML';
	nativeTextSupport = labelType == 'Native';
	useGradients = nativeCanvasSupport;
	animate = !(iStuff || !nativeCanvasSupport);
})();
// END intro taken from JIT examples

// Global constants
var nodeColor = "#eeb";
var nodeBorderColor = "#331";
var nodeSelectColor = "#e22";
var edgeColor = "black";
var edgeSelectColor = "#d11";
var maxWidth = 10;
var nodeWidth = 94, nodeHeight = 20;
var maxEdgeMovement = 0;

// Define a node type that allows easy inclusion of a border
$jit.ST.Plot.NodeTypes
		.implement({
			'stroke-rect': {
				'render': function(node, canvas) {
					var width = node.getData('width'), height = node
							.getData('height'), pos = this.getAlignedPos(
							node.pos.getc(true), width, height), posX = pos.x
							+ width / 2, posY = pos.y + height / 2;
					this.nodeHelper.rectangle.render('fill', {
						x: posX,
						y: posY
					}, width, height, canvas);
					this.nodeHelper.rectangle.render('stroke', {
						x: posX,
						y: posY
					}, width, height, canvas);
				}
			}
		});

var st;
/**
 * Called upon loading of html page
 */
function init() {
	// Empty data
	var json = loadEmptyDb();

	st = new $jit.ST(
			{
				// Element container
				injectInto: 'chart-container',
				// Enable panning
				Navigation: {
					enable: true,
					panning: true
				},
				Canvas: {
					overridable: true
				},
				// Node/edge styles
				Node: {
					overridable: true,
					// Set default size
					width: nodeWidth,
					height: nodeHeight,
					type: 'stroke-rect',
					color: nodeColor,
					CanvasStyles: {
						// Contents here are not overridable.
						strokeStyle: nodeBorderColor
					}
				},
				Edge: {
					overridable: true,
					color: edgeColor,
					type: 'quadratic:end'
				},
				// Tooltips
				Tips: {
					enable: true,
					onShow: function(tip, node) {
						var text = 'Inside: '
								+ (node.data.inside == 0 ? 'no data'
										: node.data.inside);
						text += '<br/>';
						text += 'Outside: '
								+ (node.data.outside == 0 ? 'no data'
										: node.data.outside);
						tip.innerHTML = text;
					}
				},

				onBeforeCompute: function(node) {
					console.log("loading " + node.name);
				},
				onAfterCompute: function() {
					console.log("done");
				},

				// Set up DOM label creation
				onCreateLabel: function(label, node) {
					label.id = node.id;
					// Set the text on the node label
					label.innerHTML = node.name;
					// Set the action for clicking a node
					label.onclick = function() {
						st.onClick(node.id);
					};
					// Style this label

					var style = label.style;
					style.width = node.getData('width') + 'px';
					style.height = (node.getData('height') - 4) + 'px';
					style.cursor = 'pointer';
					style.fontSize = '0.8em';
					style.paddingTop = '2px';
					style.textAlign = 'center';
				},
				// Style a node
				onBeforePlotNode: function(node) {
					if (node.selected) {
						node.data.$color = nodeSelectColor;
					} else {
						delete node.data.$color;
					}
					if (node.name == "Select a database" && node.id == "nodata") {
						node.data.$width = 70;
						node.data.$height = 36;
					}
				},
				// Style an edge
				onBeforePlotLine: function(edge) {
					var child = edge.nodeFrom.isDescendantOf(edge.nodeTo.id) ? edge.nodeFrom
							: edge.nodeTo;
					var lineWidth = child.data.outside / maxEdgeMovement
							* maxWidth;
					if (lineWidth < 0.5)
						lineWidth = 0.5;
					edge.data.$lineWidth = lineWidth;
					if (edge.nodeFrom.selected && edge.nodeTo.selected) {
						edge.data.$color = edgeSelectColor;
					} else {
						delete edge.data.$color;
					}
				}
			});

	setTreeData(json);
}

function setTreeData(jsonData, select) {
	st.loadJSON(jsonData);
	maxEdgeMovement = jsonData.max;
	st.compute();
	// If select is supplied, navigate to the specified node without animation.
	// Otherwise, start from the root node and allow animation.
	if (select == undefined) {
		st.onClick(st.root);
	} else {
		st.select(select);
	}
}

function loadEmptyDb() {
	return {
		id: "nodata",
		name: "Select a database",
		data: {
			inside: 0,
			outside: 0
		},
		children: {},
		max: 1
	};
}

function loadDb() {
	var db = document.getElementById('db_path').value;
	var status = $.ajax({
		type: "GET",
		cache: false,
		url: "file_verify.php",
		datatype: "json",
		data: {
			path: db
		},
		async: false
	}).status;

	if (status == 204) {
		// Display error: DB does not exist
	} else if (status == 200) {
		var json = getData();
		setTreeData(json);
		getEdts(db);
	}
}

function getData(edt) {
	var data = {};
	if (edt != undefined) {
		data.edt = edt;
	}
	var jsonData = $.ajax({
		url: "readdb.php",
		data: data,
		dataType: "json",
		async: false
	}).responseText;
	console.log(jsonData);
	return $.parseJSON(jsonData);
}

function getEdts(file) {
	var jsonData = $.ajax({
		url: "edtlist.php",
		dataType: "json",
		async: false
	}).responseText;
	// console.log(jsonData);
	var edtList = $.parseJSON(jsonData);

	select = document.forms["edt_select"].elements["edt_list"];
	// Reset dropdown
	select.options.length = 0;
	var opt = document.createElement("option");
	opt.text = "All EDTs";
	opt.value = "-all-";
	select.add(opt);
	// Add EDTs
	for ( var i in edtList) {
		var opt = document.createElement("option");
		opt.text = edtList[i];
		opt.value = edtList[i];
		select.add(opt);
	}
}

/*
 * Make a new query for a specific EDT
 */
function updateEdt() {
	var edt = document.forms["edt_select"].elements["edt_list"];
	var edtName = edt.options[edt.selectedIndex].value;

	var selection = st.clickedNode.id;
	var data = (edtName == '-all-') ? getData() : getData(edtName);

	setTreeData(data, selection);
}