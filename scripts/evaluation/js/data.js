var DEFAULT_SCENE = "cycles-principled"
function getCurrentScene() {
    let params = (new URL(document.location)).searchParams;
    let scene = params.get("scene")
    if (scene !== null) {
        return scene
    } else {
        return DEFAULT_SCENE
    }
}

var available_scenes = {};

var METHODS_TO_NAME = {
    "ref": "Reference",
    "cpu": "CPU",
    "gpu": "GPU",
};

var THUMBNAIL_SIZE = [128, 128];
var IMAGE_SIZE = [512, 512];

var IMAGE_EXT = ".png"

function makeImgURI(sceneName, method) {
    return `imgs/${method}-${sceneName}${IMAGE_EXT}`;
}

function getMethodFromURI(uri) {
    return uri.split("/").slice(-1)[0].split("-")[0]
}

function changeImageSize(item, is_thumbnail) {
    if (is_thumbnail) {
        item.style.width="10vw";
        item.style.minWidth=THUMBNAIL_SIZE[0] + "px";
    }
    else {
        item.width = IMAGE_SIZE[0];
        item.height = IMAGE_SIZE[1];
    }
}

function changeImageSizeStyle(style) {
    style.width = IMAGE_SIZE[0] + "px";
    style.height = IMAGE_SIZE[1] + "px";
}

function changeToScene(sceneName) {
    console.log("Loading " + sceneName);
    setupTopList(sceneName);
    for (let item of document.getElementsByClassName("thumbnail-image")) {
        let method = getMethodFromURI(item.src);
        item.src = makeImgURI(sceneName, method);
        changeImageSize(item, true);
    }

    for (let item of document.getElementsByClassName("method-image")) {
        let method = getMethodFromURI(item.src);
        item.src = makeImgURI(sceneName, method);
        changeImageSize(item, false);
    }

    for (let item of document.getElementsByClassName("img-comp-area")) {
        changeImageSizeStyle(item.style);
    }

    var sidebarList = document.getElementsByClassName("sidebar-list")[0];
    for (let item of sidebarList) {
        item.selected = (sceneName == item.value)
    }

    setupErrorTable(SCENES[sceneName])
    initComparisons();
}

function changeScene(event) {
    event.preventDefault();
    let selection = event.target[event.target.selectedIndex]

    const url = new URL(window.location);
    url.searchParams.set('scene', selection.value);
    window.history.pushState({}, '', url);
    changeToScene(selection.value);
}

function handleBack(event) {
    changeToScene(getCurrentScene())
}

function populateData() {
    let params = (new URL(document.location)).searchParams;

    sidebarList = document.getElementsByClassName("sidebar-list")[0];
    sidebarList.onchange = changeScene;

    for (const key of Object.keys(SCENES)) {
        var a = document.createElement("option");
        a.value = key
        a.textContent = key
        sidebarList.appendChild(a);
    }

    curScene = params.get("scene")
    if (curScene !== null) {
        changeToScene(curScene)
    } else {
        changeToScene(DEFAULT_SCENE)
    }

    window.addEventListener("popstate", handleBack, false);
}

/* Top List */
function createListEntry(sceneName, method) {
    var li = document.createElement("li");

    // Name
    var p = document.createElement("p");
    li.appendChild(p);

    p.classList.add("image-label");
    p.textContent = METHODS_TO_NAME[method];

    // Image content
    var img = document.createElement("img");
    li.appendChild(img);

    img.classList.add("thumbnail-image");
    img.src = makeImgURI(sceneName, method);
    changeImageSize(img, true);
    
    img.draggable = true;
    img.addEventListener("dragstart", function (ev) {
        ev.dataTransfer.setData("text", img.src);
    });

    return li;
}

function setupTopList(sceneName) {
    var info = SCENES[sceneName];
 
    var list = document.getElementsByClassName("image-list")[0];

    // Clear all children
    while (list.firstChild)
        list.firstChild.remove();

    // Create root
    var ul = document.createElement("ul");
    list.appendChild(ul);

    Object.keys(info).sort().forEach(key => {
        ul.appendChild(createListEntry(sceneName, key));
    });
}

/* Table */
function beautifulNumbers(num) {
    return num.toLocaleString(
        undefined,
        { minimumFractionDigits: 2, maximumFractionDigits: 2 }
    );
}

function createEntryString(a, b) {
    return a.toExponential(2) + " (" + beautifulNumbers(b) + "&#xD7;)";
}

function createEntry(entry, is_best) {
    var e = document.createElement("td");
    if (is_best) {
        e.className = "best";
        var s = document.createElement("em");
        s.innerHTML = entry;
        e.appendChild(s)
    } else {
        e.innerHTML = entry;
    }
    return e;
}

function createTableHeader(entry, className) {
    var e = document.createElement("th");
    if (className != null) {
        e.className = className;
        e.addEventListener("click", handleErrorPivotChange);
    }
    e.innerHTML = entry;
    return e;
}

function getCurrentErrorPivot() {
    var headers = document.getElementsByClassName("error-header");
    for (var i = 0; i < headers.length; ++i) {
        if (headers[i].classList.contains("active"))
            return i;
    }
    return 0;
}

function setupErrorTable(info) {
    var error_pivot = getCurrentErrorPivot();

    var table = document.getElementsByClassName("error-table")[0];

    // Clear all children
    while (table.firstChild)
        table.firstChild.remove();

    // Create header
    var thead = document.createElement("thead");
    table.appendChild(thead);

    var headrow = document.createElement("tr");
    thead.appendChild(headrow);
    headrow.appendChild(createTableHeader("Error", null));
    Object.keys(info).filter(key => key != "ref").forEach(key => {
        headrow.appendChild(createTableHeader(METHODS_TO_NAME[key], "error-header"));
    });

    headrow.children[error_pivot + 1].classList.add("active");

    // Create body
    var tbody = document.createElement("tbody");
    table.appendChild(tbody);

    function setup_row(name, err_data) {
        var r_err = []
        for (var i = 0; i < err_data.length; ++i) {
            r_err.push(err_data[error_pivot] / err_data[i])
        }

        best = err_data.indexOf(Math.min(...err_data))

        var row = document.createElement("tr");
        tbody.appendChild(row)
    
        row.appendChild(createEntry(name));
        for (var i = 0; i < err_data.length; ++i) {
            row.appendChild(createEntry(createEntryString(err_data[i], r_err[i]), best == i));
        }
    }

    setup_row("MSE", Object.keys(info).filter(key => key != "ref").map(key => info[key].error.mse))
    setup_row("RelMSE", Object.keys(info).filter(key => key != "ref").map(key => info[key].error.relmse))
}

function updateTable() {
    setupErrorTable(SCENES[getCurrentScene()])
}

function handleErrorPivotChange(event) {
    event.preventDefault();
    var headers = document.getElementsByClassName("error-header");
    for (var i = 0; i < headers.length; i++) {
        headers[i].classList.remove("active");
    }
    event.target.classList.add("active");

    updateTable();
}
